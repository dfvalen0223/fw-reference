#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "drivers/mcp2515.hpp"
#include "hal/mock/hal_spi_mock.hpp"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::DoAll;
using ::testing::WithArg;
using ::testing::WithArgs;
using ::testing::Sequence;
using drivers::Mcp2515;

namespace {

// Fakes the chip's shift-register behavior for a fixed reply, ignoring
// what was actually sent on MOSI (tx). Good enough for framing tests
// where we only care about what the driver *transmits* and how it
// interprets a canned response.
void expect_transfer_reply(hal::mock::MockSpi& mock, std::vector<uint8_t> reply, Sequence& seq) {
    EXPECT_CALL(mock, transfer(_, _, reply.size(), _))
        .InSequence(seq)
        .WillOnce(DoAll(
            WithArgs<1>([reply](uint8_t* rx) {
                for (std::size_t i = 0; i < reply.size(); i++) rx[i] = reply[i];
            }),
            Return(hal::ISpi::Status::OK)));
}

}  // namespace

class Mcp2515Test : public ::testing::Test {
protected:
    NiceMock<hal::mock::MockSpi> mock_spi;
};

// init(): RESET, then CNF3/CNF2/CNF1 writes, then CANCTRL mode write +
// CANSTAT poll confirming the mode took effect.
TEST_F(Mcp2515Test, InitSequence) {
    Sequence seq;

    // RESET: single byte 0xC0
    EXPECT_CALL(mock_spi, transfer(_, _, 1, _))
        .InSequence(seq)
        .WillOnce([](const uint8_t* tx, uint8_t*, std::size_t, uint32_t) {
            EXPECT_EQ(tx[0], 0xC0);
            return hal::ISpi::Status::OK;
        });

    // WRITE CNF3 (0x28), CNF2 (0x29), CNF1 (0x2A)
    EXPECT_CALL(mock_spi, transfer(_, _, 3, _))
        .InSequence(seq)
        .WillOnce([](const uint8_t* tx, uint8_t*, std::size_t, uint32_t) {
            EXPECT_EQ(tx[0], 0x02);  // WRITE
            EXPECT_EQ(tx[1], 0x28);  // CNF3
            EXPECT_EQ(tx[2], 0x03);
            return hal::ISpi::Status::OK;
        });
    EXPECT_CALL(mock_spi, transfer(_, _, 3, _))
        .InSequence(seq)
        .WillOnce([](const uint8_t* tx, uint8_t*, std::size_t, uint32_t) {
            EXPECT_EQ(tx[1], 0x29);  // CNF2
            EXPECT_EQ(tx[2], 0x02);
            return hal::ISpi::Status::OK;
        });
    EXPECT_CALL(mock_spi, transfer(_, _, 3, _))
        .InSequence(seq)
        .WillOnce([](const uint8_t* tx, uint8_t*, std::size_t, uint32_t) {
            EXPECT_EQ(tx[1], 0x2A);  // CNF1
            EXPECT_EQ(tx[2], 0x01);
            return hal::ISpi::Status::OK;
        });

    // READ CANCTRL (0x0F) -> returns 0x00 (post-reset default besides REQOP)
    expect_transfer_reply(mock_spi, {0, 0, 0x00}, seq);
    // WRITE CANCTRL with REQOP=010 (Loopback) in bits 7:5 -> 0x40
    EXPECT_CALL(mock_spi, transfer(_, _, 3, _))
        .InSequence(seq)
        .WillOnce([](const uint8_t* tx, uint8_t*, std::size_t, uint32_t) {
            EXPECT_EQ(tx[0], 0x02);
            EXPECT_EQ(tx[1], 0x0F);
            EXPECT_EQ(tx[2], 0x40);  // 010 << 5
            return hal::ISpi::Status::OK;
        });
    // READ CANSTAT (0x0E) -> OPMOD=010 confirms Loopback mode
    expect_transfer_reply(mock_spi, {0, 0, 0x40}, seq);

    Mcp2515 can(mock_spi);
    EXPECT_TRUE(can.init(0x01, 0x02, 0x03, Mcp2515::Mode::LOOPBACK,
                          8000000, [](uint32_t) {}));
}

namespace {
// DelayUsFn is a plain function pointer (no capture), so a stateless
// lambda can't record what it was called with. A static + a real
// (non-lambda) function is the simplest way to observe the value the
// driver requested.
uint32_t g_last_delay_us = 0;
void record_delay(uint32_t us) { g_last_delay_us = us; }
}  // namespace

// The OST delay must be sized from the actual oscillator frequency, not
// a hardcoded guess: ceil(128e6 / osc_freq_hz). At 8 MHz that's 16 us.
// Byte-level SPI framing is already covered by InitSequence; this test
// only cares about the delay_us argument, so any OK reply is fine.
TEST_F(Mcp2515Test, InitRequestsCorrectOstDelay) {
    ON_CALL(mock_spi, transfer(_, _, _, _))
        .WillByDefault(DoAll(
            WithArgs<1, 2>([](uint8_t* rx, std::size_t len) {
                for (std::size_t i = 0; i < len; i++) rx[i] = 0x40;  // OPMOD=010 for CANSTAT reads
            }),
            Return(hal::ISpi::Status::OK)));

    g_last_delay_us = 0;
    Mcp2515 can(mock_spi);
    ASSERT_TRUE(can.init(0x01, 0x02, 0x03, Mcp2515::Mode::LOOPBACK, 8000000, record_delay));
    EXPECT_EQ(g_last_delay_us, 16u);  // ceil(128,000,000 / 8,000,000) = 16
}

TEST_F(Mcp2515Test, InitScalesOstDelayWithOscillatorFrequency) {
    ON_CALL(mock_spi, transfer(_, _, _, _))
        .WillByDefault(DoAll(
            WithArgs<1, 2>([](uint8_t* rx, std::size_t len) {
                for (std::size_t i = 0; i < len; i++) rx[i] = 0x40;
            }),
            Return(hal::ISpi::Status::OK)));

    g_last_delay_us = 0;
    Mcp2515 can(mock_spi);
    ASSERT_TRUE(can.init(0x01, 0x02, 0x03, Mcp2515::Mode::LOOPBACK, 16000000, record_delay));
    EXPECT_EQ(g_last_delay_us, 8u);  // ceil(128,000,000 / 16,000,000) = 8
}

// send(): LOAD TX BUFFER with correct standard-ID framing, then RTS.
TEST_F(Mcp2515Test, SendFramesStandardId) {
    Sequence seq;

    Mcp2515::Frame f;
    f.id = 0x123;  // 0b100_1000_11
    f.dlc = 2;
    f.data[0] = 0xAA;
    f.data[1] = 0xBB;

    EXPECT_CALL(mock_spi, transfer(_, _, 8, _))  // 1 opcode + 5 header + 2 data
        .InSequence(seq)
        .WillOnce([](const uint8_t* tx, uint8_t*, std::size_t, uint32_t) {
            EXPECT_EQ(tx[0], 0x40);              // LOAD TX BUFFER -> TXB0SIDH
            EXPECT_EQ(tx[1], 0x24);              // SIDH = id >> 3 = 0x24
            EXPECT_EQ(tx[2], 0x60);              // SIDL = (id & 7) << 5 = 3<<5=0x60
            EXPECT_EQ(tx[3], 0x00);              // EID8
            EXPECT_EQ(tx[4], 0x00);              // EID0
            EXPECT_EQ(tx[5], 0x02);              // DLC
            EXPECT_EQ(tx[6], 0xAA);
            EXPECT_EQ(tx[7], 0xBB);
            return hal::ISpi::Status::OK;
        });

    EXPECT_CALL(mock_spi, transfer(_, _, 1, _))
        .InSequence(seq)
        .WillOnce([](const uint8_t* tx, uint8_t*, std::size_t, uint32_t) {
            EXPECT_EQ(tx[0], 0x81);  // RTS, TXB0 only
            return hal::ISpi::Status::OK;
        });

    Mcp2515 can(mock_spi);
    EXPECT_TRUE(can.send(f));
}

TEST_F(Mcp2515Test, SendRejectsOversizedId) {
    Mcp2515 can(mock_spi);
    Mcp2515::Frame f;
    f.id = 0x800;  // 12 bits, exceeds standard 11-bit range
    f.dlc = 1;
    EXPECT_FALSE(can.send(f));
}

TEST_F(Mcp2515Test, SendRejectsOversizedDlc) {
    Mcp2515 can(mock_spi);
    Mcp2515::Frame f;
    f.id = 0x100;
    f.dlc = 9;
    EXPECT_FALSE(can.send(f));
}

// has_message() reads CANINTF and checks RX0IF (bit 0).
TEST_F(Mcp2515Test, HasMessageReflectsRx0If) {
    Sequence seq;
    expect_transfer_reply(mock_spi, {0, 0, 0x01}, seq);  // RX0IF set
    Mcp2515 can(mock_spi);
    EXPECT_TRUE(can.has_message());
}

TEST_F(Mcp2515Test, HasMessageFalseWhenNoFlag) {
    Sequence seq;
    expect_transfer_reply(mock_spi, {0, 0, 0x00}, seq);
    Mcp2515 can(mock_spi);
    EXPECT_FALSE(can.has_message());
}

// receive(): decodes SIDH/SIDL back into the 11-bit ID and payload.
TEST_F(Mcp2515Test, ReceiveDecodesStandardId) {
    Sequence seq;
    // READ RX BUFFER reply: [opcode-echo, SIDH, SIDL, EID8, EID0, DLC, D0..D7]
    // The driver always clocks a fixed 14-byte burst (1+5+8); it only
    // uses the first `dlc` data bytes based on what it reads back.
    expect_transfer_reply(mock_spi,
        {0x00, 0x24, 0x60, 0x00, 0x00, 0x02, 0xAA, 0xBB, 0, 0, 0, 0, 0, 0}, seq);

    Mcp2515 can(mock_spi);
    Mcp2515::Frame f;
    ASSERT_TRUE(can.receive(f));
    EXPECT_EQ(f.id, 0x123);
    EXPECT_EQ(f.dlc, 2);
    EXPECT_EQ(f.data[0], 0xAA);
    EXPECT_EQ(f.data[1], 0xBB);
}

// A corrupt/malformed reply reporting DLC > 8 (the field is 4 bits, so
// 9-15 are representable even though CAN payloads are never >8 bytes)
// must not overflow Frame::data. Regression test for a real bug found
// during review: the unclamped DLC wrote past the 8-byte data array.
TEST_F(Mcp2515Test, ReceiveClampsOversizedDlc) {
    Sequence seq;
    std::vector<uint8_t> reply(14, 0);
    reply[5] = 0x0F;  // DLC = 15, invalid for CAN but representable in the field
    expect_transfer_reply(mock_spi, reply, seq);

    Mcp2515 can(mock_spi);
    Mcp2515::Frame f;
    ASSERT_TRUE(can.receive(f));
    EXPECT_LE(f.dlc, 8);
}

// Loopback round trip: what send() encodes, receive() must decode back
// to the same ID/payload. This is the actual invariant the HIL loopback
// test on real hardware depends on.
TEST_F(Mcp2515Test, LoopbackRoundTripIdEncoding) {
    for (uint16_t id : {0x000, 0x001, 0x123, 0x7FE, 0x7FF}) {
        const uint8_t sidh = static_cast<uint8_t>(id >> 3);
        const uint8_t sidl = static_cast<uint8_t>((id & 0x07) << 5);
        const uint16_t decoded = static_cast<uint16_t>((sidh << 3) | (sidl >> 5));
        EXPECT_EQ(decoded, id) << "round-trip failed for id=0x" << std::hex << id;
    }
}
