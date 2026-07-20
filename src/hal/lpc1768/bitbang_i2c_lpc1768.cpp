#include "bitbang_i2c_lpc1768.hpp"

// GPIO2 registers (FIO2 base 0x2009C040) - same port as the bit-bang SPI
// HAL, different pins.
#define FIO2DIR (*reinterpret_cast<volatile uint32_t*>(0x2009C040))
#define FIO2PIN (*reinterpret_cast<volatile uint32_t*>(0x2009C054))
#define FIO2SET (*reinterpret_cast<volatile uint32_t*>(0x2009C058))
#define FIO2CLR (*reinterpret_cast<volatile uint32_t*>(0x2009C05C))

// Pin Connect Block
#define PINSEL4 (*reinterpret_cast<volatile uint32_t*>(0x4002C010))

// pin assignments (next free pair after SPI's P2.0-P2.3)
#define SCL_PIN 4  // P2.4 = p22
#define SDA_PIN 5  // P2.5 = p21

#define SCL_MASK (1UL << SCL_PIN)
#define SDA_MASK (1UL << SDA_PIN)

// Bus-stuck timeout for clock-stretch wait: generous upper bound on loop
// iterations, not a calibrated time value.
#define STRETCH_TIMEOUT_LOOPS 100000UL

namespace hal::lpc1768 {

void BitbangI2cLpc1768::sda_low() {
    FIO2DIR |= SDA_MASK;   // drive
    FIO2CLR = SDA_MASK;    // low
}

void BitbangI2cLpc1768::sda_release() {
    FIO2DIR &= ~SDA_MASK;  // input = released, pulled high externally
}

void BitbangI2cLpc1768::scl_low() {
    FIO2DIR |= SCL_MASK;
    FIO2CLR = SCL_MASK;
}

int BitbangI2cLpc1768::sda_read() {
    __asm volatile("" ::: "memory");
    return (FIO2PIN & SDA_MASK) ? 1 : 0;
}

bool BitbangI2cLpc1768::scl_release_and_wait() {
    FIO2DIR &= ~SCL_MASK;  // release, pulled high externally
    uint32_t timeout = STRETCH_TIMEOUT_LOOPS;
    while (!(FIO2PIN & SCL_MASK)) {
        if (--timeout == 0) return false;  // slave holding SCL low forever
    }
    return true;
}

void BitbangI2cLpc1768::delay_half_period() const {
    volatile uint32_t n = delay_ns_;
    while (n--) { __asm volatile("nop"); }
}

void BitbangI2cLpc1768::start() const {
    sda_release();
    scl_release_and_wait();
    delay_half_period();
    sda_low();
    delay_half_period();
    scl_low();
}

void BitbangI2cLpc1768::stop() const {
    sda_low();
    delay_half_period();
    scl_release_and_wait();
    delay_half_period();
    sda_release();
    delay_half_period();
}

bool BitbangI2cLpc1768::write_byte(uint8_t b) const {
    for (int bit = 7; bit >= 0; bit--) {
        if ((b >> bit) & 1) sda_release();
        else                sda_low();
        delay_half_period();
        scl_release_and_wait();
        delay_half_period();
        scl_low();
    }

    // 9th clock: slave ACKs by pulling SDA low.
    sda_release();
    delay_half_period();
    scl_release_and_wait();
    bool ack = (sda_read() == 0);
    delay_half_period();
    scl_low();
    return ack;
}

uint8_t BitbangI2cLpc1768::read_byte(bool send_ack) const {
    uint8_t value = 0;
    sda_release();

    for (int bit = 7; bit >= 0; bit--) {
        delay_half_period();
        scl_release_and_wait();
        value = (uint8_t)((value << 1) | sda_read());
        delay_half_period();
        scl_low();
    }

    // 9th clock: drive ACK (0) or NACK (1, i.e. release) to the slave.
    if (send_ack) sda_low();
    else          sda_release();
    delay_half_period();
    scl_release_and_wait();
    delay_half_period();
    scl_low();
    sda_release();

    return value;
}

II2c::Status BitbangI2cLpc1768::init(uint32_t clock_hz) {
    if (clock_hz == 0 || clock_hz > 400000) {
        return Status::HW_ERROR;
    }

    // Same calibration approach as the bit-bang SPI HAL: nop-loop tuned
    // for 96 MHz CCLK. half_period_ns = 500,000,000 / clock_hz;
    // loop_iters ~= half_period_ns * 24 / 1000 (see bitbang_spi_lpc1768.cpp
    // for the cycle-accounting derivation).
    uint32_t half_period_ns = 500000000UL / clock_hz;
    delay_ns_ = (half_period_ns * 24) / 1000;
    if (delay_ns_ < 1) delay_ns_ = 1;

    // P2.4, P2.5 as GPIO
    PINSEL4 &= ~((3UL << 8) | (3UL << 10));

    // Idle: both lines released (pulled high externally).
    sda_release();
    FIO2DIR &= ~SCL_MASK;

    return Status::OK;
}

II2c::Status BitbangI2cLpc1768::write(uint8_t dev_addr, const uint8_t* data,
                                      std::size_t len) {
    if (data == nullptr && len > 0) return Status::HW_ERROR;

    start();
    if (!write_byte((uint8_t)(dev_addr << 1))) {
        stop();
        return Status::NACK;
    }

    for (std::size_t i = 0; i < len; i++) {
        if (!write_byte(data[i])) {
            stop();
            return Status::NACK;
        }
    }

    stop();
    return Status::OK;
}

II2c::Status BitbangI2cLpc1768::write_read(uint8_t dev_addr,
                                           const uint8_t* wdata,
                                           std::size_t wlen, uint8_t* rdata,
                                           std::size_t rlen) {
    if ((wdata == nullptr && wlen > 0) || (rdata == nullptr && rlen > 0)) {
        return Status::HW_ERROR;
    }

    start();
    if (!write_byte((uint8_t)(dev_addr << 1))) {
        stop();
        return Status::NACK;
    }
    for (std::size_t i = 0; i < wlen; i++) {
        if (!write_byte(wdata[i])) {
            stop();
            return Status::NACK;
        }
    }

    // Repeated START into read mode.
    start();
    if (!write_byte((uint8_t)((dev_addr << 1) | 1))) {
        stop();
        return Status::NACK;
    }
    for (std::size_t i = 0; i < rlen; i++) {
        bool last = (i == rlen - 1);
        rdata[i] = read_byte(!last);  // ACK all but the final byte
    }

    stop();
    return Status::OK;
}

}  // namespace hal::lpc1768
