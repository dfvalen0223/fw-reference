#include "telemetry_protocol.hpp"
#include "util/crc16.hpp"
#include <array>

namespace drivers {

bool TelemetryProtocol::init() {
    auto status = uart_.init(hal::IUart::BaudRate::BAUD_115200);
    initialized_ = (status == hal::Status::OK);
    return initialized_;
}

bool TelemetryProtocol::send_frame(const uint8_t* payload, std::size_t len) {
    // Guard clauses: check preconditions before touching hardware
    if (!initialized_) {
        return false;
    }
    if (len > MAX_PAYLOAD) {
        return false;
    }
    if (len > 0 && payload == nullptr) {
        return false;
    }

    // Build frame: [SOF][LEN][PAYLOAD][CRC16-HI][CRC16-LO]
    // Buffer size = SOF(1) + LEN(1) + MAX_PAYLOAD(64) + CRC(2) = 68
    std::array<uint8_t, 68> frame{};
    const std::size_t frame_len = 1 + 1 + len + 2;  // SOF + LEN + payload + CRC

    frame[0] = SOF;
    frame[1] = static_cast<uint8_t>(len);

    for (std::size_t i = 0; i < len; i++) {
        frame[2 + i] = payload[i];
    }

    // CRC computed over LEN byte + PAYLOAD (not SOF)
    const uint16_t crc = util::crc16_ccitt(&frame[1], 1 + len);
    frame[2 + len]     = static_cast<uint8_t>(crc >> 8);   // CRC high byte
    frame[3 + len]     = static_cast<uint8_t>(crc & 0xFF); // CRC low byte

    const auto status = uart_.transmit(frame.data(), frame_len, 1000);
    return (status == hal::Status::OK);
}

}  // namespace drivers