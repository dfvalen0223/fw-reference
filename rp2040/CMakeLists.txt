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

pico_generate_pio_header(echo_responder ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)

target_link_libraries(echo_responder
    pico_stdlib
    hardware_uart
    hardware_gpio
    hardware_pio
)

pico_add_extra_outputs(echo_responder)

target_compile_options(echo_responder PRIVATE
    -Wall -Wextra
)

# ---- Protocol responder: speaks Rs485Protocol (COBS+CRC+ACK) ----
add_executable(protocol_responder
    protocol_responder.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../src/util/cobs.cpp
)

pico_generate_pio_header(protocol_responder ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)

target_include_directories(protocol_responder PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../include
)

target_link_libraries(protocol_responder
    pico_stdlib
    hardware_uart
    hardware_gpio
    hardware_pio
)

pico_add_extra_outputs(protocol_responder)

