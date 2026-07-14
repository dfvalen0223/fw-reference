// RS-485 loopback test against the RP2040 echo responder.
//
// Runs SystemInit() first: 96 MHz CCLK from PLL0, PCLK = 24 MHz.
// Uses the real HAL (Rs485Lpc1768): UART2 @ 115200 on P0.10/P0.11 (mbed
// p28/p27), DE on P0.9 (mbed p5). Sends an 8-byte pattern, expects the
// echo responder to return it byte-for-byte, compares.
//
//   LED1 (P1.18): toggles on every PASS round        -> steady blink = OK
//   LED4 (P1.23): ON when the last round FAILED       -> lit = problem
//
// Wiring (direct TTL, no transceivers):
//   mbed p28 (TXD2) -> RP2040 GP5 (UART1 RX)
//   mbed p27 (RXD2) <- RP2040 GP4 (UART1 TX)
//   GND common. RP2040 flashed with echo_responder.uf2 at 115200 baud.

#include <stdint.h>
#include <stddef.h>
#include "hal/lpc1768/rs485_lpc1768.hpp"

#define REG32(addr) (*reinterpret_cast<volatile uint32_t*>(addr))

#define PINSEL3   REG32(0x4002C00C)
#define FIO1DIR   REG32(0x2009C020)
#define FIO1PIN   REG32(0x2009C034)
#define FIO1SET   REG32(0x2009C038)
#define FIO1CLR   REG32(0x2009C03C)

#define LED1  18   // pass heartbeat
#define LED4  23   // fail indicator

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

static void delay(volatile uint32_t n) {
    while (n--) { __asm volatile("nop"); }
}

int main(void) {
    // LEDs P1.18 / P1.23 as GPIO outputs
    PINSEL3 &= ~((0x3u << 4) | (0x3u << 14));
    FIO1DIR |= (1u << LED1) | (1u << LED4);
    FIO1CLR  = (1u << LED1) | (1u << LED4);

    hal::lpc1768::Rs485Lpc1768 rs485;
    if (rs485.init(115200) != hal::Status::OK) {
        // Init failed: both LEDs solid
        FIO1SET = (1u << LED1) | (1u << LED4);
        while (1) { }
    }

    uint8_t seq = 0;
    while (true) {
        uint8_t tx[8] = {0x55, 0xAA, seq, static_cast<uint8_t>(~seq),
                         0x01, 0x02, 0x03, 0x04};

        bool pass = false;
        if (rs485.send(tx, sizeof(tx), 100) == hal::Status::OK) {
            uint8_t rx[8] = {0};
            std::size_t rx_len = sizeof(rx);
            // Echo of 8 bytes at 115200 takes ~0.7 ms; 100 ms is generous
            if (rs485.recv(rx, rx_len, 100) == hal::Status::OK &&
                rx_len == sizeof(tx)) {
                pass = true;
                for (std::size_t i = 0; i < sizeof(tx); i++) {
                    if (rx[i] != tx[i]) { pass = false; break; }
                }
            }
        }

        if (pass) {
            FIO1PIN ^= (1u << LED1);   // heartbeat
            FIO1CLR  = (1u << LED4);
        } else {
            FIO1SET  = (1u << LED4);   // fail latch until next pass
        }

        ++seq;
        delay(10000000);   // ~0.5 s between rounds at 96 MHz
    }
}
