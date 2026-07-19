// MCP2515 internal loopback test, RP2040 side. Decisive triage after
// an extremely long, inconclusive two-node bus debugging session:
// this test does NOT touch CANH/CANL, the transceiver, the LPC, or
// any external wiring/probe setup at all — Mode::LOOPBACK feeds TX
// back to RX entirely inside the MCP2515 silicon (datasheet sec 10.4).
//
// Answers exactly one question: is the MCP2515 chip + its SPI link to
// this RP2040 healthy, independent of the transceiver? If this test
// passes (steady green), the chip/SPI/driver stack is proven fine and
// the transceiver is the remaining suspect — no more firmware changes
// will fix a dead transceiver, it needs replacing. If this test FAILS
// too, the problem is upstream of the transceiver (chip, SPI, wiring
// to the chip itself) and we know exactly where to keep looking.
//
// No wiring changes needed beyond what can_node.cpp already used
// (GP2=SCK, GP3=SI, GP4=SO, GP5=CS) — CANH/CANL/transceiver are
// irrelevant to this test and can stay disconnected.
//
// WS2812 (GP16): 3x blue = boot, then every ~1s:
//   green = loopback round-trip matched (chip+SPI healthy)
//   red   = failed (problem is NOT the transceiver)

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "ws2812.pio.h"
#include "bitbang_spi_rp2040.hpp"
#include "drivers/mcp2515.hpp"

#define WS2812_PIN 16

static inline void put_pixel(uint32_t grb) {
    pio_sm_put_blocking(pio0, 0, grb << 8u);
}

static inline uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;
}

static void delay_us_fn(uint32_t us) {
    sleep_us(us);
}

int main(void) {
    uint offset = pio_add_program(pio0, &ws2812_program);
    ws2812_program_init(pio0, 0, offset, WS2812_PIN, 800000, false);

    for (int i = 0; i < 3; i++) {
        put_pixel(rgb(0, 0, 32)); sleep_ms(150);
        put_pixel(0);             sleep_ms(150);
    }

    BitbangSpiRp2040 spi;
    if (spi.init(1000000, 0) != hal::ISpi::Status::OK) {
        while (true) { put_pixel(rgb(32, 0, 0)); sleep_ms(100); put_pixel(0); sleep_ms(100); }
    }

    drivers::Mcp2515 can(spi);
    if (!can.init(0xC1, 0xA4, 0x04, drivers::Mcp2515::Mode::LOOPBACK,
                  8000000, delay_us_fn)) {
        // init() itself failed -> chip isn't even responding over SPI.
        while (true) { put_pixel(rgb(32, 0, 0)); sleep_ms(50); put_pixel(0); sleep_ms(50); }
    }

    uint16_t id = 0x100;
    while (true) {
        drivers::Mcp2515::Frame tx;
        tx.id = id;
        tx.dlc = 4;
        tx.data[0] = 0xDE;
        tx.data[1] = 0xAD;
        tx.data[2] = 0xBE;
        tx.data[3] = 0xEF;

        bool pass = false;
        if (can.send(tx)) {
            for (int i = 0; i < 1000 && !can.has_message(); i++) { }

            drivers::Mcp2515::Frame rx;
            if (can.has_message() && can.receive(rx)) {
                pass = (rx.id == tx.id) && (rx.dlc == tx.dlc);
                for (uint8_t i = 0; pass && i < tx.dlc; i++) {
                    if (rx.data[i] != tx.data[i]) pass = false;
                }
            }
        }

        put_pixel(pass ? rgb(0, 32, 0) : rgb(32, 0, 0));
        sleep_ms(50);
        put_pixel(0);

        id = (id == 0x7FF) ? 0x000 : static_cast<uint16_t>(id + 1);
        sleep_ms(950);
    }
}
