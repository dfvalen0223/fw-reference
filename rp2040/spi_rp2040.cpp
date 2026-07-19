#include "spi_rp2040.hpp"

#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "pico/time.h"

#define SCK_PIN  2
#define MOSI_PIN 3
#define MISO_PIN 4
#define CS_PIN   5

hal::ISpi::Status SpiRp2040::init(uint32_t clock_hz, uint8_t mode) {
    if (clock_hz == 0 || mode > 3) return Status::HW_ERROR;

    spi_init(spi0, clock_hz);

    // mode: 0=CPOL0/CPHA0 ... 3=CPOL1/CPHA1 (same convention as the
    // LPC1768 HALs in this project).
    spi_cpol_t cpol = (mode & 0x2) ? SPI_CPOL_1 : SPI_CPOL_0;
    spi_cpha_t cpha = (mode & 0x1) ? SPI_CPHA_1 : SPI_CPHA_0;
    spi_set_format(spi0, 8, cpol, cpha, SPI_MSB_FIRST);

    gpio_set_function(SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(MISO_PIN, GPIO_FUNC_SPI);

    // CS as plain GPIO, idle high (deselected) — Pico SDK's hardware
    // SPI does not auto-drive chip select in master mode.
    gpio_init(CS_PIN);
    gpio_set_dir(CS_PIN, GPIO_OUT);
    gpio_put(CS_PIN, 1);

    return Status::OK;
}

hal::ISpi::Status SpiRp2040::transfer(const uint8_t* tx, uint8_t* rx,
                                      std::size_t len, uint32_t timeout_ms) {
    (void)timeout_ms;  // spi_write_read_blocking has no timeout param;
                       // the RP2040 SPI peripheral is deterministic and
                       // does not stall indefinitely like a bitbang loop.
    if (tx == nullptr || rx == nullptr) return Status::HW_ERROR;

    int written = spi_write_read_blocking(spi0, tx, rx, len);
    return (written == static_cast<int>(len)) ? Status::OK : Status::HW_ERROR;
}

hal::ISpi::Status SpiRp2040::select_slave(uint8_t /*slave_index*/) {
    gpio_put(CS_PIN, 0);
    // MCP2515 datasheet Table 13-6: T_CSS (CS Setup Time) min 50ns.
    // Bumped from 1us to 5us: a live capture proved back-to-back reads
    // of the same static register (CANSTAT) disagreed 9/9 times, with
    // each read's value matching the PREVIOUS transaction's expected
    // response — a one-transaction lag, not random noise. That pattern
    // fits the chip's internal SPI state machine not having fully
    // settled from the prior command before the next CS falling edge,
    // even though 1us already cleared the datasheet's documented
    // T_CSS/T_CSD figures. 5us is cheap insurance against whatever
    // undocumented inter-command settling this specific silicon needs.
    sleep_us(5);
    return Status::OK;
}

hal::ISpi::Status SpiRp2040::deselect_slave(uint8_t /*slave_index*/) {
    // T_CSD (CS Disable Time) min 50ns: same margin, other direction —
    // wait after the last clock edge before raising CS. Also bumped to
    // 5us; see select_slave() for why.
    sleep_us(5);
    gpio_put(CS_PIN, 1);
    // Extra idle time with CS already high, before the caller can
    // possibly call select_slave() again for a follow-up transaction —
    // gives the MCP2515's internal state machine room to fully settle
    // between back-to-back commands (the double-CANSTAT-read test
    // showed 1us total gap wasn't enough).
    sleep_us(5);
    return Status::OK;
}
