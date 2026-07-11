#include "rs485_protocol.hpp"
#include "util/cobs.hpp"
#include "util/crc16.hpp"
#include <cstring>

namespace drivers {

Rs485Protocol::Rs485Protocol(hal::IRs485& rs485) : seq_(0), rs485_(rs485) {}

bool Rs485Protocol::init(uint32_t baud) {
    return rs485_.init(baud) == hal::Status::OK;
}

bool Rs485Protocol::send_frame(const uint8_t* payload, std::size_t len) {
    if (len > MAX_PAYLOAD || payload == nullptr) return false;

    // 1. Complete RWA PCKT: Type(1) + Seq(1) + Payload(len) + CRC(2)
    uint8_t raw_frame[3 + MAX_PAYLOAD + 2];
    raw_frame[0] = static_cast<uint8_t>(FrameType::DATA);
    raw_frame[1] = seq_;
    std::memcpy(&raw_frame[2], payload, len);

    // Compute CRC over raw_frame
    std::size_t metadata_plus_payload_len = 2 + len;
    uint16_t crc = compute_crc(raw_frame, metadata_plus_payload_len);
    raw_frame[metadata_plus_payload_len]     = static_cast<uint8_t>(crc >> 8);
    raw_frame[metadata_plus_payload_len + 1] = static_cast<uint8_t>(crc & 0xFF);

    std::size_t total_raw_len = metadata_plus_payload_len + 2;

    // 2. Wrap everything in COBs (Guarantees ZERO real 0x00 bytes on the serial cable)
    // The buffer size needs space for the SOF header, the worst case COBS, and the trailing zero.
    // 3: 
    uint8_t tx_buf[3 + MAX_PAYLOAD + 8]; 
    
    // SOF - Start Of Frame
    tx_buf[0] = SOF; 
    // COBS don't computes SOF
    std::size_t cobs_len = util::cobs_encode(raw_frame, total_raw_len, tx_buf + 1, sizeof(tx_buf) - 1);
    if (cobs_len == 0) return false;
    // we add 1 in the len to include SOF
    std::size_t total_bytes_to_send = 1 + cobs_len;

    // 3. Stop-and-Wait flow control with retries
    for (uint8_t attempt = 0; attempt <= MAX_RETRIES; attempt++) {
        if (!send_raw(tx_buf, total_bytes_to_send)) continue;
        if (wait_for_ack(seq_)) {
            seq_++; // Message delivered successfully, we're moving on to the next sequence
            return true;
        }
    }

    return false;
}

bool Rs485Protocol::recv_frame(uint8_t* payload, std::size_t& len) {
    uint8_t cobs_packet[256];
    std::size_t cobs_idx = 0;
    bool sof_found = false;

    // 1. Read from the cable byte by byte and put it into the COBS linear buffer
    while (cobs_idx < sizeof(cobs_packet)) {
        uint8_t byte;
        std::size_t n = 1;
        
        // We read byte by byte with a short timeout (10ms) to avoid freezing the FreeRTOS task
        if (rs485_.recv(&byte, n, 10) != hal::Status::OK) {
            return false; 
        }

        if (!sof_found) {
            if (byte == SOF) {
                sof_found = true; 
            }
            continue;
        }

        cobs_packet[cobs_idx++] = byte;

        if (byte == 0x00) {
            break;
        }
    }

    // Minimum size validation (Cobs overhead + Type + Seq + CRC_H + CRC_L + Delimiter 0x00)
    if (cobs_idx < 6) return false;

    // 2. Unpack COBS to recover the raw data. 
    //  raw_frame typical size data(128) + Type(1) + Seq-PCKT Number (1) + CRC (2) = (132 bytes) near power of 2: 256
    uint8_t raw_frame[256];

    std::size_t decoded_len = util::cobs_decode(cobs_packet, cobs_idx, raw_frame, sizeof(raw_frame));
    if (decoded_len < 4) return false;

    // 3. Verify the CRC of the decoded frame
    uint16_t rx_crc = (static_cast<uint16_t>(raw_frame[decoded_len - 2]) << 8) | raw_frame[decoded_len - 1];
    uint16_t calc_crc = compute_crc(raw_frame, decoded_len - 2);

    if (rx_crc != calc_crc) {
        // Send NACK response if the plot is corrupted
        // raw_frame[1]: # PCKT corrupt
        uint8_t nack_frame[4] = { static_cast<uint8_t>(FrameType::NACK), raw_frame[1], 0, 0 };
        uint16_t nc = compute_crc(nack_frame, 2);
        nack_frame[2] = static_cast<uint8_t>(nc >> 8);
        nack_frame[3] = static_cast<uint8_t>(nc & 0xFF);

        uint8_t resp[8] = { SOF };
        std::size_t r_len = util::cobs_encode(nack_frame, 4, resp + 1, sizeof(resp) - 1);
        send_raw(resp, 1 + r_len);
        return false;
    }

    // 4. If the frame is valid and of type DATA, we respond with an ACK
    FrameType type = static_cast<FrameType>(raw_frame[0]);
    uint8_t received_seq = raw_frame[1];

    if (type == FrameType::DATA) {
        uint8_t ack_frame[4] = { static_cast<uint8_t>(FrameType::ACK), received_seq, 0, 0 };
        uint16_t ac = compute_crc(ack_frame, 2);
        ack_frame[2] = static_cast<uint8_t>(ac >> 8);
        ack_frame[3] = static_cast<uint8_t>(ac & 0xFF);

        uint8_t resp[8] = { SOF };
        std::size_t r_len = util::cobs_encode(ack_frame, 4, resp + 1, sizeof(resp) - 1);
        send_raw(resp, 1 + r_len);

        // Copy the raw payload back to the telemetry application
        std::size_t payload_len = decoded_len - 4; // We subtract Type, Seq and the 2 bytes of CRC
        if (payload_len > len) return false; // Output size validation
        
        std::memcpy(payload, &raw_frame[2], payload_len);
        len = payload_len;
        return true;
    }

    return false;
}

bool Rs485Protocol::send_raw(const uint8_t* buf, std::size_t len) {
    return rs485_.send(buf, len, ACK_TIMEOUT_MS) == hal::Status::OK;
}

bool Rs485Protocol::recv_raw(uint8_t* buf, std::size_t& len) {
    return rs485_.recv(buf, len, ACK_TIMEOUT_MS) == hal::Status::OK;
}

uint16_t Rs485Protocol::compute_crc(const uint8_t* buf, std::size_t len) const {
    return util::crc16_ccitt(buf, len);
}

bool Rs485Protocol::wait_for_ack(uint8_t expected_seq) {
    uint8_t cobs_packet[16];
    std::size_t cobs_idx = 0;
    bool sof_found = false;

    // Listen to the bus, securely searching for the short response (ACK) byte by byte.
    while (cobs_idx < sizeof(cobs_packet)) {
        uint8_t byte;
        std::size_t n = 1;
        if (rs485_.recv(&byte, n, ACK_TIMEOUT_MS) != hal::Status::OK) return false;

        if (!sof_found) {
            if (byte == SOF) sof_found = true;
            continue;
        }

        cobs_packet[cobs_idx++] = byte;
        if (byte == 0x00) break;
    }

    uint8_t raw_frame[16];
    std::size_t decoded_len = util::cobs_decode(cobs_packet, cobs_idx, raw_frame, sizeof(raw_frame));
    if (decoded_len < 4) return false;

    // Extract CRC received
    uint16_t rx_crc = (static_cast<uint16_t>(raw_frame[decoded_len - 2]) << 8) | raw_frame[decoded_len - 1];
    // Compute CRC and compare with Extracted CRC 
    if (compute_crc(raw_frame, decoded_len - 2) != rx_crc) return false;
    // raw_frame[0] could be DATA, ACK, or NACK
    if (raw_frame[0] == static_cast<uint8_t>(FrameType::ACK) && raw_frame[1] == expected_seq) {
        return true;
    }

    return false;
}

}  // namespace drivers