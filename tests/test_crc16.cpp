#include <gtest/gtest.h>
#include "util/crc16.hpp"
#include <array>

// CRC-16/CCITT-FALSE canonical check value.
// Source: CRC RevEng catalogue (https://reveng.sourceforge.io/crc-catalogue)
// This is the standard test vector for CRC-16/CCITT-FALSE.
TEST(Crc16Test, KnownVector_123456789) {
    const std::array<uint8_t, 9> data = {
        '1', '2', '3', '4', '5', '6', '7', '8', '9'
    };
    constexpr uint16_t expected = 0x29B1;
    EXPECT_EQ(util::crc16_ccitt(data.data(), data.size()), expected);
}

// Empty input should return the init value (0xFFFF) unchanged.
TEST(Crc16Test, EmptyInputReturnsInitValue) {
    EXPECT_EQ(util::crc16_ccitt(nullptr, 0), 0xFFFF);
}