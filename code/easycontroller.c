#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"

// Begin user config section ---------------------------

const bool IDENTIFY_HALLS_ON_BOOT = false;
const bool IDENTIFY_HALLS_REVERSE = false;

// uint8_t hallToMotor[8] = {255, 255, 255, 255, 255, 255, 255, 255};  // Default
uint8_t hallToMotor[8] = {255, 0, 2, 1, 4, 5, 3, 255};  // Bike motor
// uint8_t hallToMotor[8] = {255, 1, 5, 0, 3, 2, 4, 255};  // Prop motor
// uint8_t hallToMotor[8] = {255, 2, 0, 1, 4, 3, 5, 255};  // state +1
// uint8_t hallToMotor[8] = {255, 0, 4, 5, 2, 1, 3, 255};  // state -1
const int THROTTLE_LOW = 600;
const int THROTTLE_HIGH = 2650;

const bool CURRENT_CONTROL = true;
const int PHASE_MAX_CURRENT_MA = 50000;
const int BATTERY_MAX_CURRENT_MA = 20000;
const int CURRENT_LOOP_DIV = 200;

// End user config section -----------------------------

#define MEMORY_LEN 10000
int memory_current[MEMORY_LEN];
int memory_current_target[MEMORY_LEN];
int memory_throttle[MEMORY_LEN];
int memory_hall[MEMORY_LEN];
int memory_voltage[MEMORY_LEN];
int memory_duty[MEMORY_LEN];
int memory_pos = 0;
bool memory_writing = false;

const uint LED_PIN = 25;
const uint AH_PIN = 16;
const uint AL_PIN = 17;
const uint BH_PIN = 18;
const uint BL_PIN = 19;
const uint CH_PIN = 20;
const uint CL_PIN = 21;
const uint HALL_1_PIN = 13;
const uint HALL_2_PIN = 14;
const uint HALL_3_PIN = 15;
const uint ISENSE_PIN = 26;
const uint VSENSE_PIN = 27;
const uint THROTTLE_PIN = 28;

const uint A_PWM_SLICE = 0;
const uint B_PWM_SLICE = 1;
const uint C_PWM_SLICE = 2;

const uint F_PWM = 16000;
const uint FLAG_PIN = 2;
const uint HALL_OVERSAMPLE = 8;

const int DUTY_CYCLE_MAX = 65535;
const int CURRENT_SCALING = 3.3 / 0.001 / 20 / 4096 * 1000;
const int VOLTAGE_SCALING = 3.3 / 4096 * (47 + 2.2) / 2.2 * 1000;
const int ADC_BIAS_OVERSAMPLE = 1000;

const int HALL_IDENTIFY_DUTY_CYCLE = 25;

int adc_isense = 0;
int adc_vsense = 0;
int adc_throttle = 0;

int adc_bias = 0;
int duty_cycle = 0;
int voltage_mv = 0;
int current_ma = 0;
int current_target_ma = 0;
int hall = 0;
uint motorState = 0;
int fifo_level = 0;
uint64_t ticks_since_init = 0;

uint get_halls();
void writePWM(uint motorState, uint duty, bool synchronous);
uint8_t read_throttle();


void on_adc_fifo() {
    uint32_t flags = save_and_disable_interrupts(); // Disable interrupts for the time-critical reading ADC section

    adc_run(false);
    gpio_put(FLAG_PIN, 1);

    fifo_level = adc_fifo_get_level();    
    adc_isense = adc_fifo_get();
    adc_vsense = adc_fifo_get();
    adc_throttle = adc_fifo_get();

    restore_interrupts(flags);

    if(fifo_level != 3) {
        // The RP2040 is a shitty microcontroller. The ADC is unpredictable, and 1% of times
        // will return more or less than the 3 samples it should. If we don't get the expected number, abort
        return;
    }

    hall = get_halls();    // Takes ~200 nanoseconds
    motorState = hallToMotor[hall];

    int throttle = ((adc_throttle - THROTTLE_LOW) * 256) / (THROTTLE_HIGH - THROTTLE_LOW);
    throttle = MAX(0, MIN(255, throttle));

    current_ma = (adc_isense - adc_bias) * CURRENT_SCALING;
    current_target_ma = throttle * PHASE_MAX_CURRENT_MA / 256;
    int current_target_batt_ma = BATTERY_MAX_CURRENT_MA * DUTY_CYCLE_MAX / duty_cycle;
    current_target_ma = MIN(current_target_batt_ma, current_target_ma);
    voltage_mv = adc_vsense * VOLTAGE_SCALING;

    if (throttle == 0)
    {
        duty_cycle = 0;
        ticks_since_init = 0;
    }
    ticks_since_init++;

    if(CURRENT_CONTROL) {
        duty_cycle += (current_target_ma - current_ma) / CURRENT_LOOP_DIV;
        duty_cycle = MAX(0, MIN(DUTY_CYCLE_MAX, duty_cycle));

        bool do_synchronous = ticks_since_init > 16000;    // Only enable synchronous switching some time after beginning control loop. This allows control loop to stabilize
        writePWM(motorState, (uint)(duty_cycle / 256), do_synchronous);
    }
    else {
        duty_cycle = throttle * 256;
        writePWM(motorState, (uint)(duty_cycle / 256), true);
    }

    if(memory_writing)
    {
        memory_current[memory_pos] = current_ma;
        memory_current_target[memory_pos] = current_target_ma;
        memory_voltage[memory_pos] = voltage_mv;
        memory_throttle[memory_pos] = throttle;
        memory_hall[memory_pos] = hall;
        memory_voltage[memory_pos] = voltage_mv;
        memory_duty[memory_pos] = duty_cycle;
        memory_pos++;

        if(memory_pos >= MEMORY_LEN)
            memory_writing = false;
    }

    gpio_put(FLAG_PIN, 0);
}

void on_pwm_wrap() {
    gpio_put(FLAG_PIN, 1);
    adc_select_input(0);
    adc_run(true);
    pwm_clear_irq(A_PWM_SLICE);
    while(!adc_fifo_is_empty())
        adc_fifo_get();

    gpio_put(FLAG_PIN, 0);
}

void writePhases(uint ah, uint bh, uint ch, uint al, uint bl, uint cl)
{
    pwm_set_both_levels(A_PWM_SLICE, ah, 255 - al);
    pwm_set_both_levels(B_PWM_SLICE, bh, 255 - bl);
    pwm_set_both_levels(C_PWM_SLICE, ch, 255 - cl);
}

void writePWM(uint motorState, uint duty, bool synchronous)
{
    if(duty == 0 || duty > 255)                          // If zero throttle, turn all off
        motorState = 255;

    uint complement = 0;
    if(synchronous)
        complement = 250 - duty;

    if(motorState == 0)                         // LOW A, HIGH B
        writePhases(0, duty, 0, 255, complement, 0);
    else if(motorState == 1)                    // LOW A, HIGH C
        writePhases(0, 0, duty, 255, 0, complement);
    else if(motorState == 2)                    // LOW B, HIGH C
        writePhases(0, 0, duty, 0, 255, complement);
    else if(motorState == 3)                    // LOW B, HIGH A
        writePhases(duty, 0, 0, complement, 255, 0);
    else if(motorState == 4)                    // LOW C, HIGH A
        writePhases(duty, 0, 0, complement, 0, 255);
    else if(motorState == 5)                    // LOW C, HIGH B
        writePhases(0, duty, 0, 0, complement, 255);
    else                                        // All off
        writePhases(0, 0, 0, 0, 0, 0);
}

void init_hardware() {
    // Initialize all peripherals

    stdio_init_all();
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_init(FLAG_PIN);
    gpio_set_dir(FLAG_PIN, GPIO_OUT);

    gpio_init(HALL_1_PIN);
    gpio_set_dir(HALL_1_PIN, GPIO_IN);
    gpio_init(HALL_2_PIN);
    gpio_set_dir(HALL_2_PIN, GPIO_IN);
    gpio_init(HALL_3_PIN);
    gpio_set_dir(HALL_3_PIN, GPIO_IN);

    gpio_set_function(AH_PIN, GPIO_FUNC_PWM);
    gpio_set_function(AL_PIN, GPIO_FUNC_PWM);
    gpio_set_function(BH_PIN, GPIO_FUNC_PWM);
    gpio_set_function(BL_PIN, GPIO_FUNC_PWM);
    gpio_set_function(CH_PIN, GPIO_FUNC_PWM);
    gpio_set_function(CL_PIN, GPIO_FUNC_PWM);

    adc_init();
    adc_gpio_init(ISENSE_PIN);  // Make sure GPIO is high-impedance, no pullups etc
    adc_gpio_init(VSENSE_PIN);  // Make sure GPIO is high-impedance, no pullups etc
    adc_gpio_init(THROTTLE_PIN);  // Make sure GPIO is high-impedance, no pullups etc

    sleep_ms(100);
    for(uint i = 0; i < ADC_BIAS_OVERSAMPLE; i++)
    {
        adc_select_input(0);
        adc_bias += adc_read();
    }
    adc_bias /= ADC_BIAS_OVERSAMPLE;

    adc_set_round_robin(0b111);
    adc_fifo_setup(true, false, 3, false, false);
    irq_set_exclusive_handler(ADC_IRQ_FIFO, on_adc_fifo);
    irq_set_priority(ADC_IRQ_FIFO, 0);
    adc_irq_set_enabled(true);
    irq_set_enabled(ADC_IRQ_FIFO, true);

    pwm_clear_irq(A_PWM_SLICE);
    irq_set_exclusive_handler(PWM_IRQ_WRAP, on_pwm_wrap);
    irq_set_priority(PWM_IRQ_WRAP, 0);
    irq_set_enabled(PWM_IRQ_WRAP, true);

    float pwm_divider = (float)(clock_get_hz(clk_sys)) / (F_PWM * 255 * 2);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, pwm_divider);
    pwm_config_set_wrap(&config, 255 - 1);
    pwm_config_set_phase_correct(&config, true);
    pwm_config_set_output_polarity(&config, false, true);

    writePhases(0, 0, 0, 0, 0, 0);

    pwm_init(A_PWM_SLICE, &config, false);
    pwm_init(B_PWM_SLICE, &config, false);
    pwm_init(C_PWM_SLICE, &config, false);

    pwm_set_mask_enabled(0x07);
}

uint get_halls() {
    uint hallCounts[] = {0, 0, 0};
    for(uint i = 0; i < HALL_OVERSAMPLE; i++) // Read all the hall pins repeatedly, tally results 
    {
        hallCounts[0] += gpio_get(HALL_1_PIN);
        hallCounts[1] += gpio_get(HALL_2_PIN);
        hallCounts[2] += gpio_get(HALL_3_PIN);
    }

    uint hall_raw = 0;

    if (hallCounts[0] > HALL_OVERSAMPLE / 2)     // If votes >= threshold, call that a 1
        hall_raw |= (1<<0);                             // Store a 1 in the 0th bit
    if (hallCounts[1] > HALL_OVERSAMPLE / 2)
        hall_raw |= (1<<1);                             // Store a 1 in the 1st bit
    if (hallCounts[2] > HALL_OVERSAMPLE / 2)
        hall_raw |= (1<<2);                             // Store a 1 in the 2nd bit

    return hall_raw & 0x7;                            // Just to make sure we didn't do anything stupid, set the maximum output value to 7
}

void identify_halls()
{
    sleep_ms(3000);
    for(uint i = 0; i < 6; i++)
    {
        uint8_t nextState = (i + 1) % 6;        // Calculate what the next state should be. This is for switching into half-states
        printf("Going to %d\n", i);
        for(uint j = 0; j < 1000; j++)       // For a while, repeatedly switch between states
        {
            sleep_us(500);
            writePWM(i, HALL_IDENTIFY_DUTY_CYCLE, false);
            sleep_us(500);
            writePWM(nextState, HALL_IDENTIFY_DUTY_CYCLE, false);
        }
        if(IDENTIFY_HALLS_REVERSE)
            hallToMotor[get_halls()] = (i + 5) % 6;
        else
            hallToMotor[get_halls()] = (i + 2) % 6;

        // printf("hall %d step %d\n", get_halls(), i);
    }

    writePWM(0, 0, false);                           // Turn phases off

    printf("hallToMotor array:\n");
    for(uint8_t i = 0; i < 8; i++)            // Print out the array
        printf("%d, ", hallToMotor[i]);
    printf("\nIf any values are 255 except the first and last, the auto-identify was not successful.\n");
}

int main() {
    init_hardware();

    if(IDENTIFY_HALLS_ON_BOOT)
        identify_halls();

    // int s = 0;
    // while(true)
    // {
    //     writePWM(s % 6, 25, false);
    //     sleep_ms(100);
    //     s++;
    // }

    sleep_ms(1000);

    pwm_set_irq_enabled(A_PWM_SLICE, true); // Enables interrupts, starting commutation

    uint ticks = 0;
    while (true) {
        // if(ticks == 300)
        //     adc_throttle = 1500;
        // // if(ticks == 800)
        // //     adc_throttle = 3000;
        // if(ticks == 1000)
        //     adc_throttle = 500;
        // if(memory_pos == 0 && ticks == 290)
        //     memory_writing = true;

        // if(memory_pos == MEMORY_LEN && !memory_writing) {
        //     adc_throttle = 500;
        //     for(int i=0; i<MEMORY_LEN; i++) {
        //         printf("%6d, %6d, %6d, %6d, %6d, %6d\n", memory_current[i], memory_current_target[i], memory_duty[i], memory_voltage[i], memory_hall[i], memory_throttle[i]);
        //         sleep_ms(1);
        //     }
        //     memory_pos += 100;
        // }
        
        printf("%6d, %6d, %6d, %6d, %2d, %2d, %1d\n", current_ma, current_target_ma, duty_cycle, voltage_mv, hall, motorState, fifo_level);
        gpio_put(LED_PIN, !gpio_get(LED_PIN));
        sleep_ms(10);
        ticks++;
    }

    return 0;
}
