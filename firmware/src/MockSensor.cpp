#include "MockSensor.hpp"

bool MockSensor::initialize() {
    return true;
}

SensorSample MockSensor::read(std::uint64_t nowUs) {
    const std::int32_t millivolts =
        static_cast<std::int32_t>((nowUs / 100000) % 3301);
    return {true, millivolts, nowUs, "OK"};
}

const char* MockSensor::sourceName() const {
    return "MOCK";
}
