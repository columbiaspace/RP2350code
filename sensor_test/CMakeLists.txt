cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(sensor_test C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(sensor_test main.cpp)

pico_enable_stdio_usb(sensor_test 1)
pico_enable_stdio_uart(sensor_test 0)

target_link_libraries(sensor_test 
    pico_stdlib 
    hardware_adc
)

pico_add_extra_outputs(sensor_test)

