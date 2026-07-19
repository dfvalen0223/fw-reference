#pragma once

#include "hal/hal_spi.hpp"

namespace hal::lpc1768 {

class BitbangSpiLpc1768 : public ISpi {
public:
    Status init(uint32_t clock_hz, uint8_t mode) override;
    Status transfer(const uint8_t* tx, uint8_t* rx, std::size_t len,
                    uint32_t timeout_ms) override;
    Status select_slave(uint8_t slave_index) override;
    Status deselect_slave(uint8_t slave_index) override;

private:
    uint32_t delay_ns_{0};
    uint8_t cpol_{0};
    uint8_t cpha_{0};
    uint8_t pwron_loops_{0};

    void set_sck(int v);
    void set_mosi(int v);
    int  get_miso();
    void delay_half_period();
};

}  // namespace hal::lpc1768
