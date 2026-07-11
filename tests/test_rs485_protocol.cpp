#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cstring>
#include <vector>
#include "drivers/rs485_protocol.hpp"
#include "hal/mock/hal_rs485_mock.hpp"
#include "util/cobs.hpp"
#include "util/crc16.hpp"

using namespace drivers;
using namespace hal::mock;
using namespace testing;

namespace {

constexpr uint8_t SOF      = 0x55;
constexpr uint32_t T_ACK   = 100;
constexpr uint32_t T_BYTE  = 10;

std::vector<uint8_t> build_data_wire(uint8_t seq, const uint8_t* payload, std::size_t payload_len) {
    std::vector<uint8_t> raw(2 + payload_len + 2);
    raw[0] = static_cast<uint8_t>(Rs485Protocol::FrameType::DATA);
    raw[1] = seq;
    std::memcpy(&raw[2], payload, payload_len);
    uint16_t crc = util::crc16_ccitt(raw.data(), 2 + payload_len);
    raw[2 + payload_len]     = static_cast<uint8_t>(crc >> 8);
    raw[2 + payload_len + 1] = static_cast<uint8_t>(crc & 0xFF);

    uint8_t cobs_buf[256];
    std::size_t cobs_len = util::cobs_encode(
        raw.data(), raw.size(), cobs_buf, sizeof(cobs_buf));
    std::vector<uint8_t> wire(1 + cobs_len);
    wire[0] = SOF;
    std::memcpy(&wire[1], cobs_buf, cobs_len);
    return wire;
}

std::vector<uint8_t> build_ack_wire(uint8_t seq) {
    uint8_t raw[4];
    raw[0] = static_cast<uint8_t>(Rs485Protocol::FrameType::ACK);
    raw[1] = seq;
    uint16_t crc = util::crc16_ccitt(raw, 2);
    raw[2] = static_cast<uint8_t>(crc >> 8);
    raw[3] = static_cast<uint8_t>(crc & 0xFF);

    uint8_t cobs_buf[16];
    std::size_t cobs_len = util::cobs_encode(
        raw, sizeof(raw), cobs_buf, sizeof(cobs_buf));
    std::vector<uint8_t> wire(1 + cobs_len);
    wire[0] = SOF;
    std::memcpy(&wire[1], cobs_buf, cobs_len);
    return wire;
}

void expect_send_bytes(MockRs485& mock, const std::vector<uint8_t>& expected, uint32_t timeout_ms, Sequence& seq) {
    EXPECT_CALL(mock, send(_, expected.size(), timeout_ms))
        .InSequence(seq)
        .WillOnce(DoAll(
            WithArg<0>([expected](const uint8_t* buf) {
                EXPECT_EQ(std::memcmp(buf, expected.data(), expected.size()), 0);
            }),
            Return(hal::Status::OK)));
}

void expect_send_any(MockRs485& mock,
                     uint32_t timeout_ms,
                     Sequence& seq) {
    EXPECT_CALL(mock, send(_, _, timeout_ms))
        .InSequence(seq)
        .WillOnce(Return(hal::Status::OK));
}

void expect_recv_bytes(MockRs485& mock,
                       const std::vector<uint8_t>& bytes,
                       uint32_t timeout_ms,
                       Sequence& seq) {
    for (auto b : bytes) {
        EXPECT_CALL(mock, recv(_, _, timeout_ms))
            .InSequence(seq)
            .WillOnce(DoAll(
                WithArg<0>([b](uint8_t* buf) { buf[0] = b; }),
                SetArgReferee<1>(std::size_t{1}),
                Return(hal::Status::OK)));
    }
}

void expect_recv_timeout(MockRs485& mock,
                         uint32_t timeout_ms,
                         Sequence& seq) {
    EXPECT_CALL(mock, recv(_, _, timeout_ms))
        .InSequence(seq)
        .WillOnce(Return(hal::Status::TIMEOUT));
}

}  // namespace

class Rs485ProtocolTest : public Test {
protected:
    NiceMock<MockRs485> mock_rs485_;
    Rs485Protocol proto_{mock_rs485_};
};

TEST_F(Rs485ProtocolTest, InitOk) {
    EXPECT_CALL(mock_rs485_, init(115200))
        .WillOnce(Return(hal::Status::OK));
    EXPECT_TRUE(proto_.init(115200));
}

TEST_F(Rs485ProtocolTest, InitFails) {
    EXPECT_CALL(mock_rs485_, init(115200))
        .WillOnce(Return(hal::Status::HW_ERROR));
    EXPECT_FALSE(proto_.init(115200));
}

TEST_F(Rs485ProtocolTest, SendFrameOk) {
    uint8_t payload[] = {0x01, 0x02, 0x03};
    auto wire     = build_data_wire(0, payload, sizeof(payload));
    auto ack_wire = build_ack_wire(0);

    Sequence seq;
    expect_send_bytes(mock_rs485_, wire, T_ACK, seq);
    expect_recv_bytes(mock_rs485_, ack_wire, T_ACK, seq);

    EXPECT_TRUE(proto_.send_frame(payload, sizeof(payload)));
}

TEST_F(Rs485ProtocolTest, SendFrameNoAckRetries) {
    uint8_t payload[] = {0x01};

    Sequence seq;
    for (int i = 0; i <= Rs485Protocol::MAX_RETRIES; i++) {
        expect_send_any(mock_rs485_, T_ACK, seq);
        expect_recv_timeout(mock_rs485_, T_ACK, seq);
    }

    EXPECT_FALSE(proto_.send_frame(payload, sizeof(payload)));
}

TEST_F(Rs485ProtocolTest, SendFrameRetriesThenSucceeds) {
    uint8_t payload[] = {0x42};
    auto ack_wire = build_ack_wire(0);

    Sequence seq;
    for (int i = 0; i < Rs485Protocol::MAX_RETRIES; i++) {
        expect_send_any(mock_rs485_, T_ACK, seq);
        expect_recv_timeout(mock_rs485_, T_ACK, seq);
    }
    expect_send_any(mock_rs485_, T_ACK, seq);
    expect_recv_bytes(mock_rs485_, ack_wire, T_ACK, seq);

    EXPECT_TRUE(proto_.send_frame(payload, sizeof(payload)));
}

TEST_F(Rs485ProtocolTest, RejectsOverSizedPayload) {
    uint8_t big[Rs485Protocol::MAX_PAYLOAD + 1] = {};
    EXPECT_FALSE(proto_.send_frame(big, sizeof(big)));
}

TEST_F(Rs485ProtocolTest, RejectsNullPayload) {
    EXPECT_FALSE(proto_.send_frame(nullptr, 3));
}

TEST_F(Rs485ProtocolTest, RecvFrameOk) {
    uint8_t rx_payload[] = {0x01, 0x02, 0x03};
    auto data_wire = build_data_wire(5, rx_payload, sizeof(rx_payload));
    auto ack_wire  = build_ack_wire(5);

    Sequence seq;
    expect_recv_bytes(mock_rs485_, data_wire, T_BYTE, seq);
    expect_send_bytes(mock_rs485_, ack_wire, T_ACK, seq);

    uint8_t payload[128];
    std::size_t len = sizeof(payload);
    EXPECT_TRUE(proto_.recv_frame(payload, len));
    EXPECT_EQ(len, 3);
    EXPECT_EQ(payload[0], 0x01);
    EXPECT_EQ(payload[1], 0x02);
    EXPECT_EQ(payload[2], 0x03);
}
