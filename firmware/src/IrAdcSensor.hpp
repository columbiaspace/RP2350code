#ifndef PICO_LINK_IR_ADC_SENSOR_HPP
#define PICO_LINK_IR_ADC_SENSOR_HPP

#include "Sensor.hpp"

class IrAdcSensor final : public Sensor {
  public:
    bool initialize() override;
    SensorSample read(std::uint64_t nowUs) override;
    const char* sourceName() const override;
};

#endif
