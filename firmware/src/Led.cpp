#include "Led.hpp"

#include "hardware/gpio.h"
#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#ifndef PICO_DEFAULT_LED_PIN
#error "LED heartbeat requires a board with a direct GPIO LED; build for pico2."
#endif

namespace {

constexpr uint LED_PIN = PICO_DEFAULT_LED_PIN;
// Breadboard: GP15 (physical pin 20) -> 330 ohms -> LED anode (+).
// LED cathode (-) -> GND (for example, physical pin 18).
constexpr uint EXTERNAL_LED_PIN = 15;
constexpr uint LED_HALF_PERIOD_MS = 250;
constexpr UBaseType_t LED_TASK_PRIORITY = 1;  // Below sensor (2) and protocol (3).
constexpr configSTACK_DEPTH_TYPE LED_TASK_STACK_WORDS = 256;

// Static storage matches this firmware's no-heap FreeRTOS configuration.
StaticTask_t g_ledTaskControl;
StackType_t g_ledTaskStack[LED_TASK_STACK_WORDS];

void ledTask(void*) {
    gpio_init(LED_PIN);               // Select GPIO functionality.
    gpio_put(LED_PIN, false);         // Prepare an initial LOW output.
    gpio_set_dir(LED_PIN, GPIO_OUT);  // Enable the pin as an output.

    gpio_init(EXTERNAL_LED_PIN);               // Select GP15 for the breadboard LED.
    gpio_put(EXTERNAL_LED_PIN, false);         // Prepare its initial LOW output.
    gpio_set_dir(EXTERNAL_LED_PIN, GPIO_OUT);  // Drive the LED through its resistor.

    while (true) {
        gpio_put(LED_PIN, true);
        gpio_put(EXTERNAL_LED_PIN, true);  // Both LEDs turn on together.
        vTaskDelay(pdMS_TO_TICKS(LED_HALF_PERIOD_MS));
        gpio_put(LED_PIN, false);
        gpio_put(EXTERNAL_LED_PIN, false); // Both LEDs turn off together.
        vTaskDelay(pdMS_TO_TICKS(LED_HALF_PERIOD_MS));
    }
}

}  // namespace

void startLedTask() {
    const TaskHandle_t handle = xTaskCreateStatic(
        ledTask, "led", LED_TASK_STACK_WORDS, nullptr,
        LED_TASK_PRIORITY, g_ledTaskStack, &g_ledTaskControl);
    configASSERT(handle != nullptr);
    (void)handle;
}
