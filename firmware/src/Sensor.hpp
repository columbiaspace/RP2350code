#ifndef PICO_LINK_SENSOR_HPP
#define PICO_LINK_SENSOR_HPP

#include <cstdint>

// One measurement from whatever sensor is plugged into the Pico.
// The rest of the firmware never cares *which* sensor produced this.
struct SensorSample {
    bool valid;              // false = do not trust valueMilli
    std::int32_t valueMilli; // analog voltage in millivolts (0..3300 for GP28 ADC)
    std::uint64_t capturedUs; // Pico uptime when the sample was taken
    const char* status;      // short token with no commas: "OK" or an error name
};

// Interface every sensor driver must implement.
//
// To switch sensors:
//   1. Write a new class that inherits from Sensor (see IrAdcSensor).
//   2. In firmware/src/main.cpp, construct that class as g_sensor.
//   3. Add the new .cpp file to firmware/CMakeLists.txt.
//   4. Rebuild and flash the Pico UF2.
class Sensor {
  public:
    virtual ~Sensor() = default;
    virtual bool initialize() = 0;
    virtual SensorSample read(std::uint64_t nowUs) = 0;
};

#endif
