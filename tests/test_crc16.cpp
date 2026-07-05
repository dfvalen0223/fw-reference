#include <gtest/gtest.h>
#include "util/crc16.hpp"
#include <array>

// CRC-16/CCITT-FALSE known check value.
// Source: CRC RevEng catalogue (https://reveng.sourceforge.io/crc-catalogue)
TEST(Crc16Test, KnownVector_123456789) {
    const std::array<uint8_t, 9> data = {
        '1', '2', '3', '4', '5', '6', '7', '8', '9'
    };
    constexpr uint16_t expected = 0x29B1;
    EXPECT_EQ(util::crc16_ccitt(data.data(), data.size()), expected);
}

// Empty input — should return init value unchanged.
TEST(Crc16Test, EmptyInputReturnsInitValue) {
    EXPECT_EQ(util::crc16_ccitt(nullptr, 0), 0xFFFF);
}

// Max payload size (64 bytes), just check it doesn't blow up.
TEST(Crc16Test, MaxPayload) {
    std::array<uint8_t, 64> data{};
    data.fill(0xA5);
    uint16_t result = util::crc16_ccitt(data.data(), 64);
    EXPECT_NE(result, 0xFFFF);
}

// Single byte, known result.
TEST(Crc16Test, SingleByte) {
    const uint8_t data = 0x42;
    EXPECT_EQ(util::crc16_ccitt(&data, 1), 0x8976);
}

// Null pointer with non-zero length — defensive check.
TEST(Crc16Test, NullWithLength) {
    EXPECT_EQ(util::crc16_ccitt(nullptr, 5), 0xFFFF);
}

// All zeros — verify algorithm handles burst of nulls gracefully.
TEST(Crc16Test, AllZeros) {
    std::array<uint8_t, 10> data{};
    data.fill(0);
    EXPECT_EQ(util::crc16_ccitt(data.data(), data.size()), 0xE139);
}
