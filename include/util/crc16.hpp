#pragma once

#include <cstdint>
#include <cstddef>

namespace util {

/**
 * @brief CRC-16 CCITT (poly 0x1021, init 0xFFFF, no reflection).
 *
 * Known check value: CRC of "123456789" (ASCII) = 0x29B1.
 * This is the CRC-16/CCITT-FALSE variant, widely used in
 * industrial and marine communication protocols.
 *
 * Implementation: bitwise (no lookup table, ~zero RAM overhead).
 * Suitable for constrained embedded targets (LPC1768).
 */
inline uint16_t crc16_ccitt(const uint8_t* data, std::size_t len) {
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