// MCP2515 internal loopback HIL test. No second CAN node needed: the
// chip's own LOOPBACK mode (sec 10.4, CANCTRL REQOP=010) feeds TX back
// to RX inside the silicon, so this validates the SPI link, register
// map, and CNF1/2/3 bit-timing config end to end.
//
// Wiring (LPC1768 bit-bang SPI on Port 2 -> MCP2515 module):
//   P2.0 (p26) -> SCK
//   P2.2 (p24) <- SO  (MISO — input only)
//   P2.1 (p25) -> SI  (MOSI)
//   P2.3 (p23) -> CS
//   GND common, module VCC to 3.3V (or 5V if the module has its own
//   regulator — check before wiring; the LPC1768 side is 3.3V only).
//
// CNF1=0x00, CNF2=0x90, CNF3=0x02: 500 kbps from an 8 MHz crystal on
// the MCP2515 module (derived from UM section 5.7 formulas, see
// docs/design_decisions.md).
//
//   LED1 (P1.18): toggles on every PASS round  -> steady blink = OK
//   LED4 (P1.23): ON when the last round FAILED

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
    // See mcp2515_bus_test.cpp for the calibration measurement behind
    // 6400 (empirically ~1ms at confirmed 96MHz CCLK, Debug/-O0 build
    // — the naive "96000 iterations ~= 1ms" guess was 15.03x too slow).
    while (n--) {
        for (volatile uint32_t i = 0; i < 6400; i++) { }
    }
}

static void delay_us_fn(uint32_t us) {
    // Called only once, for the ~16us OST wait — a coarse busy loop is
    // fine here (no need for the ms-loop's calibration precision).
    for (volatile uint32_t i = 0; i < us * 96; i++) { }
}

int main(void) {
    PINSEL3 &= ~((0x3u << 4) | (0x3u << 14));
    FIO1DIR |= (1u << LED1) | (1u << LED4);
    FIO1CLR  = (1u << LED1) | (1u << LED4);

    hal::lpc1768::BitbangSpiLpc1768 spi;
    if (spi.init(1000000, 0) != hal::ISpi::Status::OK) {  // 1 MHz, mode 0
        // LED1 = SPI init fail
        FIO1SET = (1u << LED1);
        while (1) { }
    }

    drivers::Mcp2515 can(spi);
    if (!can.init(0x00, 0x90, 0x02, drivers::Mcp2515::Mode::LOOPBACK,
                  8000000, delay_us_fn)) {
        // LED4 = CAN init fail (SPI communication broken)
        FIO1SET = (1u << LED4);
        while (1) { }
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
            // Loopback delivers the frame to RXB0 essentially
            // immediately; a short poll window is plenty.
            for (int i = 0; i < 1000 && !can.has_message(); i++) { }

            drivers::Mcp2515::Frame rx;
            if (can.has_message() && can.receive(rx)) {
                pass = (rx.id == tx.id) && (rx.dlc == tx.dlc);
                for (uint8_t i = 0; pass && i < tx.dlc; i++) {
                    if (rx.data[i] != tx.data[i]) pass = false;
                }
            }
        }

        if (pass) {
            FIO1PIN ^= (1u << LED1);
            FIO1CLR  = (1u << LED4);
        } else {
            FIO1SET  = (1u << LED4);
        }

        id = (id == 0x7FF) ? 0x000 : static_cast<uint16_t>(id + 1);
        delay_ms(500);
    }
}
