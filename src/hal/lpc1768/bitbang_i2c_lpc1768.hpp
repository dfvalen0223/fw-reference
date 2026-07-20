#pragma once

#include "hal/hal_i2c.hpp"

namespace hal::lpc1768 {

// Software (bit-banged) I2C on plain GPIOs, LPC1768.
//
// Pins: SCL=P2.4, SDA=P2.5 (next free pair after the SPI bit-bang HAL's
// P2.0-P2.3). Both lines are driven open-drain by switching direction
// (input = released/high via external pull-up, output-low = driven low)
// rather than relying on true open-drain hardware mode, matching the
// bit-bang SPI HAL's approach of avoiding peripheral-specific quirks.
//
// Supports multi-master clock stretching: after releasing SCL, the
// driver polls the pin and waits for the slave to let it go high before
// continuing, instead of assuming a fixed high time.
class BitbangI2cLpc1768 : public II2c {
public:
    Status init(uint32_t clock_hz) override;
    Status write(uint8_t dev_addr, const uint8_t* data,
                std::size_t len) override;
    Status write_read(uint8_t dev_addr, const uint8_t* wdata,
                      std::size_t wlen, uint8_t* rdata,
                      std::size_t rlen) override;

private:
    uint32_t delay_ns_{0};

    static void sda_low();
    static void sda_release();
    static void scl_low();
    // Releases SCL and waits (with clock stretching) for it to read high.
    // Returns false on timeout (stuck-low bus).
    bool scl_release_and_wait() const;
    static int sda_read();

    void start() const;
    void stop() const;
    void delay_half_period() const;

    // Returns true if the slave ACKed (pulled SDA low on the 9th clock).
    bool write_byte(uint8_t b) const;
    uint8_t read_byte(bool send_ack) const;
};

}  // namespace hal::lpc1768
