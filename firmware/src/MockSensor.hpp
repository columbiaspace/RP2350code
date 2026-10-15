#ifndef PICO_LINK_MOCK_SENSOR_HPP
#define PICO_LINK_MOCK_SENSOR_HPP

#include "Sensor.hpp"

class MockSensor final : public Sensor {
  public:
    bool initialize() override;
    SensorSample read(std::uint64_t nowUs) override;
    const char* sourceName() const override;
};

#endif
