#1. Define the minimum required CMake version
cmake_minimum_required(VERSION 3.20)

# ============================================================
# Firmware Reference
# Embedded firmware reference for harsh environment systems.
# Built on NXP LPC1768 (ARM Cortex-M3).
# ============================================================

#2. Define the project name and language (C++ by default), info
project(FirmwareReference
    VERSION 0.1.0
    DESCRIPTION "Embedded firmware reference for harsh environment systems"
    LANGUAGES C CXX
)

# C++17 (required by GoogleTest) + C11
# https://google.github.io/googletest/quickstart-cmake.html
# C11 Explicit, to avoid auto selection of default compiler.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

#https://cmake.org/cmake/help/latest/variable/CMAKE_BUILD_TYPE.html#variable:CMAKE_BUILD_TYPE
# CMAKE_BUILD_TYPE [Debug, Release, RelWithDebInfo and MinSizeRel]
# Debug: Code isn't optimize it, but includes debug
# Release: The compiler optimizes the code as much as possible so that the program runs as fast as possible, but eliminates debugging information.
# Debug build by default
# We check if the build type has not been defined
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "No build type specified. Using Debug by default.")
  
  # When using CACHE, the variable is persistently saved in the project
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the build type: None Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()

# Strict warnings (catches bugs early)
# https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html
# Flags: 
#	Wall [Warning All]: e.g. catch function that might return, but return 0, etc
#	Wextra: e.g. int > unsigned int, possible Error and Bug.
#	Wpedantic:  strict ISO C and ISO C++
# 	Werror: Turn all warnings into errors.
add_compile_options(-Wall -Wextra -Wpedantic -Werror)
# Note: It's better use the following inst. to don't include libraries.
# add_executable(FileName FileName.cpp) # Same that but just apply to FileName
# target_compile_options(FileName PRIVATE -Wall -Wextra -Wpedantic -Werror) # Same that but just apply to FileName


# Coverage flags - used in CI
# Default OFF to allow a Quick compilation // GitHub Actions activate with -DENABLE_COVERAGE=ON
# Workflow:
# 	[C++ Code] ──(cmake -DENABLE_COVERAGE=ON)──> [Compilation with --coverage]
#																▼
#		[HTML/XML Report] <──(lcov / gcovr) <── [Run Tests (./my_test)]

option(ENABLE_COVERAGE "Enable coverage reporting" OFF)
if(ENABLE_COVERAGE)
	# Compilation Flags:
	# 	coverage: passes the instrumentation flags to compiler and linker
	#	O0: Force optimization level 0 for Debug. Local FW development and troubleshooting.
	#		O3: For release - Production deployment where speed is critical.
	# 		O1 or Os: For MinSizeRel (Minimum Size Release) for Embedded Sys.
	# 	g: map connecting bin code back to original source code lines
    add_compile_options(--coverage -O0 -g)
    # passes the coverage flag to the linker
    add_link_options(--coverage)
endif()
# Note: It's better use the following inst. to don't include libraries.
# if(ENABLE_COVERAGE)
#    # Apply just to Target File
#    target_compile_options(FileName PRIVATE --coverage -O0 -g)
#    target_link_options(FileName PRIVATE --coverage)
# endif()

# ---- Local coverage target (macOS only) ----
if(APPLE AND ENABLE_COVERAGE)
    find_program(LLVM_COV llvm-cov)
    if(LLVM_COV)
        add_custom_target(coverage
            COMMAND ${LLVM_COV} gcov ${CMAKE_BINARY_DIR}
            COMMENT "Generating coverage report with llvm-cov..."
        )
    else()
        message(WARNING "llvm-cov not found. Install with: brew install llvm")
    endif()
endif()

# ---- HAL interface library (header-only, no hardware deps) ----
add_library(hal_interface INTERFACE)
# Search library files (.h) on ${CMAKE_SOURCE_DIR}/include folder
target_include_directories(hal_interface INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# ---- Drivers library (compiled; depends on HAL interfaces) ----
# Compile FileName.cpp and creates drivers.a file, that it'll use for main.cpp
add_library(drivers STATIC
    src/drivers/telemetry_protocol.cpp
    src/drivers/rs485_protocol.cpp
    src/drivers/mcp2515.cpp
    src/util/cobs.cpp
)
target_link_libraries(drivers PUBLIC hal_interface)


# ---- Build mode detection ----
# Host build: unit tests with GoogleTest (no hardware needed)
# Target build: cross-compile for LPC1768 
if(CMAKE_CROSSCOMPILING)
    message(STATUS ">>> Target build: LPC1768 (firmware)")

    add_subdirectory(lib)

    add_executable(firmware.elf
        src/app/main_blinky.cpp
        src/hal/lpc1768/rs485_lpc1768.cpp
        lib/startup.cpp
        lib/clock_config.cpp
    )

    target_include_directories(firmware.elf PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )

    target_link_libraries(firmware.elf PRIVATE
        freertos_kernel
        drivers
    )

    target_compile_options(freertos_kernel_port PRIVATE -Wno-unused-variable)

    target_link_options(firmware.elf PRIVATE
        -T ${CMAKE_CURRENT_SOURCE_DIR}/lib/lpc1768.ld
    )

    set_target_properties(firmware.elf PROPERTIES
        SUFFIX ""
    )

    add_custom_command(TARGET firmware.elf POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary firmware.elf firmware.bin
        COMMENT "Generating firmware.bin"
    )

    # Helper: bare-metal target with linker script + .bin generation
    function(add_baremetal_target NAME)
        add_executable(${NAME}.elf ${ARGN})
        target_link_options(${NAME}.elf PRIVATE
            -T ${CMAKE_CURRENT_SOURCE_DIR}/lib/lpc1768.ld
        )
        set_target_properties(${NAME}.elf PROPERTIES SUFFIX "")
        add_custom_command(TARGET ${NAME}.elf POST_BUILD
            COMMAND ${CMAKE_OBJCOPY} -O binary ${NAME}.elf ${NAME}.bin
            COMMENT "Generating ${NAME}.bin"
        )
    endfunction()

    # ---- Minimal bring-up smoke test (no PLL, no FreeRTOS, no UART) ----
    add_baremetal_target(blinky_minimal
        tests/blinky_minimal.cpp
    )

    # ---- Port 2 GPIO output test (no PLL, 4 MHz IRC) ----
    # Port 0 outputs are dead on this chip; verifies Port 2 works for
    # bit-banged SPI on p21-p26. Probe with logic analyzer.
    add_baremetal_target(port2_test
        tests/port2_test.cpp
    )

    # ---- HIL test: RS-485 loopback vs RP2040 echo responder ----
    # 96 MHz PLL + real HAL, UART2 @ 115200. LED1 = pass, LED4 = fail.
    add_baremetal_target(rs485_test
        tests/rs485_test.cpp
        src/hal/lpc1768/rs485_lpc1768.cpp
        lib/clock_config.cpp
    )
    target_include_directories(rs485_test.elf PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(rs485_test.elf PRIVATE drivers)

    # ---- HIL test: MCP2515 internal loopback on bit-bang SPI ----
    # 96 MHz PLL, bit-bang SPI @ 1 MHz on Port 2 (p21-p26),
    # CAN loopback mode. LED1 = pass, LED4 = fail.
    add_baremetal_target(mcp2515_loopback_test
        tests/mcp2515_loopback_test.cpp
        src/hal/lpc1768/bitbang_spi_lpc1768.cpp
        src/drivers/mcp2515.cpp
        lib/clock_config.cpp
    )
    target_include_directories(mcp2515_loopback_test.elf PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(mcp2515_loopback_test.elf PRIVATE hal_interface)

    # ---- HIL test: MCP2515 <-> MCP2515 real two-node CAN bus ----
    # Mode::NORMAL, counterpart to rp2040/can_node.cpp. LED1 = heard
    # the RP2040, LED4 = didn't.
    add_baremetal_target(mcp2515_bus_test
        tests/mcp2515_bus_test.cpp
        src/hal/lpc1768/bitbang_spi_lpc1768.cpp
        src/drivers/mcp2515.cpp
        lib/clock_config.cpp
    )
    target_include_directories(mcp2515_bus_test.elf PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(mcp2515_bus_test.elf PRIVATE hal_interface)

    # ---- MCP2515 SPI probe: read CANCTRL after RESET, expect 0x87 ----
    # LED1=0x87 (chip alive), LED2=0x00, LED3=0xFF (SO floating), LED4=other.
    add_baremetal_target(mcp2515_probe_test
        tests/mcp2515_probe_test.cpp
        src/hal/lpc1768/bitbang_spi_lpc1768.cpp
        lib/clock_config.cpp
    )
    target_include_directories(mcp2515_probe_test.elf PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(mcp2515_probe_test.elf PRIVATE hal_interface)

    # ---- HIL probe: bit-bang I2C, read BMP280 CHIP_ID (expect 0x58) ----
    add_baremetal_target(i2c_probe_test
        tests/i2c_probe_test.cpp
        src/hal/lpc1768/bitbang_i2c_lpc1768.cpp
        lib/clock_config.cpp
    )
    target_include_directories(i2c_probe_test.elf PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(i2c_probe_test.elf PRIVATE hal_interface)

    # ---- SPI debug: raw C bitbang with GPIO loopback on logic analyzer ----
    # Sends 0xC0 (1 byte) then 3 bytes; connect CH0=SCK(p26), CH1=MOSI(p25),
    # CH2=CS(p23). LED1 on after data, LED4 blinks at ~1 Hz.
    # If 8 SCK pulses don't appear, the bug is fundamental (not class-related).
    add_baremetal_target(spi_debug_test
        tests/spi_debug_test.cpp
    )

    add_baremetal_target(sck_toggle_test
        tests/sck_toggle_test.cpp
    )

    add_baremetal_target(sck_watchdog_test
        tests/sck_watchdog_test.cpp
    )
else()
    message(STATUS ">>> Host build: unit tests (no hardware)")
    enable_testing()
    add_subdirectory(tests)
    # Add tests/CMakeLists.txt
    # add_executable(exec_test test_telemetry.cpp)
    # target_link_libraries(exec_test PRIVATE drivers GTest::gtest_main)
    # add_test(NAME AllTest COMMAND executable_tests)
endif()