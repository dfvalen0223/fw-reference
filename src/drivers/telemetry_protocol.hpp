#pragma once

#include "hal/hal_uart.hpp"
#include <cstdint>
#include <cstddef>

namespace drivers {

/**
 * @brief Robust telemetry frame protocol over UART.
 *
 * Frame format:
 *   [SOF=0xAA][LEN(1)][PAYLOAD(LEN bytes)][CRC16(2, big-endian)]
 *
 * - CRC-16 CCITT computed over LEN + PAYLOAD (not SOF, not CRC itself)
 * - Receiver validates CRC; corrupt frames silently discarded
 * - Max payload: 64 bytes (keeps frame buffer small for constrained targets)
 *
 * Dependencies: depends on hal::IUart (abstract), not on concrete hardware.
 * This is what enables SIL testing: inject a MockUart in tests, real UART in prod.
 */
class TelemetryProtocol {
public:
    static constexpr std::size_t MAX_PAYLOAD = 64;
    static constexpr uint8_t SOF = 0xAA;

    /// Constructor: takes a UART reference (dependency injection).
    explicit TelemetryProtocol(hal::IUart& uart) : uart_(uart) {}

    /// Initialize underlying UART at 115200 baud. Returns false on failure.
    bool init();

    /// Encode and transmit a frame with CRC. Returns false on failure.
    bool send_frame(const uint8_t* payload, std::size_t len);

private:
    hal::IUart& uart_;
    bool initialized_ = false;
};

}  // namespace drivers