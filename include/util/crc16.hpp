#pragma once

#include <cstdint>
#include <cstddef>

namespace util {

// CRC-16 CCITT (poly 0x1021, init 0xFFFF, no reflection).
// Known check value: CRC of "123456789" = 0x29B1.
// Bitwise implementation — no lookup table, zero RAM overhead.
// Good for constrained targets like the LPC1768.
inline uint16_t crc16_ccitt(const uint8_t* data, std::size_t len) {
    if (data == nullptr) return 0xFFFF;
    uint16_t crc = 0xFFFF;
    for (std::size_t i = 0; i < len; i++) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

}  // namespace util
