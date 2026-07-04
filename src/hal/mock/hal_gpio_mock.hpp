#pragma once

#include <gmock/gmock.h>
#include "hal/hal_gpio.hpp"

namespace hal::mock {

class MockGpio : public IGpio {
public:
    MOCK_METHOD(Status, configure, (uint8_t port, uint8_t pin, Mode mode), (override));
    MOCK_METHOD(Status, write, (uint8_t port, uint8_t pin, Level level), (override));
    MOCK_METHOD(Level,  read, (uint8_t port, uint8_t pin), (override));
    MOCK_METHOD(Status, toggle, (uint8_t port, uint8_t pin), (override));
};

}  // namespace hal::mock