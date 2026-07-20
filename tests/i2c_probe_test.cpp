// Bit-bang I2C probe: does the bus talk back?
//
// Reads the BMP280's CHIP_ID register (0xD0) over I2C and checks for the
// documented reset value 0x58 (BST-BMP280-DS001-26, sec 4.3.1) — the
// smallest possible proof of a working I2C link before writing any real
// driver logic on top of it.
//
// Wiring: SCL=P2.4 (p22), SDA=P2.5 (p21), GND common, VCC 3.3V. Both
// lines need external pull-ups to 3.3V (typically already on the BMP280
// breakout board) — this bit-bang HAL drives open-drain by switching
// GPIO direction, it does not supply pull-ups itself.
//
// BMP280 7-bit address: 0x76 if SDO is grounded, 0x77 if SDO is pulled
// high. This probe tries 0x76 first, falls back to 0x77 on NACK.
//
// LED verdict (repeats every ~1s so the analyzer can capture):
//   LED1 only  : read 0x58            -> I2C RX WORKS, chip alive
//   LED2 only  : NACK on both addrs   -> nothing on the bus at either
//                                        address (wiring, power, or
//                                        stuck bus)
//   LED4 only  : ACKed but wrong ID   -> bus works, wrong device/register

#include <stdint.h>
#include <stddef.h>
#include "hal/lpc1768/bitbang_i2c_lpc1768.hpp"

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
    // 6400 (empirically ~1ms at confirmed 96MHz CCLK, Debug/-O0 build).
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
    // independent of any I2C result.
    FIO1SET = LED_ALL;
    delay_ms(1000);
    FIO1CLR = LED_ALL;

    hal::lpc1768::BitbangI2cLpc1768 i2c;
    i2c.init(100000);

    while (true) {
        uint8_t reg = 0xD0;  // BMP280 CHIP_ID register
        uint8_t id = 0;
        uint8_t addr = 0x76;

        hal::II2c::Status st = i2c.write_read(addr, &reg, 1, &id, 1);
        if (st == hal::II2c::Status::NACK) {
            addr = 0x77;
            st = i2c.write_read(addr, &reg, 1, &id, 1);
        }

        FIO1CLR = LED_ALL;
        if (st == hal::II2c::Status::NACK) {
            FIO1SET = LED2;
        } else if (st == hal::II2c::Status::OK && id == 0x58) {
            FIO1SET = LED1;
        } else {
            FIO1SET = LED4;
        }

        delay_ms(1000);
    }
}
