// Port 2 GPIO output test. Port 0 outputs are dead on this chip; Port 1
// works. If Port 2 works we can bit-bang SPI to the MCP2515 on p21-p26.
//
// Toggles P2.0-P2.5 (mbed p26-p21) together at a few Hz, LED1 (P1.18)
// as heartbeat. Probe any of p21-p26 with the logic analyzer.
//
// No SystemInit, no PLL: 4 MHz IRC. UART0 (terminal, 9600) prints FIO2
// readbacks so we can compare register level vs physical pin.

#include <stdint.h>

#define REG32(addr) (*reinterpret_cast<volatile uint32_t*>(addr))
#define REG8(addr)  (*reinterpret_cast<volatile uint8_t*>(addr))

// GPIO2 (FIO2 base 0x2009C040)
#define FIO2DIR REG32(0x2009C040)
#define FIO2PIN REG32(0x2009C054)
#define FIO2SET REG32(0x2009C058)
#define FIO2CLR REG32(0x2009C05C)

// GPIO1 (LED)
#define FIO1DIR REG32(0x2009C020)
#define FIO1SET REG32(0x2009C038)
#define FIO1CLR REG32(0x2009C03C)
#define LED_PIN 18

// Pin Connect
#define PINSEL0 REG32(0x4002C000)
#define PINSEL4 REG32(0x4002C010)

// System Control
#define PCLKSEL0 REG32(0x400FC1A8)
#define PCONP    REG32(0x400FC0C4)

// UART0 (terminal)
#define U0_LSR  REG8(0x4000C014)
#define U0_THR  REG8(0x4000C000)
#define U0_DLL  REG8(0x4000C000)
#define U0_DLM  REG8(0x4000C004)
#define U0_FCR  REG8(0x4000C008)
#define U0_LCR  REG8(0x4000C00C)

#define P2_MASK 0x3F  // P2.0-P2.5

extern uint32_t _estack;
extern uint32_t _sdata, _edata, _sidata;
extern uint32_t _sbss, _ebss;

extern "C" int main(void);

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
    main();
    while (1) { }
}

}

static void uart0_putchar(char c) {
    while (!(U0_LSR & (1 << 5))) { }
    U0_THR = (uint8_t)c;
}

static void uart0_puts(const char* s) {
    while (*s) uart0_putchar(*s++);
}

static void uart0_putbits8(uint32_t v) {
    for (int i = 7; i >= 0; --i)
        uart0_putchar('0' + ((v >> i) & 1));
}

static void delay(volatile uint32_t n) {
    while (n--) { __asm volatile("nop"); }
}

int main(void) {
    // ---- GPIO1 LED ----
    FIO1DIR |= (1 << LED_PIN);

    // ---- UART0 init (terminal, 9600 baud, 4 MHz IRC) ----
    PCLKSEL0 = (PCLKSEL0 & ~0xC0) | 0x40;   // UART0 PCLK = CCLK
    PINSEL0 = (PINSEL0 & ~0xF0) | 0x50;      // P0.2 TX, P0.3 RX
    U0_LCR = 0x83;
    U0_DLL = 26;
    U0_DLM = 0;
    U0_LCR = 0x03;
    U0_FCR = 0x01;
    uart0_puts("Port2 test\r\n");

    // ---- Port 2: P2.0-P2.5 as GPIO outputs ----
    PINSEL4 &= ~0xFFFu;       // P2.0-P2.5 = GPIO (function 00)
    __asm volatile("dsb" ::: "memory");
    FIO2DIR |= P2_MASK;
    __asm volatile("dsb" ::: "memory");

    // Readback DIR
    uart0_puts("FIO2DIR=");
    uart0_putbits8(FIO2DIR & 0xFF);
    uart0_puts("\r\n");

    // One slow cycle with readbacks
    FIO2SET = P2_MASK;
    __asm volatile("dsb" ::: "memory");
    uart0_puts("SET: FIO2PIN=");
    uart0_putbits8(FIO2PIN & 0xFF);
    uart0_puts("\r\n");
    delay(500000);

    FIO2CLR = P2_MASK;
    __asm volatile("dsb" ::: "memory");
    uart0_puts("CLR: FIO2PIN=");
    uart0_putbits8(FIO2PIN & 0xFF);
    uart0_puts("\r\n");
    delay(500000);

    uart0_puts("Toggling P2.0-P2.5 (p26-p21)...\r\n");

    // ---- Main loop: toggle ~5 Hz at 4 MHz ----
    while (1) {
        FIO2SET = P2_MASK;
        FIO1SET = (1 << LED_PIN);
        delay(100000);

        FIO2CLR = P2_MASK;
        FIO1CLR = (1 << LED_PIN);
        delay(100000);
    }
}
