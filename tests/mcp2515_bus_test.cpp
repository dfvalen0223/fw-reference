// MCP2515 two-node real-bus HIL test, LPC1768 side.
//
// Counterpart to rp2040/can_node.cpp. Mode::NORMAL (not LOOPBACK):
// this node sends its own frame on the physical CAN bus and listens
// for the RP2040's frames, instead of looping a message back to
// itself internally. Validates the driver against a real MCP2515<->
// MCP2515 differential bus, not just one chip's internal test mode.
//
// Wiring (LPC1768 bit-bang SPI on Port 2 -> MCP2515 module):
//   P2.0 (p26) -> SCK,  P2.1 (p25) -> SI,  P2.2 (p24) <- SO,
//   P2.3 (p23) -> CS,   GND common, module VCC per its transceiver's
//   real requirement (see rp2040/can_node.cpp — TJA1050 needs 5V, not
//   3.3V, even though the SPI lines themselves stay 3.3V-3.3V).
//
// CAN bus connector: use the module's DIRECT CANH/CANL header, not
// any connector with the onboard 120R termination resistor wired in
// SERIES with the signal path (see rp2040/can_node.cpp for the full
// story — that exact mistake caused one direction of communication to
// silently fail for a long debugging session while the other worked).
//
// CNF1=0xC1, CNF2=0xA4, CNF3=0x04: 125 kbps, SJW=4, from an 8 MHz
// crystal — MUST match the RP2040 side exactly, or neither node ACKs
// the other's frames (bit-rate mismatch is silent, not a clean
// failure).
//
//   LED1 (P1.18): toggles whenever a frame is received since the last
//                 send (steady blink = hearing the RP2040)
//   LED4 (P1.23): ON when nothing was received in that window

#include <stdint.h>
#include <stddef.h>
#include "hal/lpc1768/bitbang_spi_lpc1768.hpp"
#include "drivers/mcp2515.hpp"

#define REG32(addr) (*reinterpret_cast<volatile uint32_t*>(addr))

#define PINSEL3   REG32(0x4002C00C)
#define FIO1DIR   REG32(0x2009C020)
#define FIO1PIN   REG32(0x2009C034)
#define FIO1SET   REG32(0x2009C038)
#define FIO1CLR   REG32(0x2009C03C)

#define LED1  18
#define LED4  23

extern uint32_t _estack;
extern uint32_t _sdata, _edata, _sidata;
extern uint32_t _sbss, _ebss;

extern "C" int main(void);
extern "C" void SystemInit(void);

extern "C" {

void Default_Handler(void) { while (1) { } }
void Reset_Handler(void);

__attribute__((section(".vectors")))
void (*const vector_table[])(void) = {
    reinterpret_cast<void(*)(void)>(&_estack),
    Reset_Handler,
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    0, 0, 0, 0, Default_Handler, 0, Default_Handler, 0,
    Default_Handler, Default_Handler,
};

void Reset_Handler(void) {
    for (uint32_t *src = &_sidata, *dst = &_sdata; dst < &_edata;)
        *dst++ = *src++;
    for (uint32_t *dst = &_sbss; dst < &_ebss;)
        *dst++ = 0;
    SystemInit();
    main();
    while (1) { }
}

}

static void delay_ms(volatile uint32_t n) {
    // Empirically calibrated against a logic-analyzer measurement of
    // this exact loop at confirmed 96MHz CCLK (Debug/-O0 build): the
    // naive "96000 iterations ~= 1ms" assumption ran 15.03x slower
    // than intended (measured 75.158ms for delay_ms(5), i.e. one
    // volatile loop iteration costs far more than 1 cycle once the
    // compiler can't keep the counter in a register). 6400 iterations
    // measured to ~1ms under these exact build settings.
    while (n--) {
        for (volatile uint32_t i = 0; i < 6400; i++) { }
    }
}

static void delay_us_fn(uint32_t us) {
    for (volatile uint32_t i = 0; i < us * 96; i++) { }
}

int main(void) {
    PINSEL3 &= ~((0x3u << 4) | (0x3u << 14));
    FIO1DIR |= (1u << LED1) | (1u << LED4);
    FIO1CLR  = (1u << LED1) | (1u << LED4);

    hal::lpc1768::BitbangSpiLpc1768 spi;
    if (spi.init(1000000, 0) != hal::ISpi::Status::OK) {
        FIO1SET = (1u << LED1) | (1u << LED4);
        while (1) { }
    }

    drivers::Mcp2515 can(spi);
    if (!can.init(0xC1, 0xA4, 0x04, drivers::Mcp2515::Mode::NORMAL,
                  8000000, delay_us_fn)) {
        FIO1SET = (1u << LED1) | (1u << LED4);
        while (1) { }
    }

    // One-Shot Mode (CANCTRL bit3): try each send exactly once instead
    // of auto-retrying at hardware speed forever on failure. Keeps the
    // bus from being monopolized by one node's retry storm.
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
        tx.id = 0x200;
        tx.dlc = 1;
        tx.data[0] = counter++;
        can.send(tx);

        bool heard_rp2040 = false;
        for (int i = 0; i < 100; i++) {
            drivers::Mcp2515::Frame rx;
            if (can.receive(rx) && rx.id == 0x100 && rx.dlc == 1) heard_rp2040 = true;
            delay_ms(5);
        }

        if (heard_rp2040) {
            FIO1PIN ^= (1u << LED1);
            FIO1CLR  = (1u << LED4);
        } else {
            FIO1SET  = (1u << LED4);
        }

        delay_ms(500);
    }
}
