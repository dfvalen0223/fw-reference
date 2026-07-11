#pragma once

#include "hal/hal_rs485.hpp"

namespace hal::lpc1768 {

class Rs485Lpc1768 : public IRs485 {
public:
    Rs485Lpc1768();
    Status init(uint32_t baud) override;
    Status send(const uint8_t* data, std::size_t len,
                uint32_t timeout_ms) override;
    Status recv(uint8_t* data, std::size_t& len,
                uint32_t timeout_ms) override;

private:
    void set_tx_mode();
    void set_rx_mode();
    bool tx_complete();
    bool rx_available();
    uint8_t read_byte();
    void write_byte(uint8_t b);
};

}  // namespace hal::lpc1768
