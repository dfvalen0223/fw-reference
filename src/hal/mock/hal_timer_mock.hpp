#pragma once

#include <gmock/gmock.h>
#include "hal/hal_timer.hpp"

namespace hal::mock {

class MockTimer : public ITimer {
public:
    MOCK_METHOD(Status, init, (uint32_t frequency_hz), (override));
    MOCK_METHOD(Status, start, (), (override));
    MOCK_METHOD(Status, stop, (), (override));
    MOCK_METHOD(Status, reset, (), (override));
    MOCK_METHOD(uint32_t, get_count, (), (const, override));
    MOCK_METHOD(Status, set_periodic_callback, (Callback cb), (override));
};

}  // namespace hal::mock
