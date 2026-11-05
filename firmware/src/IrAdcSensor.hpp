#ifndef PICO_LINK_IR_ADC_SENSOR_HPP
#define PICO_LINK_IR_ADC_SENSOR_HPP

#include "Sensor.hpp"

// Analog IR (or any 0..3.3 V analog output) on Pico GP28 = ADC2.
// Hookup: sensor VCC -> Pico 3V3, GND -> GND, analog OUT -> GP28.
class IrAdcSensor final : public Sensor {
  public:
    bool initialize() override;
    SensorSample read(std::uint64_t nowUs) override;
};

#endif
