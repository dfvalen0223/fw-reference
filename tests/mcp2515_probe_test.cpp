// MCP2515 SPI probe: answers ONE question — does the chip talk back?
//
// After RESET + OST wait, CANCTRL has a known reset value of 0x87
// (Register 10-1: REQOP=100 Configuration, CLKEN=1, CLKPRE=11). Reading
// it is the smallest possible proof of a working full-duplex SPI link,
// with no mode changes and no bit-timing involved.
//
// Wiring (same as mcp2515_loopback_test):
//   P2.0 (p26) -> SCK,  P2.1 (p25) -> SI,  P2.2 (p24) <- SO,
//   P2.3 (p23) -> CS,   GND common, VCC 3.3V.
//
// LED verdict (repeats every ~1s so the analyzer can capture):
//   LED1 only  : read 0x87       -> SPI RX WORKS, chip alive
//   LED2 only  : read 0x00       -> SO stuck low (short? wrong pin? chip
//                                   held in reset?)
//   LED3 only  : read 0xFF       -> SO floating (SO not connected to p24,
//                                   chip unpowered, or CS not reaching it;
//                                   0xFF = MISO pull-up wins)
//   LED4 only  : anything else   -> partial garbage (noise, timing)
//
// mbed LEDs: LED1=P1.18, LED2=P1.20, LED3=P1.21, LED4=P1.23.

#include <stdint.h>
#include <stddef.h>
#include "hal/lpc1768/bitbang_spi_lpc1768.hpp"

#define REG32(addr) (*reinterpret_cast<volatile uint32_t*>(addr))

#define PINSEL3   REG32(0x4002C00C)
#define FIO1DIR   REG32(0x2009C020)
#define FIO1SET   REG32(0x2009C038)
#define FIO1CLR   REG32(0x2009C03C)

#define LED1 (1u << 18)
#define LED2 (1u << 20)
#define LED3 (1u << 21)
#define LED4 (1u << 23)
#define LED_ALL (LED1 | LED2 | LED3 | LED4)

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

int main(void) {
    // LEDs as GPIO outputs
    PINSEL3 &= ~((0x3u << 4) | (0x3u << 8) | (0x3u << 10) | (0x3u << 14));
    FIO1DIR |= LED_ALL;
    FIO1CLR  = LED_ALL;

    // Boot indicator: all 4 LEDs on for ~1s proves the firmware started
    // and the LED path works, independent of any SPI result.
    FIO1SET = LED_ALL;
    delay_ms(1000);
    FIO1CLR = LED_ALL;

    hal::lpc1768::BitbangSpiLpc1768 spi;
    spi.init(1000000, 0);

    while (true) {
        // RESET instruction (0xC0), then OST wait (>16us at 8 MHz; use
        // 1 ms — generous, and this probe is not timing-sensitive).
        uint8_t tx = 0xC0;
        uint8_t rx = 0;
        spi.select_slave(0);
        spi.transfer(&tx, &rx, 1, 100);
        spi.deselect_slave(0);
        delay_ms(1);

        // READ CANCTRL (0x0F): [0x03, 0x0F, dummy] -> reply in byte 2
        uint8_t rtx[3] = {0x03, 0x0F, 0x00};
        uint8_t rrx[3] = {};
        spi.select_slave(0);
        spi.transfer(rtx, rrx, 3, 100);
        spi.deselect_slave(0);

        const uint8_t canctrl = rrx[2];

        FIO1CLR = LED_ALL;
        if      (canctrl == 0x87) FIO1SET = LED1;
        else if (canctrl == 0x00) FIO1SET = LED2;
        else if (canctrl == 0xFF) FIO1SET = LED3;
        else                      FIO1SET = LED4;

        delay_ms(1000);
    }
}
