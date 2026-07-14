#include "rs485_lpc1768.hpp"

// UART2 registers (base 0x40098000)
#define U2RBR    (*reinterpret_cast<volatile uint8_t*>(0x40098000))
#define U2THR    (*reinterpret_cast<volatile uint8_t*>(0x40098000))
#define U2DLL    (*reinterpret_cast<volatile uint8_t*>(0x40098000))
#define U2DLM    (*reinterpret_cast<volatile uint8_t*>(0x40098004))
#define U2FCR    (*reinterpret_cast<volatile uint8_t*>(0x40098008))
#define U2LCR    (*reinterpret_cast<volatile uint8_t*>(0x4009800C))
#define U2LSR    (*reinterpret_cast<volatile uint8_t*>(0x40098014))
#define U2FDR    (*reinterpret_cast<volatile uint8_t*>(0x40098028))

// UART2 LSR bits
#define LSR_THRE  (1 << 5)
#define LSR_TEMT  (1 << 6)
#define LSR_RDR   (1 << 0)

// GPIO0 base (Fast GPIO, FIO0DIR = 0x2009C000)
#define FIO0DIR   (*reinterpret_cast<volatile uint32_t*>(0x2009C000UL))
#define FIO0SET   (*reinterpret_cast<volatile uint32_t*>(0x2009C018UL))
#define FIO0CLR   (*reinterpret_cast<volatile uint32_t*>(0x2009C01CUL))

// DE control pin
#define DE_PIN    9
#define DE_MASK   (1UL << DE_PIN)

// Pin connect block
#define PINSEL0   (*reinterpret_cast<volatile uint32_t*>(0x4002C000))

// Peripheral clock control — UART2 is PCLKSEL1 bits 17:16 (UM10360 Table 42)
#define PCLKSEL1  (*reinterpret_cast<volatile uint32_t*>(0x400FC1AC))

// Power control — UART2 is PCONP bit 24 (off at reset, must enable first)
#define PCONP     (*reinterpret_cast<volatile uint32_t*>(0x400FC0C4))
#define PCUART2   (1UL << 24)

namespace hal::lpc1768 {

// UART2 peripheral clock: reset default PCLK = CCLK/4 with CCLK = 96 MHz
// from SystemInit(). PCLKSEL is deliberately not used (unreliable on this
// part) — if the PLL fell back to IRC, baud will be off by 24x.
static constexpr uint32_t PCLK_HZ = 24000000;

// Busy-wait iterations per millisecond. Rough calibration: 96 MHz core,
// -O0 poll loop of ~40 cycles. Recalibrate if the core clock changes.
static constexpr uint32_t TICKS_PER_MS = 2000;

Rs485Lpc1768::Rs485Lpc1768() {}

Status Rs485Lpc1768::init(uint32_t baud) {
    if (baud == 0) return Status::HW_ERROR;

    // Integer divisor from 24 MHz PCLK. 115200 -> DL=13 (actual 115385,
    // +0.16%); 9600 -> DL=156 (+0.16%). Both well within UART tolerance.
    const uint32_t dl = PCLK_HZ / (16 * baud);
    if (dl == 0 || dl > 0xFFFF) return Status::HW_ERROR;

    // Power on UART2 before touching any of its registers (off at reset)
    PCONP |= PCUART2;

    // Configure P0.10 as TX (01), P0.11 as RX (01)
    PINSEL0 = (PINSEL0 & ~(0x0F << 20)) | (0x01 << 20) | (0x01 << 22);

    // Configure DE pin (P0.9) as GPIO output
    PINSEL0 &= ~(0x3 << 18);
    FIO0DIR |= DE_MASK;
    set_rx_mode();

    U2LCR = 0x83;             // DLAB + 8N1
    U2DLL = static_cast<uint8_t>(dl & 0xFF);
    U2DLM = static_cast<uint8_t>((dl >> 8) & 0xFF);
    U2FDR = 0x10;             // fractional divider off (MulVal=1, DivAddVal=0)
    U2LCR = 0x03;             // clear DLAB
    U2FCR = 0x07;             // enable + reset FIFOs

    return Status::OK;
}

Status Rs485Lpc1768::send(const uint8_t* data, std::size_t len, uint32_t timeout_ms) {
    if (data == nullptr || len == 0) return Status::HW_ERROR;

    set_tx_mode();

    for (std::size_t i = 0; i < len; i++) {
        uint32_t elapsed = 0;
        while (!(U2LSR & LSR_THRE)) {
            if (timeout_ms > 0 && ++elapsed > timeout_ms * TICKS_PER_MS) {
                set_rx_mode();
                return Status::TIMEOUT;
            }
        }
        write_byte(data[i]);
    }

    while (!(U2LSR & LSR_TEMT)) {
        // small busy-wait for TX complete
    }

    set_rx_mode();
    return Status::OK;
}

Status Rs485Lpc1768::recv(uint8_t* data, std::size_t& len, uint32_t timeout_ms) {
    if (data == nullptr) return Status::HW_ERROR;

    set_rx_mode();
    std::size_t count = 0;

    if (len == 0) {
        len = 0;
        return Status::OK;
    }

    uint32_t elapsed = 0;

    while (count < len) {
        if (rx_available()) {
            data[count++] = read_byte();
            elapsed = 0;
        } else {
            if (timeout_ms > 0 && ++elapsed > timeout_ms * TICKS_PER_MS) {
                break;
            }
        }
    }

    len = count;
    return (count > 0) ? Status::OK : Status::TIMEOUT;
}

void Rs485Lpc1768::set_tx_mode() {
    FIO0SET = DE_MASK;
}

void Rs485Lpc1768::set_rx_mode() {
    FIO0CLR = DE_MASK;
}

bool Rs485Lpc1768::rx_available() {
    return (U2LSR & LSR_RDR) != 0;
}

uint8_t Rs485Lpc1768::read_byte() {
    return U2RBR;
}

void Rs485Lpc1768::write_byte(uint8_t b) {
    U2THR = b;
}

}  // namespace hal::lpc1768
