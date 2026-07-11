
if(NOT CMAKE_CROSSCOMPILING)
	message(STATUS "Setting up a Unit Test Suite (UTS)...")	
	
	# FetchContent downloads GoogleTest on first configure.
	# This makes the repo self-contained: no brew install needed.
	include(FetchContent)

	FetchContent_Declare(
	    googletest
	    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
	    DOWNLOAD_EXTRACT_TIMESTAMP TRUE # to prevent CMake from thinking that the file date is the version date
	)

	# For Windows: prevent overriding parent project's compiler settings
	set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
	set(INSTALL_GTEST OFF CACHE BOOL "" FORCE) # Prevent GoogleTest from trying to install on this PC

	FetchContent_MakeAvailable(googletest)

	include(GoogleTest)

	add_executable(unit_tests
	    test_crc16.cpp
	    test_telemetry_protocol.cpp
	    test_cobs.cpp
	    test_rs485_protocol.cpp
	)

	# Mock headers live under src/, tests need to find them
	target_include_directories(unit_tests PRIVATE
	    ${CMAKE_SOURCE_DIR}/src
	)

	target_link_libraries(unit_tests PRIVATE
	    drivers           # the library under test
	    hal_interface     # HAL headers (transitively via drivers, but explicit is clear)
	    GTest::gtest_main
	    GTest::gmock_main
	)

	# Auto-discover tests for ctest
	gtest_discover_tests(unit_tests)
else()
	message(STATUS "Cross-compilation detected for Target. Test suite skipped.")
endif()