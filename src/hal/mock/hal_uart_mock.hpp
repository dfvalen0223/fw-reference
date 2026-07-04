#pragma once

#include <gmock/gmock.h>
#include "hal/hal_uart.hpp"

namespace hal::mock {

/**
 * @brief Mock implementation of IUart for unit testing.
 *
 * MOCK_METHOD generates a fake implementation that:
 *   1. Records every call (so tests can verify with EXPECT_CALL)
 *   2. Returns whatever the test configures via WillOnce/WillRepeatedly
 *
 * Usage in tests:
 *   NiceMock<MockUart> mock;          // doesn't warn on unexpected calls
 *   EXPECT_CALL(mock, init(_))        // expect init() called with any arg
 *       .WillOnce(Return(Status::OK)); // first call returns OK
 */
class MockUart : public IUart {
public:
    // MOCK_METHOD(ReturnType, methodName, (argTypes), (qualifiers))
    MOCK_METHOD(Status, init, (BaudRate baud), (override));
    MOCK_METHOD(Status, transmit,
                (const uint8_t* data, std::size_t len, uint32_t timeout_ms),
                (override));
    MOCK_METHOD(Status, receive,
                (uint8_t* buf, std::size_t len, uint32_t timeout_ms),
                (override));
    MOCK_METHOD(std::size_t, bytes_available, (), (const, override));
};

}  // namespace hal::mock