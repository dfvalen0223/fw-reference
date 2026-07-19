#include <stdint.h>

#define REG32(addr) (*reinterpret_cast<volatile uint32_t*>(addr))

#define FIO2DIR REG32(0x2009C040)
#define FIO2SET REG32(0x2009C058)
#define FIO2CLR REG32(0x2009C05C)

#define PINSEL4 REG32(0x4002C010)

#define FIO1DIR REG32(0x2009C020)
#define FIO1SET REG32(0x2009C038)

#define WDMOD REG32(0x40000000)
#define WDFEED REG32(0x40000008)

#define LED1 (1u << 18)
#define SCK  (1u << 0)
#define MOSI (1u << 1)
#define CS   (1u << 3)

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

static void feed_wdt(void) {
    WDFEED = 0xAA;
    WDFEED = 0x55;
}

static void delay(volatile uint32_t n) {
    while (n--) { __asm volatile("nop"); }
}

int main(void) {
    FIO1DIR |= LED1;

    // GPIO init
    PINSEL4 &= ~0xFFu;
    FIO2DIR |= SCK | MOSI | CS;
    FIO2SET = CS;  // CS inactive (high)
    FIO2CLR = SCK; // SCK low

    feed_wdt();

    // Phase 1: 8 SCK pulses via FIO2SET/CLR, feed watchdog between each
    for (int i = 0; i < 8; i++) {
        FIO2SET = SCK;
        delay(500000);
        FIO2CLR = SCK;
        delay(500000);
        feed_wdt();
    }

    // Phase 2: CS via FIO2SET/CLR
    for (int i = 0; i < 4; i++) {
        FIO2SET = CS;
        delay(500000);
        FIO2CLR = CS;
        delay(500000);
        feed_wdt();
    }

    // Done
    FIO1SET = LED1;

    while (1) {
        feed_wdt();
        delay(5000000);
    }
}
