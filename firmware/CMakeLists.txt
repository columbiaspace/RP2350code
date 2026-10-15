cmake_minimum_required(VERSION 3.22)

if(NOT DEFINED PICO_SDK_PATH AND DEFINED ENV{PICO_SDK_PATH})
    set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
endif()
if(NOT PICO_SDK_PATH)
    message(FATAL_ERROR "Set PICO_SDK_PATH to a Pico SDK 2.3.1 checkout")
endif()

include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
project(pico_link_firmware LANGUAGES C CXX ASM)
pico_sdk_init()

if(NOT DEFINED FREERTOS_KERNEL_PATH AND DEFINED ENV{FREERTOS_KERNEL_PATH})
    set(FREERTOS_KERNEL_PATH "$ENV{FREERTOS_KERNEL_PATH}")
endif()
if(NOT FREERTOS_KERNEL_PATH)
    set(FREERTOS_KERNEL_PATH "${CMAKE_CURRENT_LIST_DIR}/../third-party/FreeRTOS-Kernel")
endif()
set(FREERTOS_RP2350_PORT
    "${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2350_ARM_NTZ")
if(NOT EXISTS "${FREERTOS_RP2350_PORT}/FreeRTOS_Kernel_import.cmake")
    message(FATAL_ERROR
        "FreeRTOS RP2350 port not found. Clone raspberrypi/FreeRTOS-Kernel "
        "commit 4f7299d6ea746b27a9dd19e87af568e34bd65b15 to "
        "${FREERTOS_KERNEL_PATH}, or set FREERTOS_KERNEL_PATH.")
endif()
include("${FREERTOS_RP2350_PORT}/FreeRTOS_Kernel_import.cmake")

option(PICO_LINK_MOCK_SENSOR "Generate clearly marked mock millivolt readings" OFF)

if(PICO_LINK_MOCK_SENSOR)
    set(PICO_LINK_SENSOR_SOURCES src/MockSensor.cpp)
else()
    set(PICO_LINK_SENSOR_SOURCES src/IrAdcSensor.cpp)
endif()

add_executable(pico_link_firmware
    src/main.cpp
    ${PICO_LINK_SENSOR_SOURCES}
    ../common/protocol/PicoProtocol.cpp
)
target_include_directories(pico_link_firmware PRIVATE
    src
    ../common/protocol
)
target_compile_features(pico_link_firmware PRIVATE cxx_std_17)
target_compile_definitions(pico_link_firmware PRIVATE
    PICO_LINK_MOCK_SENSOR=$<BOOL:${PICO_LINK_MOCK_SENSOR}>
)
target_compile_options(pico_link_firmware PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(pico_link_firmware PRIVATE
    FreeRTOS-Kernel-Static
    pico_stdlib
    hardware_uart
    hardware_timer
)
if(NOT PICO_LINK_MOCK_SENSOR)
    target_link_libraries(pico_link_firmware PRIVATE hardware_adc)
endif()

# Protocol UART is UART0 on GP0/GP1. Logs use USB CDC only.
pico_enable_stdio_uart(pico_link_firmware 0)
pico_enable_stdio_usb(pico_link_firmware 1)
pico_add_extra_outputs(pico_link_firmware)
