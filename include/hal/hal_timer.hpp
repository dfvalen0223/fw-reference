#pragma once

#include <cstdint>
#include <functional>

namespace hal {

class ITimer {
public:
    enum class Status { OK, INVALID_CONFIG, HW_ERROR };

    using Callback = std::function<void()>;

    virtual ~ITimer() = default;

    virtual Status init(uint32_t frequency_hz) = 0;
    virtual Status start() = 0;
    virtual Status stop() = 0;
    virtual Status reset() = 0;
    virtual uint32_t get_count() const = 0;
    virtual Status set_periodic_callback(Callback cb) = 0;
};

}  // namespace hal