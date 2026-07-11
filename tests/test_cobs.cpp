#include <gtest/gtest.h>
#include "util/cobs.hpp"

using namespace util;

TEST(CobsTest, EmptyInput) {
    uint8_t enc[16];
    std::size_t n = cobs_encode(nullptr, 0, enc, sizeof(enc));
    ASSERT_EQ(n, 2);
    EXPECT_EQ(enc[0], 0x01);
    EXPECT_EQ(enc[1], 0x00);

    uint8_t dec[16];
    n = cobs_decode(enc, n, dec, sizeof(dec));
    ASSERT_EQ(n, 0);
}

TEST(CobsTest, SingleByte) {
    const uint8_t in[] = {0x42};
    uint8_t enc[16];
    std::size_t n = cobs_encode(in, sizeof(in), enc, sizeof(enc));
    uint8_t dec[16];
    n = cobs_decode(enc, n, dec, sizeof(dec));
    ASSERT_EQ(n, 1);
    EXPECT_EQ(dec[0], 0x42);
}

TEST(CobsTest, SingleZero) {
    const uint8_t in[] = {0x00};
    uint8_t enc[16];
    std::size_t n = cobs_encode(in, sizeof(in), enc, sizeof(enc));
    uint8_t dec[16];
    n = cobs_decode(enc, n, dec, sizeof(dec));
    ASSERT_EQ(n, 1);
    EXPECT_EQ(dec[0], 0x00);
}

TEST(CobsTest, RoundTrip) {
    const uint8_t in[] = {0x01, 0x02, 0x00, 0x03, 0x00, 0x00, 0x04, 0x05};
    uint8_t enc[32];
    std::size_t e = cobs_encode(in, sizeof(in), enc, sizeof(enc));
    uint8_t dec[32];
    std::size_t d = cobs_decode(enc, e, dec, sizeof(dec));
    ASSERT_EQ(d, sizeof(in));
    for (std::size_t i = 0; i < sizeof(in); i++) {
        EXPECT_EQ(dec[i], in[i]) << " mismatch at " << i;
    }
}

TEST(CobsTest, AllZeros) {
    const uint8_t in[] = {0x00, 0x00, 0x00};
    uint8_t enc[32];
    std::size_t e = cobs_encode(in, sizeof(in), enc, sizeof(enc));
    uint8_t dec[32];
    std::size_t d = cobs_decode(enc, e, dec, sizeof(dec));
    ASSERT_EQ(d, sizeof(in));
    for (std::size_t i = 0; i < sizeof(in); i++) {
        EXPECT_EQ(dec[i], 0x00);
    }
}

TEST(CobsTest, NoZeros) {
    const uint8_t in[] = {0x11, 0x22, 0x33, 0x44};
    uint8_t enc[32];
    std::size_t e = cobs_encode(in, sizeof(in), enc, sizeof(enc));
    uint8_t dec[32];
    std::size_t d = cobs_decode(enc, e, dec, sizeof(dec));
    ASSERT_EQ(d, sizeof(in));
    for (std::size_t i = 0; i < sizeof(in); i++) {
        EXPECT_EQ(dec[i], in[i]);
    }
}

TEST(CobsTest, MaxBlock) {
    uint8_t in[254];
    for (std::size_t i = 0; i < sizeof(in); i++) in[i] = static_cast<uint8_t>(i + 1);
    uint8_t enc[512];
    std::size_t e = cobs_encode(in, sizeof(in), enc, sizeof(enc));
    uint8_t dec[512];
    std::size_t d = cobs_decode(enc, e, dec, sizeof(dec));
    ASSERT_EQ(d, sizeof(in));
    for (std::size_t i = 0; i < sizeof(in); i++) {
        EXPECT_EQ(dec[i], in[i]);
    }
}

TEST(CobsTest, BufferOverflowEncode) {
    const uint8_t in[] = {0x01, 0x02};
    uint8_t tiny[1];
    std::size_t n = cobs_encode(in, sizeof(in), tiny, sizeof(tiny));
    EXPECT_EQ(n, 0);
}

TEST(CobsTest, BufferOverflowDecode) {
    const uint8_t in[] = {0x01, 0x02, 0x00};
    uint8_t tiny[1];
    std::size_t n = cobs_decode(in, sizeof(in), tiny, sizeof(tiny));
    EXPECT_EQ(n, 0);
}

TEST(CobsTest, NoTerminator) {
    const uint8_t in[] = {0x01, 0x02, 0x03};
    uint8_t dec[16];
    std::size_t n = cobs_decode(in, sizeof(in), dec, sizeof(dec));
    EXPECT_EQ(n, 0);
}

TEST(CobsTest, ZeroCode) {
    const uint8_t in[] = {0x00, 0x01, 0x00};
    uint8_t dec[16];
    std::size_t n = cobs_decode(in, sizeof(in), dec, sizeof(dec));
    EXPECT_EQ(n, 0);
}
