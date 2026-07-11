#pragma once

#include <cstdint>
#include <cstddef>
#include "hal/hal_uart.hpp"

namespace hal {

class IRs485 {
public:
    virtual ~IRs485() = default;

    virtual Status init(uint32_t baud) = 0;
    virtual Status send(const uint8_t* data, std::size_t len,
                        uint32_t timeout_ms) = 0;
    virtual Status recv(uint8_t* data, std::size_t& len,
                        uint32_t timeout_ms) = 0;
};

}  // namespace hal
