// SPI bitbang debug: toggle SCK 8 times in a raw loop, no class, no
// interface. Connect CH0=SCK(p26), CH1=MOSI(p25), CH2=CS(p23).
// LED1 heartbeat. After loop, LED4 on.

#include <stdint.h>

#define REG32(addr) (*reinterpret_cast<volatile uint32_t*>(addr))

#define FIO2DIR REG32(0x2009C040)
#define FIO2PIN REG32(0x2009C054)
#define FIO2SET REG32(0x2009C058)
#define FIO2CLR REG32(0x2009C05C)

#define PINSEL4 REG32(0x4002C010)

#define FIO1DIR REG32(0x2009C020)
#define FIO1SET REG32(0x2009C038)
#define FIO1CLR REG32(0x2009C03C)

#define LED1 (1u << 18)
#define LED4 (1u << 23)

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

static void delay(volatile uint32_t n) {
    while (n--) { __asm volatile("nop"); }
}

int main(void) {
    FIO1DIR |= LED1 | LED4;

    // GPIO init
    PINSEL4 &= ~0xFFu;
    FIO2DIR |= SCK | MOSI | CS;
    FIO2DIR &= ~(1u << 2);  // MISO input

    // idle
    FIO2SET = CS;
    FIO2CLR = SCK;

    delay(50000);

    // Send byte 0xC0 (8 bits, MSB first)
    FIO2CLR = CS;  // select

    uint8_t txb = 0xC0;
    for (int bit = 7; bit >= 0; bit--) {
        // set MOSI
        if (txb & (1u << bit))
            FIO2SET = MOSI;
        else
            FIO2CLR = MOSI;

        delay(3);    // small setup
        FIO2SET = SCK;  // rising edge
        delay(3);    // hold
        FIO2CLR = SCK;  // falling edge
        delay(3);    // inter-bit gap
    }

    FIO2SET = CS;  // deselect
    delay(50000);

    // Send 3 bytes
    uint8_t data[3] = {0x02, 0x29, 0x02};
    FIO2CLR = CS;  // select
    for (int byte = 0; byte < 3; byte++) {
        txb = data[byte];
        for (int bit = 7; bit >= 0; bit--) {
            if (txb & (1u << bit))
                FIO2SET = MOSI;
            else
                FIO2CLR = MOSI;

            delay(3);
            FIO2SET = SCK;
            delay(3);
            FIO2CLR = SCK;
            delay(3);
        }
    }
    FIO2SET = CS;
    delay(50000);

    // Done: LED1 on
    FIO1SET = LED1;

    while (1) {
        // toggle LED4 so we know we're alive
        FIO1SET = LED4;
        delay(500000);
        FIO1CLR = LED4;
        delay(500000);
    }
}
