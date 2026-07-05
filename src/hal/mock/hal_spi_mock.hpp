#pragma once

#include <gmock/gmock.h>
#include "hal/hal_spi.hpp"

namespace hal::mock {

class MockSpi : public ISpi {
public:
    MOCK_METHOD(Status, init, (uint32_t clock_hz, uint8_t mode), (override));
    MOCK_METHOD(Status, transfer,
                (const uint8_t* tx, uint8_t* rx, std::size_t len, uint32_t timeout_ms),
                (override));
    MOCK_METHOD(Status, select_slave, (uint8_t slave_index), (override));
    MOCK_METHOD(Status, deselect_slave, (uint8_t slave_index), (override));
};

}  // namespace hal::mock
