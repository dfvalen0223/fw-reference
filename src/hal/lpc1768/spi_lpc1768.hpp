#pragma once

#include "hal/hal_spi.hpp"

namespace hal::lpc1768 {

/**
 * @brief LPC1768 legacy SPI0 implementation (UM10360 chapter 17).
 *
 * Pins: SCK0=P0.15, MISO0=P0.17, MOSI0=P0.18 (PINSEL0/1 alternate
 * function 01/11, confirmed against Table 79/80). Chip select is a
 * plain GPIO on P0.16 driven manually by select_slave/deselect_slave
 * — NOT the SSEL0 hardware pin function. Rationale: in SPI master
 * mode the datasheet requires the SSEL0 pin to stay inactive or the
 * peripheral raises a Mode Fault (sec 17.6.4); using a software GPIO
 * instead sidesteps that hazard entirely and matches how the rest of
 * this codebase handles chip-select-like signals (e.g. DE_PIN in
 * Rs485Lpc1768). P0.16 defaults to GPIO function (PINSEL reset = 00),
 * so it needs no PINSEL write at all.
 *
 * PCLK is left at its reset default (CCLK/4 = 24 MHz with the PLL
 * running) — PCLKSEL writes are avoided project-wide (see
 * rs485_lpc1768.cpp) after being found unreliable on this hardware.
 */
class SpiLpc1768 : public ISpi {
public:
    SpiLpc1768() = default;

    Status init(uint32_t clock_hz, uint8_t mode) override;
    Status transfer(const uint8_t* tx, uint8_t* rx, std::size_t len,
                    uint32_t timeout_ms) override;
    Status select_slave(uint8_t slave_index) override;
    Status deselect_slave(uint8_t slave_index) override;
};

}  // namespace hal::lpc1768
