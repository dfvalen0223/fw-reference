#pragma once

#include <cstdint>
#include <cstddef>
#include "hal/hal_rs485.hpp"

namespace drivers {

class Rs485Protocol {
public:
    static constexpr std::size_t MAX_PAYLOAD = 128;
    static constexpr uint8_t SOF = 0x55;
    static constexpr uint8_t MAX_RETRIES = 3;
    static constexpr uint32_t ACK_TIMEOUT_MS = 100;

    enum class FrameType : uint8_t {
        DATA = 0x01,
        ACK  = 0x02,
        NACK = 0x03,
    };

    explicit Rs485Protocol(hal::IRs485& rs485);

    bool init(uint32_t baud);
    bool send_frame(const uint8_t* payload, std::size_t len);
    bool recv_frame(uint8_t* payload, std::size_t& len);

private:
    uint8_t seq_ = 0;
    hal::IRs485& rs485_;

    bool send_raw(const uint8_t* buf, std::size_t len);
    static uint16_t compute_crc(const uint8_t* buf, std::size_t len);
    bool wait_for_ack(uint8_t expected_seq);
};

}  // namespace drivers
