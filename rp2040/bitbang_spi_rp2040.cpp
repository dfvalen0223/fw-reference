#include "bitbang_spi_rp2040.hpp"

#include "hardware/gpio.h"
#include "pico/time.h"

#define SCK_PIN  2
#define MOSI_PIN 3
#define MISO_PIN 4
#define CS_PIN   5

namespace {
inline void set_sck(int v) { gpio_put(SCK_PIN, v); }
inline void set_mosi(int v) { gpio_put(MOSI_PIN, v); }
inline int get_miso() { return gpio_get(MISO_PIN) ? 1 : 0; }
}  // namespace

hal::ISpi::Status BitbangSpiRp2040::init(uint32_t clock_hz, uint8_t mode) {
    if (clock_hz == 0 || mode > 3) return Status::HW_ERROR;

    cpol_ = (mode >> 1) & 1;
    cpha_ = mode & 1;

    // sleep_us() has ~1us practical granularity on this SDK; anything
    // requested above ~500kHz just clamps to the fastest achievable
    // rate. MCP2515 tolerates any SPI clock down to DC (datasheet
    // Features: "High-Speed SPI Interface (10 MHz)" as a ceiling, no
    // floor), so a conservative half-period is safe either way.
    uint32_t half_period_ns = 500000000UL / clock_hz;
    half_period_ns_ = half_period_ns < 1000 ? 1000 : half_period_ns;

    gpio_init(SCK_PIN);
    gpio_init(MOSI_PIN);
    gpio_init(MISO_PIN);
    gpio_init(CS_PIN);

    // CS driven high BEFORE switching to output: gpio_init() clears
    // the output register, so if we set direction=OUT first, there's
    // a brief window where CS could glitch low (selected) before the
    // explicit gpio_put(1) below runs. Setting the output level while
    // still an input avoids that — output register is pre-loaded, so
    // the pin comes up already high the instant it becomes an output.
    gpio_put(CS_PIN, 1);

    gpio_set_dir(SCK_PIN, GPIO_OUT);
    gpio_set_dir(MOSI_PIN, GPIO_OUT);
    gpio_set_dir(MISO_PIN, GPIO_IN);
    gpio_set_dir(CS_PIN, GPIO_OUT);

    // Pico SDK GPIOs default to no pull (floating) unless explicitly
    // configured — unlike the LPC1768 side, where MISO's on-chip
    // pull-up is enabled by reset default. MCP2515's SO output should
    // actively drive throughout a transfer, so this is defensive
    // insurance against any brief high-Z window (e.g. right as CS
    // falls, before the slave starts driving) reading as noise instead
    // of a defined level.
    gpio_pull_up(MISO_PIN);

    set_sck(cpol_);
    gpio_put(CS_PIN, 1);  // idle deselected

    return Status::OK;
}

hal::ISpi::Status BitbangSpiRp2040::transfer(const uint8_t* tx, uint8_t* rx,
                                             std::size_t len, uint32_t timeout_ms) {
    (void)timeout_ms;  // bit-bang is deterministic; no timeout needed
    if (tx == nullptr || rx == nullptr) return Status::HW_ERROR;

    for (std::size_t byte_i = 0; byte_i < len; byte_i++) {
        uint8_t txb = tx[byte_i];
        uint8_t rxb = 0;

        for (int bit = 7; bit >= 0; bit--) {
            set_mosi((txb >> bit) & 1);

            if (cpha_ == 0) {
                // leading edge samples, trailing edge changes
                sleep_us(half_period_ns_ / 1000 + 1);
                set_sck(1 ^ cpol_);
                rxb = static_cast<uint8_t>((rxb << 1) | get_miso());
                sleep_us(half_period_ns_ / 1000 + 1);
                set_sck(cpol_);
            } else {
                set_sck(1 ^ cpol_);
                sleep_us(half_period_ns_ / 1000 + 1);
                set_sck(cpol_);
                rxb = static_cast<uint8_t>((rxb << 1) | get_miso());
                sleep_us(half_period_ns_ / 1000 + 1);
            }
        }

        rx[byte_i] = rxb;
    }

    return Status::OK;
}

hal::ISpi::Status BitbangSpiRp2040::select_slave(uint8_t /*slave_index*/) {
    gpio_put(CS_PIN, 0);
    sleep_us(5);
    return Status::OK;
}

hal::ISpi::Status BitbangSpiRp2040::deselect_slave(uint8_t /*slave_index*/) {
    sleep_us(5);
    gpio_put(CS_PIN, 1);
    sleep_us(5);
    return Status::OK;
}
