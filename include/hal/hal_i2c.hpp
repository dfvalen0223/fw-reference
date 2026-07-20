#pragma once

#include <cstdint>
#include <cstddef>

namespace hal {

class II2c {
public:
    enum class Status { OK, TIMEOUT, HW_ERROR, NACK };

    virtual ~II2c() = default;

    virtual Status init(uint32_t clock_hz) = 0;

    /// Full transaction: START, 7-bit dev_addr+W, len bytes, STOP.
    virtual Status write(uint8_t dev_addr, const uint8_t* data,
                         std::size_t len) = 0;

    /// Full transaction: START, dev_addr+W, wlen bytes, repeated START,
    /// dev_addr+R, rlen bytes (NACK on last), STOP. This is the standard
    /// "write register pointer, then read" pattern used by the BMP280
    /// and ICM-20948 register maps.
    virtual Status write_read(uint8_t dev_addr, const uint8_t* wdata,
                              std::size_t wlen, uint8_t* rdata,
                              std::size_t rlen) = 0;
};

}  // namespace hal
