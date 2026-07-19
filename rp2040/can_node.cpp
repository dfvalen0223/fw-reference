// MCP2515 CAN node for real two-node HIL testing, Waveshare RP2040-Zero.
//
// Counterpart to tests/mcp2515_bus_test.cpp on the LPC1768 side.
// Mode::NORMAL (not LOOPBACK) — this node talks over a real physical
// CAN bus (CANH/CANL, transceiver on both modules, 120R termination
// at each physical end) instead of looping back internally.
//
// Wiring (RP2040-Zero -> MCP2515 module):
//   GP2 -> SCK, GP3 -> SI, GP4 <- SO, GP5 -> CS, GND common, VCC 5V
//   (this module's TJA1050 transceiver needs 4.75-5.25V per its own
//   datasheet; the SPI lines stay 3.3V-3.3V with the RP2040 either
//   way, only the module's own supply pin needs 5V).
//
// CAN bus connector: use the module's DIRECT CANH/CANL header (J2/J3
// on this board's silkscreen), NOT J1 — J1 has the onboard 120R
// resistor wired in SERIES with CANH before it reaches the connector,
// so using J1 as the main bus connection point puts 120 ohms in the
// signal path itself instead of terminating it, degrading reception
// without fully blocking it (a real, confirmed root cause this
// project hit: one direction of communication silently failed for
// hours while the other worked, traced to exactly this). To actually
// enable this module's own termination resistor, bridge J1's two pins
// with a jumper while the real bus wire goes through J2/J3.
//
// Behavior: sends its own frame (ID 0x100, incrementing payload)
// every ~1s, and reports on the WS2812 (GP16) whether it heard the
// LPC's traffic (ID 0x200) since the last send:
//   blue x3 = boot, then per round: green = received, red = nothing.

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

    // Bit-banged SPI, not the Pico SDK hardware_spi peripheral: an
    // earlier version of this HAL used hardware_spi and corrupted
    // data on rapid back-to-back short transfers (proven by reading
    // the same static register twice with no delay and getting two
    // different values, every time). Bit-banging sidesteps whatever
    // peripheral/FIFO issue caused that.
    BitbangSpiRp2040 spi;
    if (spi.init(1000000, 0) != hal::ISpi::Status::OK) {
        while (true) { put_pixel(rgb(32, 0, 0)); sleep_ms(200); put_pixel(0); sleep_ms(200); }
    }

    drivers::Mcp2515 can(spi);
    // 125 kbps, SJW=4 (max): more per-bit timing margin than 500kbps
    // for unshielded breadboard jumpers. Must match the LPC side's
    // CNF1/2/3 exactly (both use an 8 MHz crystal).
    // 8 MHz, BRP=1: TQ = 2*2/8MHz = 500ns, 16 TQ/bit -> 8us bit
    // (125 kbps). SyncSeg=1, PropSeg=5, PS1=5, PS2=5 (sample point
    // 68.75%, within the 60-70% recommended range).
    if (!can.init(0xC1, 0xA4, 0x04, drivers::Mcp2515::Mode::NORMAL,
                  8000000, delay_us_fn)) {
        while (true) { put_pixel(rgb(32, 0, 0)); sleep_ms(100); put_pixel(0); sleep_ms(100); }
    }

    // One-Shot Mode (CANCTRL bit3): try each send exactly once instead
    // of auto-retrying at hardware speed forever on failure. Keeps the
    // bus from being monopolized by one node's retry storm and makes
    // genuine success/failure directly observable per round.
    {
        uint8_t bitmod[4] = {0x05, 0x0F /*CANCTRL*/,
                             0x08 /*mask: OSM bit only*/, 0x08 /*set OSM=1*/};
        uint8_t dummy[4] = {};
        spi.select_slave(0);
        spi.transfer(bitmod, dummy, 4, 100);
        spi.deselect_slave(0);
    }

    uint8_t counter = 0;
    while (true) {
        drivers::Mcp2515::Frame tx;
        tx.id = 0x100;
        tx.dlc = 1;
        tx.data[0] = counter++;
        can.send(tx);

        bool heard_lpc = false;
        for (int i = 0; i < 200; i++) {
            drivers::Mcp2515::Frame rx;
            if (can.receive(rx) && rx.id == 0x200 && rx.dlc == 1) heard_lpc = true;
            sleep_ms(5);
        }

        put_pixel(heard_lpc ? rgb(0, 32, 0) : rgb(32, 0, 0));
        sleep_ms(50);
        put_pixel(0);
    }
}
