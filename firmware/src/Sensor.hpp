#ifndef PICO_LINK_SENSOR_HPP
#define PICO_LINK_SENSOR_HPP

#include <cstdint>

struct SensorSample {
  bool valid;
  // Analog IR voltage in millivolts (0..3300 vs the 3.3 V ADC reference).
  std::int32_t valueMilli;
  std::uint64_t capturedUs;
  const char *status;
};

class Sensor {
public:
  virtual ~Sensor() = default;
  virtual bool initialize() = 0;
  virtual SensorSample read(std::uint64_t nowUs) = 0;
  virtual const char *sourceName() const = 0;
};

#endif
