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
add_library(drivers STATIC src/drivers/telemetry_protocol.cpp)
target_link_libraries(drivers PUBLIC hal_interface)


# ---- Build mode detection ----
# Host build: unit tests with GoogleTest (no hardware needed)
# Target build: cross-compile for LPC1768 
if(CMAKE_CROSSCOMPILING)
    message(STATUS ">>> Target build: LPC1768 (firmware)")
    # add_subdirectory(src/app)
    # Add src/app/CMakeLists.txt
	# add_executable(firmware.elf main_firmware.cpp)
	# target_link_libraries(firmware.elf PRIVATE drivers)
else()
    message(STATUS ">>> Host build: unit tests (no hardware)")
    enable_testing()
    add_subdirectory(tests)
    # Add tests/CMakeLists.txt
    # add_executable(exec_test test_telemetry.cpp)
    # target_link_libraries(exec_test PRIVATE drivers GTest::gtest_main)
    # add_test(NAME AllTest COMMAND executable_tests)
endif()