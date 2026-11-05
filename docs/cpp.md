# C++ used in this firmware

You do not need all of C++. This file is the subset that appears in `firmware/` and `common/protocol/`.

## Files

| Kind | Example | Role |
|---|---|---|
| Header `.hpp` | `Sensor.hpp` | Types and function *declarations* |
| Source `.cpp` | `IrAdcSensor.cpp` | Function *definitions* (the bodies) |

`#include "Sensor.hpp"` copies the header into this file so the compiler knows the types.

`#ifndef` / `#define` / `#endif` around a header is an include guard. It stops the header from being pasted twice.

## Types

```cpp
bool              // true or false
std::uint16_t     // unsigned 16-bit (0..65535)
std::int32_t      // signed 32-bit
std::uint64_t     // unsigned 64-bit (Pico uptime in microseconds)
const char*       // pointer to a C string like "OK"
```

`std::` means the name lives in the C++ standard library namespace.

## Struct

A struct groups fields into one value:

```cpp
struct SensorSample {
    bool valid;
    std::int32_t valueMilli;
    std::uint64_t capturedUs;
    const char* status;
};

return {true, millivolts, nowUs, "OK"};  // fills fields in order
```

Access a field with a dot: `sample.valueMilli`.

## Class and inheritance

`Sensor` is an interface: it lists functions every sensor must provide, marked `virtual` and `= 0` (no body).

`IrAdcSensor final : public Sensor` means “this class **is a** Sensor.” `override` on `read()` tells the compiler you are filling in that interface. `final` means nothing inherits from `IrAdcSensor`.

To swap sensors you write another class that inherits `Sensor` and construct that instead of `IrAdcSensor g_sensor`.

`ClassName::function` in a `.cpp` file means “this function belongs to that class.”

## Other syntax you will see

```cpp
constexpr uint UART_TX_PIN = 0;   // compile-time constant
namespace { ... }                 // names visible only in this .cpp
static_cast<std::int32_t>(x)      // convert x to int32
(void)handle;                     // we have the value but do not use it
reinterpret_cast<const uint8_t*>  // treat these bytes as another type (UART write)
```

F Prime on the Pi is also C++, plus FPP (`.fpp`) which generates command/telemetry glue. The Pico does not run F Prime.
