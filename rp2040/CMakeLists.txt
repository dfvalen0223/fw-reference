cmake_minimum_required(VERSION 3.20)

set(PICO_SDK_PATH $ENV{HOME}/pico-sdk)
include($ENV{HOME}/pico-sdk/external/pico_sdk_import.cmake)

project(rp2040_rs485_responder
    VERSION 0.1.0
    DESCRIPTION "RP2040 RS-485 echo responder for HIL testing"
    LANGUAGES C CXX ASM
)

pico_sdk_init()

add_executable(echo_responder
    echo_responder.cpp
)

target_link_libraries(echo_responder
    pico_stdlib
    hardware_uart
    hardware_gpio
)

pico_add_extra_outputs(echo_responder)

target_compile_options(echo_responder PRIVATE
    -Wall -Wextra
)
