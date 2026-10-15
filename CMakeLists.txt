cmake_minimum_required(VERSION 3.16)
project(RP2350code VERSION 1.0.0 LANGUAGES C CXX)

set(FPRIME_INCLUDE_FRAMEWORK_CODE OFF)
if(DEFINED ENV{FPRIME_FRAMEWORK_PATH})
    set(FPRIME_FRAMEWORK_PATH "$ENV{FPRIME_FRAMEWORK_PATH}")
else()
    set(FPRIME_FRAMEWORK_PATH "${CMAKE_CURRENT_LIST_DIR}/third-party/fprime")
endif()
if(NOT EXISTS "${FPRIME_FRAMEWORK_PATH}/cmake/FPrime.cmake")
    message(FATAL_ERROR
        "F Prime not found at ${FPRIME_FRAMEWORK_PATH}. "
        "Clone nasa/fprime v4.4.0 there or set FPRIME_FRAMEWORK_PATH.")
endif()

find_package(FPrime REQUIRED PATHS "${FPRIME_FRAMEWORK_PATH}")
fprime_add_validation_flags()
add_subdirectory("${FPRIME_FRAMEWORK_PATH}" "${CMAKE_BINARY_DIR}/F-Prime")

add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Components/PicoLink/")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/PicoLinkDeployment/")
