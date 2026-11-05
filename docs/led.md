# Onboard and breadboard LED test

The Pico 2 runs one LED task alongside the existing sensor and UART tasks.
The onboard LED and an external breadboard LED blink together: approximately
250 ms on and 250 ms off, or two complete flashes per second. They start
automatically when FreeRTOS starts. All LED behavior is in `firmware/src/Led.cpp`.

This uses the onboard LED on the **non-wireless Pico 2**, built with
`PICO_BOARD=pico2`. The board defines `PICO_DEFAULT_LED_PIN` as GP25. The external
LED uses GP15, which is physical header pin 20. GPIO numbers and physical header
pin numbers are different: the code uses `15`, not `20`.
Pico 2 W uses a different LED interface; this implementation does not support it.

A blinking LED shows that this task is being scheduled. It does not establish
that the sensor is healthy or that UART communication is working. There is no
ground-station LED command in this change.

## Confirmed working setup

On November 4, 2026, you confirmed that the breadboard LED works with a **550 ohm
series resistor**. This is your reported hardware result; current was calculated,
not measured. The updated Pico firmware also built successfully.

| Item | Working setup |
|---|---|
| Board | Non-wireless Raspberry Pi Pico 2 |
| LED | Two-leg 5 mm LED; stated forward voltage 2.0-2.2 V, forward current 20 mA |
| Resistor | 550 ohms in series |
| Output | GP15, physical pin 20 |
| Return | Pico GND, physical pin 18 |
| Expected pattern | Onboard and breadboard LEDs blink together: 250 ms on, 250 ms off |

## Checklist for next time

1. **Just running the same program?** Connect power normally. No rebuild or
   BOOTSEL is needed; the Pico remembers the firmware you flashed.
2. **Changing the wiring?** Disconnect power, follow the wiring below, then
   reconnect. Changing between the supported resistor values needs no code change.
3. **Changing the behavior?** Edit and save `firmware/src/Led.cpp`.
4. **Build** using the command under [Build and check on the board](#build-and-check-on-the-board).
   Continue only if the build succeeds.
5. **Flash** the newly built UF2 using BOOTSEL. Building on the Mac alone does not
   update the Pico.
6. **Test** both LEDs and compare their pattern with the timing you selected.

Editing this Markdown guide requires no rebuild or flashing.

## Wire your two-leg LED

Your part is a 5 mm LED with a 2.0-2.2 V forward voltage and a stated 20 mA forward
current. It emits light; it is not a light-measuring sensor in this circuit.

Use your **550 ohm resistor** in series with the LED. A standard 1/4 W resistor is
suitable. At a nominal 3.3 V GPIO output, the expected current is approximately:

```text
I = (3.3 V - LED forward voltage) / 550 ohms
  = about 2.0-2.4 mA for a 2.0-2.2 V LED
```

The LED can operate below its stated 20 mA. The resistor limits the current; do
not connect the bare LED directly between GPIO and ground.

A **330 ohm resistor**, as shown in the source comments, is also suitable for this
LED and gives about 3.3-3.9 mA. The 550 ohm resistor may give a dimmer light. The
resistor value is a hardware choice, not a firmware setting; either value uses
the same code.

1. Disconnect USB and any other power before changing wires.
2. Insert the LED's two legs into separate, unconnected breadboard rows.
3. Connect **GP15 (physical pin 20)** to one end of the **550 ohm resistor**.
4. Connect the resistor's other end to the LED's **long leg (+, anode)**.
5. Connect the LED's **short leg (-, cathode, normally the flat side of the body)**
   to **GND (physical pin 18)**.

```text
Pico 2 GP15               LED
physical pin 20 --[550 ohms]-- long leg (+)  short leg (-) -- GND pin 18
```

| Pico connection | Breadboard connection |
|---|---|
| GP15, physical pin 20 | Resistor, then LED long leg (+) |
| GND, physical pin 18 | LED short leg (-) |

With USB at the top and the components facing you, physical pin 20 is the bottom
pin on the left header; physical pin 18 is two positions above it. Check the
[official Pico 2 pinout](https://datasheets.raspberrypi.com/pico/Pico-2-Pinout.pdf).

No separate breadboard power supply is needed: the GPIO output supplies this small
LED current. Do not connect this circuit to VBUS/5 V. If you use a breadboard ground
rail, connect that rail to Pico GND. Keep the LED legs in different connected
strips; putting both into the same five-hole strip shorts them together.

GP0/GP1 remain assigned to UART and GP28 remains the analog sensor input. GP15 is
now an output owned by the LED task; do not attach a sensor output to that pin.

## Files added and changed

| File | Change |
|---|---|
| `firmware/src/Led.hpp` | New public declaration of `startLedTask()`. |
| `firmware/src/Led.cpp` | GPIO setup for onboard GP25 and external GP15, shared blink loop, and static task creation. |
| `docs/led.md` | This explanation and build guide. |
| `firmware/CMakeLists.txt` | One source-list entry: `src/Led.cpp`. |
| `firmware/src/main.cpp` | Include `Led.hpp`, call `startLedTask()`, and update the opening task description. |

The existing sensor implementation, UART protocol, F Prime components, deployment,
and smoke-test script were not changed. Previously identified sensor and error
reporting issues are outside this LED change.

The breadboard extension changes only `Led.cpp` and this guide. The existing
header, startup call, and CMake source entry already connect the LED task, so no
additional integration changes are needed.

## How the files connect

```text
firmware/CMakeLists.txt includes Led.cpp in the firmware executable
    -> main.cpp includes Led.hpp so it knows startLedTask() exists
    -> main() calls startLedTask() once
    -> startLedTask() registers ledTask with FreeRTOS
    -> vTaskStartScheduler() starts scheduling all three tasks
    -> ledTask configures both LED pins and blinks them together forever
```

The header declares a function; the `.cpp` implements it. Adding a source file to
CMake compiles and links it, but does not run its functions. The call in `main()`
provides that execution connection. `Led.cpp` does not need a second `main()`.

## Led.hpp, line by line

| Statement | Meaning |
|---|---|
| `#ifndef PICO_LINK_LED_HPP` | Start a guard against including the header more than once. |
| `#define PICO_LINK_LED_HPP` | Mark the header as already included. |
| `void startLedTask();` | Declare a function with no arguments and no return value. |
| `#endif` | End the header guard. |

Call `startLedTask()` exactly once, before the scheduler starts. That is how the
existing `main.cpp` now uses it.

## Led.cpp, line by line

| Statement | Meaning |
|---|---|
| `#include "Led.hpp"` | Include this module's public declaration. |
| `#include "hardware/gpio.h"` | Make the GPIO functions available. |
| `#include "pico/stdlib.h"` | Include Pico support and the selected board's definitions. |
| `#include "FreeRTOS.h"` | Include FreeRTOS types and project configuration. |
| `#include "task.h"` | Include task creation and delay functions. |
| `#ifndef PICO_DEFAULT_LED_PIN` | Check whether the board defines a direct GPIO LED. |
| `#error ...` | Stop compilation with an explanation if that LED definition is missing. |
| `#endif` | End the board-definition check. |
| `namespace {` | Keep the following names private to this source file. |
| `LED_PIN = PICO_DEFAULT_LED_PIN` | Use the board's onboard LED pin. |
| `EXTERNAL_LED_PIN = 15` | Use GP15 (physical pin 20) for the breadboard LED. |
| `LED_HALF_PERIOD_MS = 250` | Set the duration of each on or off phase. |
| `LED_TASK_PRIORITY = 1` | Let the sensor and protocol tasks take precedence. |
| `LED_TASK_STACK_WORDS = 256` | Reserve 256 stack words, or 1024 bytes on this target. |
| `StaticTask_t g_ledTaskControl;` | Reserve FreeRTOS task bookkeeping memory. |
| `StackType_t g_ledTaskStack[...]` | Reserve the stack used by the LED task. |
| `void ledTask(void*)` | Define the function FreeRTOS will run; its parameter is unused. |
| `gpio_init(LED_PIN)` | Configure the pin for GPIO use. |
| `gpio_put(LED_PIN, false)` | Prepare an initial LOW output value. |
| `gpio_set_dir(LED_PIN, GPIO_OUT)` | Enable the pin as a digital output. |
| `gpio_init(EXTERNAL_LED_PIN)` | Configure GP15 for GPIO use. |
| `gpio_put(EXTERNAL_LED_PIN, false)` | Prepare an initial LOW value for GP15. |
| `gpio_set_dir(EXTERNAL_LED_PIN, GPIO_OUT)` | Enable GP15 as an output driving the resistor and external LED. |
| `while (true)` | Keep the task running indefinitely. |
| `gpio_put(LED_PIN, true)` | Turn the onboard LED on. |
| `gpio_put(EXTERNAL_LED_PIN, true)` | Drive GP15 HIGH, turning the external LED on. |
| First `vTaskDelay(...)` | Leave both LEDs on while this task waits and other tasks can run. |
| `gpio_put(LED_PIN, false)` | Turn the onboard LED off. |
| `gpio_put(EXTERNAL_LED_PIN, false)` | Drive GP15 LOW, turning the external LED off. |
| Second `vTaskDelay(...)` | Leave both LEDs off while this task waits, then repeat. |
| `} // namespace` | End the private implementation section. |
| `void startLedTask()` | Define the public startup function used by `main.cpp`. |
| `xTaskCreateStatic(...)` | Create the task using the memory already reserved above. |
| `configASSERT(handle != nullptr)` | Check that task creation returned a handle. |
| `(void)handle` | Avoid an unused-variable warning when assertions are disabled. |

The `xTaskCreateStatic` arguments, in order, are the task function, debug name,
stack capacity in words, parameter pointer, priority, stack array, and task
bookkeeping address. Static allocation matches the existing firmware: no task
heap allocation is introduced. In addition to the 1024-byte stack, the task uses
bookkeeping storage whose size is determined by FreeRTOS.

`pdMS_TO_TICKS` converts milliseconds into scheduler ticks. `vTaskDelay` blocks
only this task; it does not pause the whole firmware. Higher-priority work can
delay a transition, so this is a heartbeat, not a precision timing signal.

To change the blink speed, edit `LED_HALF_PERIOD_MS` in `Led.cpp`, then rebuild
and flash. This constant controls both LEDs.

| `LED_HALF_PERIOD_MS` | On time | Off time | Complete flashes |
|---|---|---|---|
| `250` | 250 ms | 250 ms | Approximately twice per second |
| `500` | 500 ms | 500 ms | Approximately once per second |
| `1000` | 1 second | 1 second | Approximately once every two seconds |

Keep the `vTaskDelay` calls so the LED task waits between transitions and other
tasks can run. For a blink-speed change, leave the pin, task priority, and stack
size alone. Moving the external LED to another pin requires updating both the
wiring and `EXTERNAL_LED_PIN`, while avoiding pins used by other tasks.

## Build and check on the board

For the existing working build, edit, build, flash, and test each time. Run:

```bash
cd /Users/thomaskourakos/Documents/cubesat_mydir/RP2350code
cmake --build firmware/build --parallel
```

Wait for a successful build. If it fails, the UF2 may still contain your old code.
You do not need to reinstall the SDK, kernel, or compiler for each change.

Hold BOOTSEL while connecting the Pico 2 with a USB data cable, then copy
`firmware/build/pico_link_firmware.uf2` onto the board's mounted drive. After it
restarts, expect both LEDs to blink twice per second without a laptop command or
a connected sensor. The existing UART and sensor tasks still start.

On this Mac, show the firmware file in Finder with:

```bash
open -R /Users/thomaskourakos/Documents/cubesat_mydir/RP2350code/firmware/build/pico_link_firmware.uf2
```

Drag the highlighted UF2 onto the `RP2350` drive. The drive disappears when the
Pico restarts; leave USB connected to power it. Ordinary power-on afterward runs
the flashed firmware without BOOTSEL.

This feature requires rebuilding and flashing the Pico firmware only. The Pi's
F Prime executable and the ground-station dictionary do not need to change.

For a fresh checkout, first install the dependencies described in the
[project README](../README.md), then configure and build from the repository root:

```bash
export PICO_SDK_PATH="$PWD/third-party/pico-sdk"
export FREERTOS_KERNEL_PATH="$PWD/third-party/FreeRTOS-Kernel"
cmake -S firmware -B firmware/build -G Ninja -DPICO_BOARD=pico2
cmake --build firmware/build --parallel
```

## Troubleshooting

Disconnect power before adjusting the breadboard wiring.

| What you observe | What to check |
|---|---|
| Onboard LED blinks; breadboard LED does not | LED polarity, 550 ohm resistor connection, shared ground, and GP15 at physical pin 20. Ensure the LED legs are in separate connected strips and header pins have reliable soldered connections. |
| Breadboard LED stays on continuously | Check whether it is connected to 3V3 instead of GP15. Confirm the updated firmware was flashed. |
| Neither LED blinks | Check USB power and whether the Pico is still in BOOTSEL mode. Flash the successfully built firmware, or reconnect normally if it is already flashed. |
| Code was edited, but the old pattern remains | Save the file, build successfully, and flash the new UF2. Editing or building alone does not change the running Pico. |
| No `RP2350` drive appears | Hold BOOTSEL while reconnecting with a USB data cable. Check Finder's Locations or run `ls /Volumes`. The drive is only expected during BOOTSEL mode. |
| The drive disappears after copying the UF2 | Expected: the Pico has restarted to run the firmware. |
| The UF2 file is missing or the build failed | Resolve the build error first. Do not flash an older file as if it contained the latest changes. |

## Possible next extension

For future laptop-controlled LED commands, add a command to the F Prime component
and shared UART protocol, then route it to an LED task queue. Replace or explicitly
disable the heartbeat in manual mode so it cannot overwrite a requested state.

## SDK references

- [Pico 2 physical pinout](https://datasheets.raspberrypi.com/pico/Pico-2-Pinout.pdf)
- [Pico SDK 2.3.1 Pico 2 board definition](https://github.com/raspberrypi/pico-sdk/blob/2.3.1/src/boards/include/boards/pico2.h)
- [FreeRTOS static task creation](https://www.freertos.org/Documentation/02-Kernel/04-API-references/01-Task-creation/02-xTaskCreateStatic)
- [FreeRTOS task delay](https://www.freertos.org/Documentation/02-Kernel/04-API-references/02-Task-control/01-vTaskDelay)
