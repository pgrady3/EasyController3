#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"

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

const int THROTTLE_LOW = 600;
const int THROTTLE_HIGH = 2650;

uint8_t hallToMotor[8] = {255, 2, 0, 1, 4, 3, 5, 255};

// void on_pwm_wrap() {
//     pwm_clear_irq(0);

//     gpio_put(FLAG_PIN, 1);
//     uint16_t result = adc_read();
//     gpio_put(FLAG_PIN, 0);
// }

void writePhases(uint ah, uint bh, uint ch, uint al, uint bl, uint cl)
{
    pwm_set_both_levels(A_PWM_SLICE, ah, 255 - al);
    pwm_set_both_levels(B_PWM_SLICE, bh, 255 - bl);
    pwm_set_both_levels(C_PWM_SLICE, ch, 255 - cl);
}

void writePWM(uint motorState, uint dutyCycle)
{
  if(dutyCycle == 0)                          // If zero throttle, turn all off
    motorState = 255;

  if(motorState == 0)                         // LOW A, HIGH B
      writePhases(0, dutyCycle, 0, 255, 0, 0);
  else if(motorState == 1)                    // LOW A, HIGH C
      writePhases(0, 0, dutyCycle, 255, 0, 0);
  else if(motorState == 2)                    // LOW B, HIGH C
      writePhases(0, 0, dutyCycle, 0, 255, 0);
  else if(motorState == 3)                    // LOW B, HIGH A
      writePhases(dutyCycle, 0, 0, 0, 255, 0);
  else if(motorState == 4)                    // LOW C, HIGH A
      writePhases(dutyCycle, 0, 0, 0, 0, 255);
  else if(motorState == 5)                    // LOW C, HIGH B
      writePhases(0, dutyCycle, 0, 0, 0, 255);
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

    adc_init();
    adc_gpio_init(ISENSE_PIN);  // Make sure GPIO is high-impedance, no pullups etc
    adc_gpio_init(VSENSE_PIN);  // Make sure GPIO is high-impedance, no pullups etc
    adc_gpio_init(THROTTLE_PIN);  // Make sure GPIO is high-impedance, no pullups etc

    gpio_set_function(AH_PIN, GPIO_FUNC_PWM);
    gpio_set_function(AL_PIN, GPIO_FUNC_PWM);
    gpio_set_function(BH_PIN, GPIO_FUNC_PWM);
    gpio_set_function(BL_PIN, GPIO_FUNC_PWM);
    gpio_set_function(CH_PIN, GPIO_FUNC_PWM);
    gpio_set_function(CL_PIN, GPIO_FUNC_PWM);

    // pwm_clear_irq(slice_num);
    // pwm_set_irq_enabled(slice_num, true);
    // irq_set_exclusive_handler(PWM_IRQ_WRAP, on_pwm_wrap);
    // irq_set_enabled(PWM_IRQ_WRAP, true);

    // // uint slice_num = pwm_gpio_to_slice_num(AH_PIN);
    // for(uint slice_num=0; slice_num < 3; slice_num++) {
    //     float pwm_divider = (float)(clock_get_hz(clk_sys)) / (F_PWM * 255 * 2);
    //     pwm_set_wrap(slice_num, 255 - 1);
    //     pwm_set_phase_correct(slice_num, true);
    //     pwm_set_clkdiv(slice_num, pwm_divider);
    //     pwm_set_output_polarity(slice_num, false, true);
    //     // pwm_set_chan_level(slice_num, PWM_CHAN_A, 100);
    //     // pwm_set_chan_level(slice_num, PWM_CHAN_B, 100);
    //     pwm_set_enabled(slice_num, true);
    // }

    float pwm_divider = (float)(clock_get_hz(clk_sys)) / (F_PWM * 255 * 2);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, pwm_divider);
    pwm_config_set_wrap(&config, 255 - 1);
    pwm_config_set_phase_correct(&config, true);
    pwm_config_set_output_polarity(&config, false, true);

    writePhases(0, 0, 0, 0, 0, 0);

    pwm_init(A_PWM_SLICE, &config, true);
    pwm_init(B_PWM_SLICE, &config, true);
    pwm_init(C_PWM_SLICE, &config, true);
}

uint get_halls() {
    uint hallCounts[] = {0, 0, 0};
    for(uint i = 0; i < HALL_OVERSAMPLE; i++) // Read all the hall pins repeatedly, tally results 
    {
        hallCounts[0] += gpio_get(HALL_1_PIN);
        hallCounts[1] += gpio_get(HALL_2_PIN);
        hallCounts[2] += gpio_get(HALL_3_PIN);
    }

    uint hall = 0;

    if (hallCounts[0] >= HALL_OVERSAMPLE / 2)     // If votes >= threshold, call that a 1
        hall |= (1<<0);                             // Store a 1 in the 0th bit
    if (hallCounts[1] >= HALL_OVERSAMPLE / 2)
        hall |= (1<<1);                             // Store a 1 in the 1st bit
    if (hallCounts[2] >= HALL_OVERSAMPLE / 2)
        hall |= (1<<2);                             // Store a 1 in the 2nd bit

    return hall & 0x7;                            // Just to make sure we didn't do anything stupid, set the maximum output value to 7
}

uint8_t read_throttle()
{
    adc_select_input(2);    // Select ADC input to throttle
    int adc = adc_read();
    adc = (adc - THROTTLE_LOW) * 256;
    adc = adc / (THROTTLE_HIGH - THROTTLE_LOW);

    if (adc > 255) // Bound the output between 0 and 255
        return 255;

    if (adc < 0)
        return 0;

    return adc;
}

void identifyHalls()
{
    for(uint i = 0; i < 6; i++)
    {
        uint8_t nextState = (i + 1) % 6;        // Calculate what the next state should be. This is for switching into half-states
        printf("Going to %d\n", i);
        for(uint j = 0; j < 500; j++)       // For a while, repeatedly switch between states
        {
            sleep_ms(1);
            writePWM(i, 25);
            sleep_ms(1);
            writePWM(nextState, 25);
        }
        hallToMotor[get_halls()] = (i + 2) % 6;
    }

    writePWM(0, 0);                           // Turn phases off

    for(uint8_t i = 0; i < 8; i++)            // Print out the array
        printf("%d, ", hallToMotor[i]);
    printf("\n");
}

int main() {
    init_hardware();
    sleep_ms(2000);
    // identifyHalls();

    uint counter = 0;
    while (true) {
        uint throttle = read_throttle();  // readThrottle() is slow. So do the more important things 200 times more often
        gpio_put(LED_PIN, !gpio_get(LED_PIN));
        gpio_put(FLAG_PIN, !gpio_get(FLAG_PIN));
        for(uint i = 0; i < 200; i++)
        {  
            uint hall = get_halls();              // Read from the hall sensors
            uint motorState = hallToMotor[hall]; // Convert from hall values (from 1 to 6) to motor state values (from 0 to 5) in the correct order. This line is magic
            writePWM(motorState, throttle);         // Actually command the transistors to switch into specified sequence and PWM value
        }


        // uint throttle = read_throttle();
        // printf("Throttle: %03d, halls %d\n", read_throttle(), get_halls());

        // sleep_ms(200);
        // // pwm_set_chan_level(slice_num, PWM_CHAN_B, thresh);

        // counter++;
        // writePWM((counter / 5) % 6, 18);
    }
    return 0;
}
