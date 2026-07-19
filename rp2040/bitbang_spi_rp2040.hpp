#pragma once

#include "hal/hal_spi.hpp"

// Software (bit-banged) SPI on plain GPIOs, RP2040-Zero.
//
// Used instead of the Pico SDK hardware_spi peripheral after a live
// logic-analyzer capture showed that HAL returning inconsistent data
// on rapid back-to-back short transfers (reading the same static
// register twice with no delay gave two different values every time).
// Bit-banging avoids that peripheral/FIFO behavior entirely. This
// mirrors BitbangSpiLpc1768 (src/hal/lpc1768/bitbang_spi_lpc1768) —
// same technique, adapted to Pico SDK gpio_* calls.
//
// Pins: SCK=GP2, MOSI=GP3, MISO=GP4, CS=GP5. GP16 stays reserved for
// the WS2812 status LED.

class BitbangSpiRp2040 : public hal::ISpi {
public:
    Status init(uint32_t clock_hz, uint8_t mode) override;
    Status transfer(const uint8_t* tx, uint8_t* rx, std::size_t len,
                    uint32_t timeout_ms) override;
    Status select_slave(uint8_t slave_index) override;
    Status deselect_slave(uint8_t slave_index) override;

private:
    uint32_t half_period_ns_{0};
    uint8_t cpol_{0};
    uint8_t cpha_{0};
};
