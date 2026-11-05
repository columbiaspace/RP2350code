#include "IrAdcSensor.hpp"

#include "hardware/adc.h"

#include <cstdint>

namespace {

// Pico 2 pinout: GP28 is physical pin 34 and is ADC input 2.
constexpr uint IR_ADC_GPIO = 28;
constexpr uint IR_ADC_INPUT = 2;

// 12-bit ADC: raw counts run 0..4095 for 0..3.3 V.
constexpr std::uint32_t ADC_FULL_SCALE = 4095;
constexpr std::uint32_t ADC_REF_MILLIVOLTS = 3300;

}  // namespace

bool IrAdcSensor::initialize() {
    adc_init();                  // turn on the ADC peripheral
    adc_gpio_init(IR_ADC_GPIO);  // GP28 as analog input (not digital GPIO)
    adc_select_input(IR_ADC_INPUT);
    return true;
}

SensorSample IrAdcSensor::read(std::uint64_t nowUs) {
    adc_select_input(IR_ADC_INPUT);
    const std::uint16_t counts = adc_read();  // 0..4095
    if (counts > ADC_FULL_SCALE) {
        return {false, 0, nowUs, "ADC"};
    }
    // Integer millivolts: counts * 3300 / 4095. This is voltage, not distance.
    const std::int32_t millivolts = static_cast<std::int32_t>(
        (static_cast<std::uint32_t>(counts) * ADC_REF_MILLIVOLTS) / ADC_FULL_SCALE);
    return {true, millivolts, nowUs, "OK"};
}
