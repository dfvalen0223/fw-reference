#include <stdint.h>

#define SYSCON_BASE 0x400FC000UL

static inline volatile uint32_t &REG(uint32_t off) {
    return *((volatile uint32_t *)(SYSCON_BASE + off));
}

#define FLASHCFG    REG(0x000)
#define CLKSRCSEL   REG(0x010)
#define OSCTRIM     REG(0x01C)
#define SYSOSCCTRL  REG(0x020)
#define PLL0CON     REG(0x080)
#define PLL0CFG     REG(0x084)
#define PLL0STAT    REG(0x088)
#define PLL0FEED    REG(0x08C)
#define CCLKCFG     REG(0x104)
#define PCLKSEL0    REG(0x1A8)
#define PCLKSEL1    REG(0x1AC)

#define DSB __asm volatile("dsb" ::: "memory")
#define ISB __asm volatile("isb" ::: "memory")

static void pll_feed(void) {
    DSB;
    PLL0FEED = 0xAA;
    DSB;
    PLL0FEED = 0x55;
    DSB;
}

extern "C" void SystemInit(void) {
    // 1. Enable main oscillator (12 MHz crystal), no bypass
    SYSOSCCTRL = 0;
    DSB;

    // Wait for oscillator to stabilize (~1 ms at 4 MHz = ~4000 cycles)
    for (volatile uint32_t i = 0; i < 200000; i++) {
        __asm volatile("nop");
    }

    // 2. Select main oscillator as PLL0 clock source
    CLKSRCSEL = 1;
    DSB;

    // 3. Set CPU clock divider to 1 (before PLL change)
    CCLKCFG = 0;
    DSB;

    // 4. Configure PLL0:
    //    Crystal = 12 MHz, target CCLK = 96 MHz
    //    MSEL=7 (M=8), PSEL=0 (P=1), NSEL=0 (N=1)
    //    FCCO = 2 * M * Fosc / N = 2 * 8 * 12 / 1 = 192 MHz ✓
    //    CCLK = FCCO / (2 * P) = 192 / (2 * 1) = 96 MHz ✓
    //    USB can use main osc or PLL1 separately
    PLL0CFG = (7 << 0) | (0 << 5) | (0 << 8);
    PLL0CON = 1;  // Enable PLL0 (not yet connected)
    pll_feed();

    // 5. Wait for PLL0 lock
    while (!(PLL0STAT & (1 << 26))) {
        __asm volatile("nop");
    }

    // 6. Set FLASH wait states for 96 MHz (5 cycles: 96/20 = 4.8 → round up)
    FLASHCFG = (FLASHCFG & ~0x0F00) | (5 << 8);
    DSB;

    // 7. Connect PLL0 as system clock
    PLL0CON = 3;  // Enable + Connect
    pll_feed();
    while (!(PLL0STAT & (1 << 25))) {
        __asm volatile("nop");
    }

    // 8. Set peripheral clocks: UART0 PCLK = CCLK
    PCLKSEL1 = (PCLKSEL1 & ~(3 << 2)) | (1 << 2);
    DSB;

    ISB;
}
