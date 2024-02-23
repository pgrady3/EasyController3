#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "hardware/uart.h"

// Begin user config section ---------------------------

const bool IDENTIFY_HALLS_ON_BOOT = true;   // If true, controller will initialize the hall table by slowly spinning the motor
const bool IDENTIFY_HALLS_REVERSE = false;  // If true, will initialize the hall table to spin the motor backwards

uint8_t hallToMotor[8] = {255, 255, 255, 255, 255, 255, 255, 255};  // Default hall table. Overwrite this with the output of the hall auto-identification 
// uint8_t hallToMotor[8] = {255, 2, 0, 1, 4, 3, 5, 255};  // Example hall table

const int THROTTLE_LOW = 600;               // ADC value corresponding to minimum throttle, 0-4095
const int THROTTLE_HIGH = 2650;             // ADC value corresponding to maximum throttle, 0-4095

const bool CURRENT_CONTROL = true;          // Use current control or duty cycle control
const int PHASE_MAX_CURRENT_MA = 6000;      // If using current control, the maximum phase current allowed
const int BATTERY_MAX_CURRENT_MA = 3000;    // If using current control, the maximum battery current allowed
const int CURRENT_CONTROL_LOOP_GAIN = 200;  // Adjusts the speed of the current control loop

// End user config section -----------------------------

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

const uint F_PWM = 16000;   // Desired PWM frequency
const uint FLAG_PIN = 2;
const uint HALL_OVERSAMPLE = 8;

const int DUTY_CYCLE_MAX = 65535;
const int CURRENT_SCALING = 3.3 / 0.0005 / 20 / 4096 * 1000;
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
    // This interrupt is where the magic happens. This is fired once the ADC conversions have finished (roughly 6us for 3 conversions)
    // This reads the hall sensors, determines the motor state to switch to, and reads the current sensors and throttle to
    // determine the desired duty cycle. This takes ~7us to complete.

    uint32_t flags = save_and_disable_interrupts(); // Disable interrupts for the time-critical reading ADC section. USB interrupts may interfere

    adc_run(false);             // Stop the ADC from free running
    gpio_put(FLAG_PIN, 1);      // For debugging, toggle the flag pin

    fifo_level = adc_fifo_get_level();
    adc_isense = adc_fifo_get();    // Read the ADC values into the registers
    adc_vsense = adc_fifo_get();
    adc_throttle = adc_fifo_get();

    restore_interrupts(flags);      // Re-enable interrupts

    if(fifo_level != 3) {
        // The RP2040 is a shitty microcontroller. The ADC is unpredictable, and 1% of times
        // will return more or less than the 3 samples it should. If we don't get the expected number, abort
        return;
    }

    hall = get_halls();                 // Read the hall sensors
    motorState = hallToMotor[hall];     // Convert the current hall reading to the desired motor state

    int throttle = ((adc_throttle - THROTTLE_LOW) * 256) / (THROTTLE_HIGH - THROTTLE_LOW);  // Scale the throttle value read from the ADC
    throttle = MAX(0, MIN(255, throttle));      // Clamp to 0-255

    current_ma = (adc_isense - adc_bias) * CURRENT_SCALING;     // Since the current sensor is bidirectional, subtract the zero-current value and scale
    voltage_mv = adc_vsense * VOLTAGE_SCALING;  // Calculate the bus voltage

    if(CURRENT_CONTROL) {
        int user_current_target_ma = throttle * PHASE_MAX_CURRENT_MA / 256;  // Calculate the user-demanded phase current
        int battery_current_limit_ma = BATTERY_MAX_CURRENT_MA * DUTY_CYCLE_MAX / duty_cycle;  // Calculate the maximum phase current allowed while respecting the battery current limit
        current_target_ma = MIN(user_current_target_ma, battery_current_limit_ma);

        if (throttle == 0)
        {
            duty_cycle = 0;     // If zero throttle, ignore the current control loop and turn all transistors off
            ticks_since_init = 0;   // Reset the timer since the transistors were turned on
        }
        else
            ticks_since_init++;

        duty_cycle += (current_target_ma - current_ma) / CURRENT_CONTROL_LOOP_GAIN;  // Perform a simple integral controller to adjust the duty cycle
        duty_cycle = MAX(0, MIN(DUTY_CYCLE_MAX, duty_cycle));   // Clamp the duty cycle

        bool do_synchronous = ticks_since_init > 16000;    // Only enable synchronous switching some time after beginning control loop. This allows control loop to stabilize
        writePWM(motorState, (uint)(duty_cycle / 256), do_synchronous);
    }
    else {
        duty_cycle = throttle * 256;    // Set duty cycle based directly on throttle
        bool do_synchronous = true;     // Note, if doing synchronous duty-cycle control, the motor will regen if the throttle decreases. This may regen VERY HARD

        writePWM(motorState, (uint)(duty_cycle / 256), do_synchronous);
    }

    gpio_put(FLAG_PIN, 0);
}

void on_pwm_wrap() {
    // This interrupt is triggered when the A_PWM slice reaches 0 (the middle of the PWM cycle)
    // This allows us to start ADC conversions while the high-side FETs are on, which is required
    // to read current, based on where the current sensor is placed in the schematic.
    // Takes ~1.3 microseconds

    gpio_put(FLAG_PIN, 1);      // Toggle the flag pin high for debugging
    adc_select_input(0);        // Force the ADC to start with input 0
    adc_run(true);              // Start the ADC
    pwm_clear_irq(A_PWM_SLICE); // Clear this interrupt flag
    while(!adc_fifo_is_empty()) // Clear out the ADC fifo, in case it still has samples in it
        adc_fifo_get();

    gpio_put(FLAG_PIN, 0);
}

void writePhases(uint ah, uint bh, uint ch, uint al, uint bl, uint cl)
{
    // Set the timer registers for each PWM slice. The lowside values are inverted,
    // since the PWM slices were already configured to invert the lowside pin.
    // ah: desired high-side duty cycle, range of 0-255
    // al: desired low-side duty cycle, range of 0-255

    pwm_set_both_levels(A_PWM_SLICE, ah, 255 - al);
    pwm_set_both_levels(B_PWM_SLICE, bh, 255 - bl);
    pwm_set_both_levels(C_PWM_SLICE, ch, 255 - cl);
}

void writePWM(uint motorState, uint duty, bool synchronous)
{
    // Switch the transistors given a desired electrical state and duty cycle
    // motorState: desired electrical position, range of 0-5
    // duty: desired duty cycle, range of 0-255
    // synchronous: perfom synchronous (low-side and high-side alternating) or non-synchronous switching (high-side only) 

    if(duty == 0 || duty > 255)     // If zero throttle, turn both low-sides and high-sides off
        motorState = 255;

    // At near 100% duty cycles, the gate driver bootstrap capacitor may become discharged as the high-side gate is repeatedly driven
    // high and low without time for the phase voltage to fall to zero and charge the bootstrap capacitor. Thus, if duty cycle is near
    // 100%, clamp it to 100%.
    if(duty > 245)
        duty = 255;

    uint complement = 0;
    if(synchronous)
    {
        complement = MAX(0, 248 - (int)duty);    // Provide switching deadtime by having duty + complement < 255
    }

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
    else                                        // All transistors off
        writePhases(0, 0, 0, 0, 0, 0);
}

void init_hardware() {
    // Initialize all peripherals

    stdio_init_all();

    gpio_init(LED_PIN);     // Set LED and FLAG pin as outputs
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_init(FLAG_PIN);
    gpio_set_dir(FLAG_PIN, GPIO_OUT);

    gpio_init(HALL_1_PIN);  // Set up hall sensor pins
    gpio_set_dir(HALL_1_PIN, GPIO_IN);
    gpio_init(HALL_2_PIN);
    gpio_set_dir(HALL_2_PIN, GPIO_IN);
    gpio_init(HALL_3_PIN);
    gpio_set_dir(HALL_3_PIN, GPIO_IN);

    gpio_set_function(AH_PIN, GPIO_FUNC_PWM);   // Set gate control pins as output
    gpio_set_function(AL_PIN, GPIO_FUNC_PWM);
    gpio_set_function(BH_PIN, GPIO_FUNC_PWM);
    gpio_set_function(BL_PIN, GPIO_FUNC_PWM);
    gpio_set_function(CH_PIN, GPIO_FUNC_PWM);
    gpio_set_function(CL_PIN, GPIO_FUNC_PWM);

    adc_init();
    adc_gpio_init(ISENSE_PIN);  // Set up ADC pins
    adc_gpio_init(VSENSE_PIN);
    adc_gpio_init(THROTTLE_PIN);

    sleep_ms(100);
    for(uint i = 0; i < ADC_BIAS_OVERSAMPLE; i++)   // Find the zero-current ADC reading. Reads the ADC multiple times and takes the average
    {
        adc_select_input(0);
        adc_bias += adc_read();
    }
    adc_bias /= ADC_BIAS_OVERSAMPLE;

    adc_set_round_robin(0b111);     // Set ADC to read our three ADC pins one after the other (round robin)
    adc_fifo_setup(true, false, 3, false, false);   // ADC writes into a FIFO buffer, and an interrupt is fired once FIFO reaches 3 samples
    irq_set_exclusive_handler(ADC_IRQ_FIFO, on_adc_fifo);   // Sets ADC interrupt
    irq_set_priority(ADC_IRQ_FIFO, 0);
    adc_irq_set_enabled(true);
    irq_set_enabled(ADC_IRQ_FIFO, true);

    pwm_clear_irq(A_PWM_SLICE);     // Clear interrupt flag, just in case
    irq_set_exclusive_handler(PWM_IRQ_WRAP, on_pwm_wrap);   // Set interrupt to fire when PWM counter wraps
    irq_set_priority(PWM_IRQ_WRAP, 0);
    irq_set_enabled(PWM_IRQ_WRAP, true);

    float pwm_divider = (float)(clock_get_hz(clk_sys)) / (F_PWM * 255 * 2);     // Calculate the desired PWM divisor
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, pwm_divider);
    pwm_config_set_wrap(&config, 255 - 1);      // Set the PWM to wrap at 254. This allows a PWM value of 255 to equal 100% duty cycle
    pwm_config_set_phase_correct(&config, true);    // Set phase correct (counts up then down). This is allows firing the interrupt in the middle of the PWM cycle
    pwm_config_set_output_polarity(&config, false, true);   // Invert the lowside PWM such that 0 corresponds to lowside transistors on

    writePhases(0, 0, 0, 0, 0, 0);  // Initialize all the PWMs to be off

    pwm_init(A_PWM_SLICE, &config, false);
    pwm_init(B_PWM_SLICE, &config, false);
    pwm_init(C_PWM_SLICE, &config, false);

    pwm_set_mask_enabled(0x07); // Enable our three PWM timers
}

uint get_halls() {
    // Read the hall sensors with oversampling. Hall sensor readings can be noisy, which produces commutation "glitches".
    // This reads the hall sensors multiple times, then takes the majority reading to reduce noise. Takes ~200 nanoseconds

    uint hallCounts[] = {0, 0, 0};
    for(uint i = 0; i < HALL_OVERSAMPLE; i++) // Read all the hall pins repeatedly, count votes
    {
        hallCounts[0] += gpio_get(HALL_1_PIN);
        hallCounts[1] += gpio_get(HALL_2_PIN);
        hallCounts[2] += gpio_get(HALL_3_PIN);
    }

    uint hall_raw = 0;
    for(uint i = 0; i < 3; i++)
        if (hallCounts[i] > HALL_OVERSAMPLE / 2)    // If more than half the readings are positive,
            hall_raw |= 1<<i;                       // set the i-th bit high

    return hall_raw;    // Range is 0-7. However, 0 and 7 are invalid hall states
}

void identify_halls()
{
    // This is the magic function which sets the hallToMotor commutation table. This allows
    // you to plug in the motor and hall wires in any order, and the controller figures out the order.
    // This works by PWM-ing to all of the "half states", such as 0.5, by switching rapidly between state 0 and 1.
    // This commutates to all half-states and reads the corresponding hall value. Then, since electrical position
    // should lead rotor position by 90 degrees, for this hall state, save a motor state 1.5 steps ahead.

    sleep_ms(2000);
    for(uint i = 0; i < 6; i++)
    {
        for(uint j = 0; j < 1000; j++)       // Commutate to a half-state long enough to allow the rotor to stop moving
        {
            sleep_us(500);
            writePWM(i, HALL_IDENTIFY_DUTY_CYCLE, false);
            sleep_us(500);
            writePWM((i + 1) % 6, HALL_IDENTIFY_DUTY_CYCLE, false);     // PWM to the next half-state
        }

        if(IDENTIFY_HALLS_REVERSE)
            hallToMotor[get_halls()] = (i + 5) % 6;     // If the motor should spin backwards, save a motor state of -1.5 steps
        else
            hallToMotor[get_halls()] = (i + 2) % 6;     // If the motor should spin forwards, save a motor state of +1.5 steps
    }

    writePWM(0, 0, false);      // Turn phases off

    printf("hallToMotor array:\n");     // Print out the array
    for(uint8_t i = 0; i < 8; i++)
        printf("%d, ", hallToMotor[i]);
    printf("\nIf any values are 255 except the first and last, auto-identify failed. Otherwise, save this table in code.\n");
}

void commutate_open_loop()
{
    // A useful function to debug electrical problems with the board.
    // This slowly advances the motor commutation without reading hall sensors. The motor should slowly spin
    int state = 0;
    while(true)
    {
        writePWM(state % 6, 25, false);
        sleep_ms(50);
        state++;
    }
}

int main() {
    init_hardware();

    // commutate_open_loop();   // May be helpful for debugging electrical problems

    if(IDENTIFY_HALLS_ON_BOOT)
        identify_halls();

    sleep_ms(1000);

    pwm_set_irq_enabled(A_PWM_SLICE, true); // Enables interrupts, starting motor commutation

    while (true) {
        printf("%6d, %6d, %6d, %6d, %2d, %2d\n", current_ma, current_target_ma, duty_cycle, voltage_mv, hall, motorState);
        gpio_put(LED_PIN, !gpio_get(LED_PIN));  // Toggle the LED
        sleep_ms(100);
    }

    return 0;
}
