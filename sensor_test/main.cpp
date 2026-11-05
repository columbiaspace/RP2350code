// this shows that the sensor works!


#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"

#define ADC_PIN 28
#define ADC_CHANNEL 2

int main() {
    stdio_init_all();
    
    adc_init();
    adc_gpio_init(ADC_PIN);
    adc_select_input(ADC_CHANNEL);

    printf("--- Pico 2 IR Sensor Test Start ---\n");

    while (true) {
        uint16_t raw_value = adc_read();
        uint32_t millivolts = ((uint32_t)raw_value * 3300) / 4095;

        printf("Raw Count: %4d | Sensor Voltage: %4d mV\n", raw_value, millivolts);

        sleep_ms(500);
    }

    return 0;
}


