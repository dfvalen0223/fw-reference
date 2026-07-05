# CMake toolchain file for LPC1768 (ARM Cortex-M3).
# Used when cross-compiling for target (Day 7+).
# Usage:
#   cmake -B build-arm -DCMAKE_TOOLCHAIN_FILE=cmake/arm-toolchain.cmake

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR cortex-m3)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(MCU_FLAGS "-mcpu=cortex-m3 -mthumb")
set(CMAKE_C_FLAGS_INIT "${MCU_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${MCU_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${MCU_FLAGS} -Wl,--gc-sections")