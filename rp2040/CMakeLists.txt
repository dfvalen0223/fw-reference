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

# ---- CAN node: MCP2515 over SPI0, two-node real-bus HIL test ----
add_executable(can_node
    can_node.cpp
    bitbang_spi_rp2040.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../src/drivers/mcp2515.cpp
)

pico_generate_pio_header(can_node ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)

target_include_directories(can_node PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../include
    ${CMAKE_CURRENT_LIST_DIR}/../src
)

target_link_libraries(can_node
    pico_stdlib
    hardware_gpio
    hardware_pio
)

pico_enable_stdio_usb(can_node 1)
pico_enable_stdio_uart(can_node 0)

pico_add_extra_outputs(can_node)

# ---- Decisive triage: MCP2515 internal loopback, no transceiver/bus involved ----
add_executable(can_loopback_test
    can_loopback_test.cpp
    bitbang_spi_rp2040.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../src/drivers/mcp2515.cpp
)

pico_generate_pio_header(can_loopback_test ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)

target_include_directories(can_loopback_test PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../include
    ${CMAKE_CURRENT_LIST_DIR}/../src
)

target_link_libraries(can_loopback_test
    pico_stdlib
    hardware_gpio
    hardware_pio
)

pico_add_extra_outputs(can_loopback_test)

