#include <stdint.h>

#define REG32(addr) (*reinterpret_cast<volatile uint32_t*>(addr))
#define GPIO1_FIODIR REG32(0x2009C020)
#define GPIO1_FIOSET REG32(0x2009C038)
#define GPIO1_FIOCLR REG32(0x2009C03C)
#define LED_PIN 18

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

int main(void) {
    GPIO1_FIODIR |= (1 << LED_PIN);

    while (true) {
        GPIO1_FIOSET = (1 << LED_PIN);
        for (volatile uint32_t i = 0; i < 2000000; i++) { }
        GPIO1_FIOCLR = (1 << LED_PIN);
        for (volatile uint32_t i = 0; i < 2000000; i++) { }
    }
}
