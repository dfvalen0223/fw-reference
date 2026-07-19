#include "bitbang_spi_lpc1768.hpp"

// GPIO2 registers (FIO2 base 0x2009C040)
#define FIO2DIR (*reinterpret_cast<volatile uint32_t*>(0x2009C040))
#define FIO2PIN (*reinterpret_cast<volatile uint32_t*>(0x2009C054))
#define FIO2SET (*reinterpret_cast<volatile uint32_t*>(0x2009C058))
#define FIO2CLR (*reinterpret_cast<volatile uint32_t*>(0x2009C05C))

// Pin Connect Block
#define PINSEL4 (*reinterpret_cast<volatile uint32_t*>(0x4002C010))

// pin assignments
#define SCK_PIN   0   // P2.0 = p26
#define MOSI_PIN  1   // P2.1 = p25
#define MISO_PIN  2   // P2.2 = p24
#define CS_PIN    3   // P2.3 = p23

#define SCK_MASK  (1UL << SCK_PIN)
#define MOSI_MASK (1UL << MOSI_PIN)
#define MISO_MASK (1UL << MISO_PIN)
#define CS_MASK   (1UL << CS_PIN)

namespace hal::lpc1768 {

void BitbangSpiLpc1768::set_sck(int v) {
    if (v) FIO2SET = SCK_MASK;
    else   FIO2CLR = SCK_MASK;
}

void BitbangSpiLpc1768::set_mosi(int v) {
    if (v) FIO2SET = MOSI_MASK;
    else   FIO2CLR = MOSI_MASK;
}

int BitbangSpiLpc1768::get_miso() {
    __asm volatile("" ::: "memory");
    return (FIO2PIN & MISO_MASK) ? 1 : 0;
}

void BitbangSpiLpc1768::delay_half_period() const {
    volatile uint32_t n = delay_ns_;
    while (n--) { __asm volatile("nop"); }
}

ISpi::Status BitbangSpiLpc1768::init(uint32_t clock_hz, uint8_t mode) {
    if (clock_hz == 0 || clock_hz > 4000000 || mode > 3) {
        return Status::HW_ERROR;
    }

    cpol_ = (mode >> 1) & 1;
    cpha_ = mode & 1;

    // Calibrate half-period delay for 96 MHz CPU.
    // Each loop iteration: NOP (1 cycle) + dec + branch (~3 cycles) ≈ 4 cycles.
    // half_period_ns = 500,000,000 / clock_hz  (in ns)
    // loop_iters = half_period_ns * 96 / 1000 / 4 = half_period_ns * 24 / 1000
    //            = (500,000,000 / clock_hz) * 24 / 1000
    //            = 12,000,000 / clock_hz
    uint32_t half_period_ns = 500000000UL / clock_hz;
    delay_ns_ = (half_period_ns * 24) / 1000;
    if (delay_ns_ < 1) delay_ns_ = 1;

    // P2.0-P2.3 as GPIO
    PINSEL4 &= ~((3UL << 0) | (3UL << 2) | (3UL << 4) | (3UL << 6));

    // SCK, MOSI, CS as outputs; MISO as input
    FIO2DIR |= (SCK_MASK | MOSI_MASK | CS_MASK);
    FIO2DIR &= ~MISO_MASK;

    // idle: SCK = CPOL, CS high (deselected)
    set_sck(cpol_);
    FIO2SET = CS_MASK;

    return Status::OK;
}

ISpi::Status BitbangSpiLpc1768::transfer(const uint8_t* tx, uint8_t* rx,
                                         std::size_t len, uint32_t timeout_ms) {
    if (tx == nullptr || rx == nullptr) return Status::HW_ERROR;

    (void)timeout_ms;  // bit-bang is deterministic; no timeout needed

    for (std::size_t byte_i = 0; byte_i < len; byte_i++) {
        uint8_t txb = tx[byte_i];
        uint8_t rxb = 0;

        for (int bit = 7; bit >= 0; bit--) {
            // prepare data before clock edge
            set_mosi((txb >> bit) & 1);

            if (cpha_ == 0) {
                // leading edge samples, trailing edge changes
                delay_half_period();
                set_sck(1 ^ cpol_);
                rxb = (uint8_t)((rxb << 1) | get_miso());
                delay_half_period();
                set_sck(cpol_);
            } else {
                // leading edge changes, trailing edge samples
                set_sck(1 ^ cpol_);
                delay_half_period();
                set_sck(cpol_);
                rxb = (uint8_t)((rxb << 1) | get_miso());
                delay_half_period();
            }
        }

        rx[byte_i] = rxb;
    }

    return Status::OK;
}

ISpi::Status BitbangSpiLpc1768::select_slave(uint8_t /*slave_index*/) {
    FIO2CLR = CS_MASK;
    return Status::OK;
}

ISpi::Status BitbangSpiLpc1768::deselect_slave(uint8_t /*slave_index*/) {
    FIO2SET = CS_MASK;
    return Status::OK;
}

}  // namespace hal::lpc1768
