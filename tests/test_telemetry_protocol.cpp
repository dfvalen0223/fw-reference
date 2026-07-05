#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "drivers/telemetry_protocol.hpp"
#include "hal/mock/hal_uart_mock.hpp"
#include "util/crc16.hpp"

using ::testing::_;
using ::testing::Return;
using ::testing::NiceMock;
using ::testing::ElementsAre;

class TelemetryProtocolTest : public ::testing::Test {
protected:
    NiceMock<hal::mock::MockUart> mock_uart;
};

// Init should return false if UART init fails.
TEST_F(TelemetryProtocolTest, InitFailsWhenUartInitFails) {
    EXPECT_CALL(mock_uart, init(hal::IUart::BaudRate::BAUD_115200))
        .WillOnce(Return(hal::Status::HW_ERROR));

    drivers::TelemetryProtocol proto(mock_uart);
    EXPECT_FALSE(proto.init());
}

// send_frame should reject anything over MAX_PAYLOAD.
TEST_F(TelemetryProtocolTest, RejectsOversizedPayload) {
    EXPECT_CALL(mock_uart, init(_))
        .WillOnce(Return(hal::Status::OK));

    drivers::TelemetryProtocol proto(mock_uart);
    ASSERT_TRUE(proto.init());

    std::array<uint8_t, drivers::TelemetryProtocol::MAX_PAYLOAD + 1> payload{};
    EXPECT_FALSE(proto.send_frame(payload.data(), payload.size()));
}

// Normal case: init OK, transmit OK, check the byte sequence.
TEST_F(TelemetryProtocolTest, SendFrameTransmitsCorrectByteSequence) {
    EXPECT_CALL(mock_uart, init(_))
        .WillOnce(Return(hal::Status::OK));
    drivers::TelemetryProtocol proto(mock_uart);
    ASSERT_TRUE(proto.init());

    const uint8_t payload[] = {0x42, 0xAB, 0xCD};

    uint8_t crc_data[] = {0x03, 0x42, 0xAB, 0xCD};
    uint16_t expected_crc = util::crc16_ccitt(crc_data, 4);
    uint8_t crc_hi = static_cast<uint8_t>(expected_crc >> 8);
    uint8_t crc_lo = static_cast<uint8_t>(expected_crc & 0xFF);

    EXPECT_CALL(mock_uart, transmit(_, 7, 1000))
        .WillOnce([crc_hi, crc_lo](const uint8_t* data, [[maybe_unused]] std::size_t len, [[maybe_unused]] uint32_t timeout) {
            EXPECT_EQ(data[0], 0xAA);
            EXPECT_EQ(data[1], 0x03);
            EXPECT_EQ(data[2], 0x42);
            EXPECT_EQ(data[3], 0xAB);
            EXPECT_EQ(data[4], 0xCD);
            EXPECT_EQ(data[5], crc_hi);
            EXPECT_EQ(data[6], crc_lo);
            return hal::Status::OK;
        });

    EXPECT_TRUE(proto.send_frame(payload, sizeof(payload)));
}

// Transmit timeout should propagate as failure.
TEST_F(TelemetryProtocolTest, SendFrameFailsOnTransmitTimeout) {
    EXPECT_CALL(mock_uart, init(_))
        .WillOnce(Return(hal::Status::OK));
    EXPECT_CALL(mock_uart, transmit(_, _, _))
        .WillOnce(Return(hal::Status::TIMEOUT));

    drivers::TelemetryProtocol proto(mock_uart);
    ASSERT_TRUE(proto.init());

    const uint8_t payload[] = {0x01};
    EXPECT_FALSE(proto.send_frame(payload, sizeof(payload)));
}

// Not initialized — should bail out before touching hardware.
TEST_F(TelemetryProtocolTest, SendFrameFailsIfNotInitialized) {
    drivers::TelemetryProtocol proto(mock_uart);
    const uint8_t payload[] = {0x01};
    EXPECT_FALSE(proto.send_frame(payload, sizeof(payload)));
}

// Zero-length payload: frame becomes SOF + LEN + CRC only.
TEST_F(TelemetryProtocolTest, SendFrameEmptyPayload) {
    EXPECT_CALL(mock_uart, init(_))
        .WillOnce(Return(hal::Status::OK));
    EXPECT_CALL(mock_uart, transmit(_, 4, 1000))
        .WillOnce([](const uint8_t* data, std::size_t /* len */, uint32_t) {
            EXPECT_EQ(data[0], 0xAA);
            EXPECT_EQ(data[1], 0x00);
            uint16_t expected_crc = util::crc16_ccitt(&data[1], 1);
            EXPECT_EQ(data[2], static_cast<uint8_t>(expected_crc >> 8));
            EXPECT_EQ(data[3], static_cast<uint8_t>(expected_crc & 0xFF));
            return hal::Status::OK;
        });

    drivers::TelemetryProtocol proto(mock_uart);
    ASSERT_TRUE(proto.init());
    EXPECT_TRUE(proto.send_frame(nullptr, 0));
}

// Max payload boundary — 64 bytes should go through.
TEST_F(TelemetryProtocolTest, SendFrameMaxPayload) {
    EXPECT_CALL(mock_uart, init(_))
        .WillOnce(Return(hal::Status::OK));
    EXPECT_CALL(mock_uart, transmit(_, 68, 1000))
        .WillOnce(Return(hal::Status::OK));

    std::array<uint8_t, drivers::TelemetryProtocol::MAX_PAYLOAD> payload{};
    payload.fill(0xFF);

    drivers::TelemetryProtocol proto(mock_uart);
    ASSERT_TRUE(proto.init());
    EXPECT_TRUE(proto.send_frame(payload.data(), payload.size()));
}

// Two calls to send_frame — both should succeed.
TEST_F(TelemetryProtocolTest, ConsecutiveSendFrame) {
    EXPECT_CALL(mock_uart, init(_))
        .WillOnce(Return(hal::Status::OK));
    EXPECT_CALL(mock_uart, transmit(_, _, 1000))
        .Times(2)
        .WillRepeatedly(Return(hal::Status::OK));

    const uint8_t payload[] = {0x01, 0x02};

    drivers::TelemetryProtocol proto(mock_uart);
    ASSERT_TRUE(proto.init());
    EXPECT_TRUE(proto.send_frame(payload, sizeof(payload)));
    EXPECT_TRUE(proto.send_frame(payload, sizeof(payload)));
}

// Init fails — send_frame should never call transmit.
TEST_F(TelemetryProtocolTest, SendFrameFailsWhenInitFails) {
    EXPECT_CALL(mock_uart, init(_))
        .WillOnce(Return(hal::Status::HW_ERROR));
    EXPECT_CALL(mock_uart, transmit(_, _, _))
        .Times(0);

    drivers::TelemetryProtocol proto(mock_uart);
    EXPECT_FALSE(proto.init());

    const uint8_t payload[] = {0x01};
    EXPECT_FALSE(proto.send_frame(payload, sizeof(payload)));
}
