#pragma once

#include <cstdint>
#include <cstddef>

namespace hal {

class ISpi {
public:
    enum class Status { OK, TIMEOUT, HW_ERROR, BUSY };

    virtual ~ISpi() = default;

    virtual Status init(uint32_t clock_hz, uint8_t mode) = 0;
    virtual Status transfer(const uint8_t* tx, uint8_t* rx,
                            std::size_t len, uint32_t timeout_ms) = 0;
    virtual Status select_slave(uint8_t slave_index) = 0;
    virtual Status deselect_slave(uint8_t slave_index) = 0;
};

}  // namespace hal