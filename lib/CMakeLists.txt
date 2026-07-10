include(FetchContent)

# FreeRTOS requires a target that provides FreeRTOSConfig.h
add_library(freertos_config INTERFACE)
target_include_directories(freertos_config INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

set(FREERTOS_PORT GCC_ARM_CM3 CACHE STRING "")
set(FREERTOS_HEAP 4 CACHE STRING "")

FetchContent_Declare(
    freertos_kernel
    URL https://github.com/FreeRTOS/FreeRTOS-Kernel/archive/refs/tags/V11.3.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(freertos_kernel)
