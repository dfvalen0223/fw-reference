// LPC1768 clock setup: 12 MHz crystal -> PLL0 -> 96 MHz CCLK.
//
// Register map per UM10360 chapter 4 (System control):
//   SCS       0x1A0  main oscillator enable/status
//   CLKSRCSEL 0x10C  PLL0 source select
//   PLL0*     0x080+ PLL0 control/config/status/feed
//   CCLKCFG   0x104  CPU clock divider
//   FLASHCFG  0x000  flash accelerator timing (FLASHTIM = bits 15:12)
//
// PLL0 constraints (UM10360 §4.5): FCCO must be 275–550 MHz.
//   M = 12, N = 1: FCCO = 2*M*Fosc/N = 2*12*12/1 = 288 MHz  (in range)
//   CCLKCFG = 2 (divide by 3): CCLK = 288/3 = 96 MHz
//
// Peripheral clocks are left at reset default PCLK = CCLK/4 = 24 MHz.
// (PCLKSEL writes are deliberately avoided; UART divisors elsewhere
// assume 24 MHz.)
//
// Every PLL wait is timeout-guarded; on failure the firmware runs
// (slowly) instead of hanging, at whichever clock was already active:
//   - crystal never starts     -> 4 MHz IRC  (CLKSRCSEL untouched)
//   - crystal ok, PLL no lock  -> 12 MHz raw crystal (CLKSRCSEL=1, PLL0 disconnected)
//   - locked, never connects   -> 12 MHz raw crystal (PLL0 explicitly disabled)

#include <stdint.h>

#define SYSCON_BASE 0x400FC000UL

static inline volatile uint32_t &REG(uint32_t off) {
    return *((volatile uint32_t *)(SYSCON_BASE + off));
}

#define FLASHCFG    REG(0x000)
#define PLL0CON     REG(0x080)
#define PLL0CFG     REG(0x084)
#define PLL0STAT    REG(0x088)
#define PLL0FEED    REG(0x08C)
#define CCLKCFG     REG(0x104)
#define CLKSRCSEL   REG(0x10C)
#define SCS         REG(0x1A0)

// SCS bits
#define SCS_OSCEN   (1u << 5)
#define SCS_OSCSTAT (1u << 6)

// PLL0STAT bits
#define PLOCK0      (1u << 26)
#define PLLC0_STAT  (1u << 25)
#define PLLE0_STAT  (1u << 24)

#define DSB __asm volatile("dsb" ::: "memory")
#define ISB __asm volatile("isb" ::: "memory")

static void pll_feed(void) {
    DSB;
    PLL0FEED = 0xAA;
    DSB;
    PLL0FEED = 0x55;
    DSB;
}

// Diagnostic only: which fallback tier SystemInit() actually reached.
// Not touched by firmware that doesn't care (defaults to 0, harmless).
//   0 = not yet run / 1 = 4MHz IRC (crystal never started)
//   2 = 12MHz raw crystal (PLL never locked)
//   3 = 12MHz raw crystal (locked but never connected)
//   4 = 96MHz full success
extern "C" { volatile uint32_t g_clock_tier = 0; }

extern "C" void SystemInit(void) {
    // 1. Enable main oscillator (12 MHz crystal, OSCRANGE=0 for 1–20 MHz)
    SCS |= SCS_OSCEN;
    DSB;

    // 3 Safety actions (A, B, and C)
    // A. Safety Action: Wait for oscillator ready, with timeout (no crystal -> stay on IRC)
    uint32_t osc_ok = 0;
    for (volatile uint32_t i = 0; i < 1000000; i++) {
        if (SCS & SCS_OSCSTAT) { osc_ok = 1; break; }
    }
    if (!osc_ok) { g_clock_tier = 1; return; }  // fall back to 4 MHz IRC

    // 2. Select main oscillator as PLL0 source
    CLKSRCSEL = 1;
    DSB;

    // 3. Configure PLL0: M=12 (MSEL=11), N=1 (NSEL=0) -> FCCO = 288 MHz
    PLL0CFG = (11u << 0) | (0u << 16);
    pll_feed();

    // 4. (B. Safety Action) Enable PLL0 and wait for lock, with timeout
    PLL0CON = 1;
    pll_feed();

    uint32_t lock_ok = 0;
    for (volatile uint32_t i = 0; i < 1000000; i++) {
        if (PLL0STAT & PLOCK0) { lock_ok = 1; break; }
    }
    // Fall back to sysclk unmultiplied: CLKSRCSEL was already set to the
    // crystal above, PLL0 stays disconnected -> CCLK = 12 MHz raw crystal
    // (not the 4 MHz IRC — that fallback only applies if the crystal
    // itself never started, see the osc_ok check above).
    if (!lock_ok) { g_clock_tier = 2; return; }

    // 5. CPU clock divider BEFORE connecting: 288 / 3 = 96 MHz
    CCLKCFG = 2;
    DSB;

    // 6. Flash accelerator: FLASHTIM (bits 15:12) = 4 -> 5 clocks, up to 100 MHz
    FLASHCFG = (FLASHCFG & ~0xF000u) | (4u << 12);
    DSB;

    // 7. Connect PLL0 as system clock, with timeout (PLOCK0 already
    // confirmed above, so this should be near-instant; guard anyway —
    // an unprotected wait here would reintroduce the same hang class
    // as the oscillator/lock waits above).
    PLL0CON = 3;
    pll_feed();

    uint32_t connect_ok = 0;
    for (volatile uint32_t i = 0; i < 1000000; i++) {
        if ((PLL0STAT & (PLLC0_STAT | PLLE0_STAT)) == (PLLC0_STAT | PLLE0_STAT)) {
            connect_ok = 1;
            break;
        }
    }
    if (!connect_ok) {
        // Locked but never connected: disable PLL0 and fall back to the
        // unmultiplied sysclk (12 MHz crystal) already selected above.
        PLL0CON = 0;
        pll_feed();
        g_clock_tier = 3;
        return;
    }

    g_clock_tier = 4;
    ISB;
}
