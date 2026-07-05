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

// Edge-case Max Payload (64 bytes)
TEST(Crc16Test, MaxPayload) {
    std::array<uint8_t, 64> data{};
    data.fill(0xA5);
    // Should not crash, should return a consistent CRC
    uint16_t result = util::crc16_ccitt(data.data(), 64);
    // CRC of 64 bytes of 0xA5 — just verify it's not 0xFFFF (unmodified)
    EXPECT_NE(result, 0xFFFF);
}

// Edge-case Min Payload (1 Byte)
TEST(Crc16Test, SingleByte) {
    const uint8_t data = 0x42;
    // uint16_t result = util::crc16_ccitt(&data, 1);
    // CRC of single byte 0x42 — verify it's not 0xFFFF
    //EXPECT_NE(result, 0xFFFF);
    EXPECT_NE(util::crc16_ccitt(&data, 1), 0xFFFF);
}

// Edge-case Defensive FW, just in case a nullptr with size 5 pass to CRC. I Shouldn't happen a Fault segmentation (PC) or HardFeilt (MCU)
TEST(Crc16Test, NullWithLength) {
    // Passing nullptr with len > 0 should not crash
    // In practice this tests the function doesn't dereference unconditionally
    // uint16_t result = util::crc16_ccitt(nullptr, 5);
    EXPECT_EQ(util::crc16_ccitt(nullptr, 5), 0xFFFF);
    // Result should still be defined (CRC computation continues with whatever)
    // We just check it doesn't crash — pass if we reach here
    //SUCCEED();
}


// Edge-case Evaluate the algorithm's ability to detect bursts of null data or dropped communication lines.
TEST(Crc16Test, AllZeros) {
    std::array<uint8_t, 10> data{};
    data.fill(0);
    uint16_t result = util::crc16_ccitt(data.data(), data.size());
    // CRC of all zeros — known value, just verify != 0xFFFF
    // EXPECT_NE(result, 0xFFFF);
    // https://crccalc.com/?crc=00000000000000000000&method=CRC-16/IBM-3740&datatype=hex&outtype=hex
    EXPECT_EQ(util::crc16_ccitt(data.data(), data.size()), 0x2476);
}