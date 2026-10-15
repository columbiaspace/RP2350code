#include "IrAdcSensor.hpp"

#include "hardware/adc.h"

#include <cstdint>

namespace {

constexpr uint IR_ADC_GPIO = 28;
constexpr uint IR_ADC_INPUT = 2;
constexpr std::uint32_t ADC_FULL_SCALE = 4095;
constexpr std::uint32_t ADC_REF_MILLIVOLTS = 3300;

}  // namespace

bool IrAdcSensor::initialize() {
    adc_init();
    adc_gpio_init(IR_ADC_GPIO);
    adc_select_input(IR_ADC_INPUT);
    return true;
}

SensorSample IrAdcSensor::read(std::uint64_t nowUs) {
    adc_select_input(IR_ADC_INPUT);
    const std::uint16_t counts = adc_read();
    if (counts > ADC_FULL_SCALE) {
        return {false, 0, nowUs, "ADC"};
    }
    const std::int32_t millivolts =
        static_cast<std::int32_t>((static_cast<std::uint32_t>(counts) * ADC_REF_MILLIVOLTS) /
                                  ADC_FULL_SCALE);
    return {true, millivolts, nowUs, "OK"};
}

const char* IrAdcSensor::sourceName() const {
    return "REAL";
}
