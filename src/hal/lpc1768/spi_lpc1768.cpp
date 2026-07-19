#include "spi_lpc1768.hpp"

// SPI0 registers (UM10360 Table 360, base 0x40020000)
#define S0SPCR   (*reinterpret_cast<volatile uint32_t*>(0x40020000UL))
#define S0SPSR   (*reinterpret_cast<volatile uint32_t*>(0x40020004UL))
#define S0SPDR   (*reinterpret_cast<volatile uint32_t*>(0x40020008UL))
#define S0SPCCR  (*reinterpret_cast<volatile uint32_t*>(0x4002000CUL))

// S0SPCR bits (Table 361)
#define SPCR_BITENABLE (1UL << 2)
#define SPCR_CPHA      (1UL << 3)
#define SPCR_CPOL      (1UL << 4)
#define SPCR_MSTR      (1UL << 5)
#define SPCR_LSBF      (1UL << 6)
#define SPCR_SPIE      (1UL << 7)

// S0SPSR bits (Table 362)
#define SPSR_ABRT (1UL << 3)
#define SPSR_MODF (1UL << 4)
#define SPSR_ROVR (1UL << 5)
#define SPSR_WCOL (1UL << 6)
#define SPSR_SPIF (1UL << 7)

// Pin Connect Block (UM10360 chapter 8)
#define PINSEL0 (*reinterpret_cast<volatile uint32_t*>(0x4002C000UL))
#define PINSEL1 (*reinterpret_cast<volatile uint32_t*>(0x4002C004UL))

// Power control (UM10360 Table 46: PCSPI = bit 8, on by default at reset)
#define PCONP   (*reinterpret_cast<volatile uint32_t*>(0x400FC0C4UL))
#define PCSPI   (1UL << 8)

// GPIO0 (Table 101, base 0x2009C000)
#define FIO0DIR (*reinterpret_cast<volatile uint32_t*>(0x2009C000UL))
#define FIO0SET (*reinterpret_cast<volatile uint32_t*>(0x2009C018UL))
#define FIO0CLR (*reinterpret_cast<volatile uint32_t*>(0x2009C01CUL))

// Chip select: P0.16, software-driven GPIO (see header for rationale)
#define CS_PIN  16
#define CS_MASK (1UL << CS_PIN)

namespace hal::lpc1768 {

namespace {
constexpr uint32_t PCLK_SPI_HZ = 24000000;  // CCLK/4 default, PCLKSEL untouched
}

ISpi::Status SpiLpc1768::init(uint32_t clock_hz, uint8_t mode) {
    if (clock_hz == 0 || clock_hz > PCLK_SPI_HZ / 8 || mode > 3) {
        return Status::HW_ERROR;
    }

    PCONP |= PCSPI;

    // SCK0 = P0.15 alt-fn 11; MISO0 = P0.17, MOSI0 = P0.18, alt-fn 01.
    // P0.16 is deliberately left at its GPIO default (see header).
    PINSEL0 = (PINSEL0 & ~(0x3UL << 30)) | (0x3UL << 30);
    PINSEL1 = (PINSEL1 & ~((0x3UL << 2) | (0x3UL << 4)))
            | (0x1UL << 2) | (0x1UL << 4);

    // CS as GPIO output, idle high (deselected)
    FIO0DIR |= CS_MASK;
    FIO0SET = CS_MASK;

    // SPCCR = PCLK / desired clock, rounded to the nearest even value
    // >= 8 (both requirements per sec 17.7.4).
    uint32_t divisor = (PCLK_SPI_HZ + clock_hz - 1) / clock_hz;
    if (divisor < 8) divisor = 8;
    if (divisor & 1) divisor += 1;
    S0SPCCR = divisor;

    // mode: 0=CPOL0/CPHA0, 1=CPOL0/CPHA1, 2=CPOL1/CPHA0, 3=CPOL1/CPHA1
    uint32_t cr = SPCR_MSTR;  // 8 bits/transfer (BitEnable=0), MSB-first (LSBF=0)
    if (mode & 0x1) cr |= SPCR_CPHA;
    if (mode & 0x2) cr |= SPCR_CPOL;
    S0SPCR = cr;

    return Status::OK;
}

ISpi::Status SpiLpc1768::select_slave(uint8_t /*slave_index*/) {
    FIO0CLR = CS_MASK;
    return Status::OK;
}

ISpi::Status SpiLpc1768::deselect_slave(uint8_t /*slave_index*/) {
    FIO0SET = CS_MASK;
    return Status::OK;
}

ISpi::Status SpiLpc1768::transfer(const uint8_t* tx, uint8_t* rx,
                                  std::size_t len, uint32_t timeout_ms) {
    if (tx == nullptr || rx == nullptr) return Status::HW_ERROR;

    for (std::size_t i = 0; i < len; i++) {
        S0SPDR = tx[i];

        uint32_t elapsed = 0;
        while (!(S0SPSR & SPSR_SPIF)) {
            if (timeout_ms > 0 && ++elapsed > timeout_ms * 2000) {
                return Status::TIMEOUT;
            }
        }

        // Reading S0SPSR then S0SPDR clears SPIF (sec 17.7.2 note) and
        // retrieves this byte's received data in the same step.
        (void)S0SPSR;
        rx[i] = static_cast<uint8_t>(S0SPDR);
    }

    return Status::OK;
}

}  // namespace hal::lpc1768
