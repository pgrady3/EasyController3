#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"


const uint LED_PIN = PICO_DEFAULT_LED_PIN;
const uint F_PWM = 16000;
const uint FLAG_PIN = 2;

void on_pwm_wrap() {
    pwm_clear_irq(0);

    gpio_put(FLAG_PIN, 1);
    uint16_t result = adc_read();
    gpio_put(FLAG_PIN, 0);
}

int main() {
    stdio_init_all();
    gpio_init(LED_PIN);
    gpio_init(FLAG_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_set_dir(FLAG_PIN, GPIO_OUT);

    adc_init();
    adc_gpio_init(26);  // Make sure GPIO is high-impedance, no pullups etc
    adc_select_input(0);    // Select ADC input 0 (GPIO26)

    // Tell GPIO 0 and 1 they are allocated to the PWM
    gpio_set_function(0, GPIO_FUNC_PWM);
    gpio_set_function(1, GPIO_FUNC_PWM);

    uint slice_num = pwm_gpio_to_slice_num(0);

    float pwmClockDivider = (float)(clock_get_hz(clk_sys)) / (F_PWM * 255 * 2);


    pwm_clear_irq(slice_num);
    pwm_set_irq_enabled(slice_num, true);
    irq_set_exclusive_handler(PWM_IRQ_WRAP, on_pwm_wrap);
    irq_set_enabled(PWM_IRQ_WRAP, true);

    pwm_set_wrap(slice_num, 255 - 1);
    pwm_set_phase_correct(slice_num, true);
    pwm_set_clkdiv(slice_num, pwmClockDivider);
    pwm_set_output_polarity(slice_num, false, true);

    pwm_set_chan_level(slice_num, PWM_CHAN_A, 1);
    pwm_set_chan_level(slice_num, PWM_CHAN_B, 1);
    
    pwm_set_enabled(slice_num, true);

    // Note we could also use pwm_set_gpio_level(gpio, x) which looks up the
    // correct slice and channel for a given GPIO.
    uint8_t thresh = 0;
    while (true) {
        printf("Wassup homes\n");
        // const float conversion_factor = 3.3f / (1 << 12);
        // uint16_t result = adc_read();
        // printf("Raw value: 0x%03x, voltage: %f V\n", result, result * conversion_factor);

        gpio_put(LED_PIN, 1);
        sleep_ms(200);
        gpio_put(LED_PIN, 0);
        sleep_ms(200);
        thresh += 1;
        // pwm_set_chan_level(slice_num, PWM_CHAN_B, thresh);
    }
    return 0;
}
