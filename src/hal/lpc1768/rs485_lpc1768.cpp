#include "rs485_lpc1768.hpp"

// UART2 registers (base 0x40098000)
#define U2RBR    (*(volatile uint8_t*)0x40098000)
#define U2THR    (*(volatile uint8_t*)0x40098000)
#define U2DLL    (*(volatile uint8_t*)0x40098000)
#define U2DLM    (*(volatile uint8_t*)0x40098004)
#define U2FCR    (*(volatile uint8_t*)0x40098008)
#define U2LCR    (*(volatile uint8_t*)0x4009800C)
#define U2LSR    (*(volatile uint8_t*)0x40098014)

// UART2 LSR bits
#define LSR_THRE  (1 << 5)
#define LSR_TEMT  (1 << 6)
#define LSR_RDR   (1 << 0)

// GPIO0 base
#define FIO0BASE  0x20098000UL
#define FIO0DIR   (*(volatile uint32_t*)FIO0BASE)
#define FIO0SET   (*(volatile uint32_t*)(FIO0BASE + 0x1C))
#define FIO0CLR   (*(volatile uint32_t*)(FIO0BASE + 0x20))

// DE control pin
#define DE_PORT   0
#define DE_PIN    9
#define DE_MASK   (1UL << DE_PIN)

// Pin connect block
#define PINSEL0   (*(volatile uint32_t*)0x4002C000)

namespace hal::lpc1768 {

Rs485Lpc1768::Rs485Lpc1768() {}

Status Rs485Lpc1768::init(uint32_t baud) {
    (void)baud;

    // Configure P0.10 as TX (01), P0.11 as RX (01)
    PINSEL0 = (PINSEL0 & ~(0x3F << 20)) | (0x01 << 20) | (0x01 << 22);

    // Configure DE pin as output
    FIO0DIR |= DE_MASK;
    set_rx_mode();

    // Enable DLAB, 8N1
    U2LCR = 0x83;
    // Baud rate: assuming CCLK = 96 MHz, PCLK = CCLK
    // DLL = 96000000 / (16 * 115200) = 52
    U2DLL = 52;
    U2DLM = 0;
    // Clear DLAB, enable FIFOs
    U2LCR = 0x03;
    U2FCR = 0x07;

    return Status::OK;
}

Status Rs485Lpc1768::send(const uint8_t* data, std::size_t len, uint32_t timeout_ms) {
    if (data == nullptr || len == 0) return Status::HW_ERROR;

    set_tx_mode();

    for (std::size_t i = 0; i < len; i++) {
        uint32_t elapsed = 0;
        while (!(U2LSR & LSR_THRE)) {
            if (timeout_ms > 0 && ++elapsed > timeout_ms * 1000) {
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
    constexpr uint32_t TICKS_PER_MS = 100000;

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

bool Rs485Lpc1768::tx_complete() {
    return (U2LSR & LSR_TEMT) != 0;
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
