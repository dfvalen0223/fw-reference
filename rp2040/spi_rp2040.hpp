#pragma once

#include "hal/hal_spi.hpp"

// RP2040 SPI0 HAL, Waveshare RP2040-Zero.
//
// Pins (verified against RP2040 datasheet GPIO function table, mod-4
// pattern for SPI0/SPI1, cross-checked with Mischianti's RP2040-Zero
// pinout): SCK=GP2, MOSI=GP3, MISO=GP4, CS=GP5 (manual GPIO, not the
// hardware CS — Pico SDK's hardware_spi doesn't auto-drive CS in
// master mode, every example controls it as a plain GPIO, same
// pattern already used on the LPC1768 side).
//
// GP16 deliberately avoided: that's the onboard WS2812 status LED
// pin used elsewhere in this project (rp2040/protocol_responder.cpp).

class SpiRp2040 : public hal::ISpi {
public:
    Status init(uint32_t clock_hz, uint8_t mode) override;
    Status transfer(const uint8_t* tx, uint8_t* rx, std::size_t len,
                    uint32_t timeout_ms) override;
    Status select_slave(uint8_t slave_index) override;
    Status deselect_slave(uint8_t slave_index) override;
};
