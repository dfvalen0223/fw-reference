#pragma once

#include <gmock/gmock.h>
#include "hal/hal_rs485.hpp"

namespace hal::mock {

class MockRs485 : public IRs485 {
public:
    MOCK_METHOD(Status, init, (uint32_t baud), (override));
    MOCK_METHOD(Status, send,
                (const uint8_t* data, std::size_t len, uint32_t timeout_ms),
                (override));
    MOCK_METHOD(Status, recv,
                (uint8_t* data, std::size_t& len, uint32_t timeout_ms),
                (override));
};

}  // namespace hal::mock
