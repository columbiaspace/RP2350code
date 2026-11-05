#ifndef PICO_LINK_LED_HPP
#define PICO_LINK_LED_HPP

// Create the onboard LED heartbeat task. Call once, before vTaskStartScheduler().
// Uses the direct GPIO LED on the non-wireless Pico 2 (PICO_BOARD=pico2).
void startLedTask();

#endif
