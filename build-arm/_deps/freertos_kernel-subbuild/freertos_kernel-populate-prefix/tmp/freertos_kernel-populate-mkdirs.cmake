# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/Users/danielvalencia/Documents/Jobs/Kraken Robotics - Senior Firmware Developer (Towed Systems) - Jun 21 2026/FreeRTOS Practice/fw-reference/build-arm/_deps/freertos_kernel-src"
  "/Users/danielvalencia/Documents/Jobs/Kraken Robotics - Senior Firmware Developer (Towed Systems) - Jun 21 2026/FreeRTOS Practice/fw-reference/build-arm/_deps/freertos_kernel-build"
  "/Users/danielvalencia/Documents/Jobs/Kraken Robotics - Senior Firmware Developer (Towed Systems) - Jun 21 2026/FreeRTOS Practice/fw-reference/build-arm/_deps/freertos_kernel-subbuild/freertos_kernel-populate-prefix"
  "/Users/danielvalencia/Documents/Jobs/Kraken Robotics - Senior Firmware Developer (Towed Systems) - Jun 21 2026/FreeRTOS Practice/fw-reference/build-arm/_deps/freertos_kernel-subbuild/freertos_kernel-populate-prefix/tmp"
  "/Users/danielvalencia/Documents/Jobs/Kraken Robotics - Senior Firmware Developer (Towed Systems) - Jun 21 2026/FreeRTOS Practice/fw-reference/build-arm/_deps/freertos_kernel-subbuild/freertos_kernel-populate-prefix/src/freertos_kernel-populate-stamp"
  "/Users/danielvalencia/Documents/Jobs/Kraken Robotics - Senior Firmware Developer (Towed Systems) - Jun 21 2026/FreeRTOS Practice/fw-reference/build-arm/_deps/freertos_kernel-subbuild/freertos_kernel-populate-prefix/src"
  "/Users/danielvalencia/Documents/Jobs/Kraken Robotics - Senior Firmware Developer (Towed Systems) - Jun 21 2026/FreeRTOS Practice/fw-reference/build-arm/_deps/freertos_kernel-subbuild/freertos_kernel-populate-prefix/src/freertos_kernel-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/danielvalencia/Documents/Jobs/Kraken Robotics - Senior Firmware Developer (Towed Systems) - Jun 21 2026/FreeRTOS Practice/fw-reference/build-arm/_deps/freertos_kernel-subbuild/freertos_kernel-populate-prefix/src/freertos_kernel-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/danielvalencia/Documents/Jobs/Kraken Robotics - Senior Firmware Developer (Towed Systems) - Jun 21 2026/FreeRTOS Practice/fw-reference/build-arm/_deps/freertos_kernel-subbuild/freertos_kernel-populate-prefix/src/freertos_kernel-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
