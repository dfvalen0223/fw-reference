#pragma once

#include <cstdint>

namespace hal {

class IGpio {
public:
    enum class Level : uint8_t { LOW = 0, HIGH = 1 };
    enum class Mode : uint8_t { INPUT, OUTPUT, ALTERNATE, ANALOG };
    enum class Status { OK, INVALID_PIN, HW_ERROR };

    virtual ~IGpio() = default;

    virtual Status configure(uint8_t port, uint8_t pin, Mode mode) = 0;
    virtual Status write(uint8_t port, uint8_t pin, Level level) = 0;
    virtual Level  read(uint8_t port, uint8_t pin) = 0;
    virtual Status toggle(uint8_t port, uint8_t pin) = 0;
};

}  // namespace hal