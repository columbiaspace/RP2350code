# Pico GPIO and the analog sensor

A **GPIO** (General Purpose Input/Output) pin is a chip pad you can use as digital in, digital out, UART, ADC, and more. The Pico SDK call `gpio_set_function(pin, GPIO_FUNC_UART)` assigns a pin to a peripheral.

## Pins this firmware uses

Pico 2 physical pin numbers (the plastic header) are not the same as GP numbers.

| Function | GP number | Physical pin |
|---|---|---|
| UART TX | GP0 | 1 |
| UART RX | GP1 | 2 |
| UART GND | — | 3 (any GND works) |
| IR analog in | GP28 / ADC2 | 34 |
| IR / Pico 3.3 V | — | 36 (3V3 OUT) |
| IR GND | — | 33 |

GP0/GP1 are the default UART0 pins. GP28 is ADC input 2 (12-bit, 0..4095 for 0..3.3 V).

## Analog vs UART

UART is **digital**: timed 0s and 1s that form bytes.

The IR pin is **analog**: a voltage between 0 V and 3.3 V. The ADC turns that voltage into a number. There is no protocol on GP28.

## Hook up a real analog IR sensor

```text
Sensor VCC  -> Pico 3V3 (pin 36)
Sensor GND  -> Pico GND (pin 33)
Sensor OUT  -> Pico GP28 (pin 34)
```

Keep analog OUT ≤ 3.3 V. Power the Pico from USB; the UART cable does not power it.

`IrAdcSensor` converts:

```text
millivolts = adc_counts * 3300 / 4095
```

Zero millivolts can be a real dark reading. Check wiring and `valid=1` before calling it a fault.

## Digital sensors

If your module is HIGH/LOW on a GPIO instead of analog, you still implement `Sensor::read()`. Return 0 or 3300 (or any units you choose) in `valueMilli`. The UART protocol does not change — only `firmware/src/` does. See `Sensor.hpp`.
