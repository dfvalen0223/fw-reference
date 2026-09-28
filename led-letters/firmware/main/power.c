#include "power.h"

#include <math.h>

// Gamma 2.2 so brightness steps look even to the eye. Table built once.
static uint8_t gamma8(uint8_t v)
{
    static uint8_t table[256];
    static int ready;
    if (!ready) {
        for (int i = 0; i < 256; i++)
            table[i] = (uint8_t)lroundf(powf(i / 255.0f, 2.2f) * 255.0f);
        ready = 1;
    }
    return table[v];
}

uint32_t power_estimate_ma(const rgb_t *px, size_t n)
{
    uint64_t sum = 0;
    for (size_t i = 0; i < n; i++) sum += (uint32_t)px[i].r + px[i].g + px[i].b;
    return (uint32_t)(n * POWER_MA_IDLE_PER_PIXEL + (sum * POWER_MA_PER_CHANNEL) / 255);
}

uint32_t power_finalize(const rgb_t *in, rgb_t *out, size_t n, uint8_t brightness, uint32_t max_ma)
{
    for (size_t i = 0; i < n; i++) {
        out[i].r = gamma8((uint8_t)((in[i].r * brightness) / 255));
        out[i].g = gamma8((uint8_t)((in[i].g * brightness) / 255));
        out[i].b = gamma8((uint8_t)((in[i].b * brightness) / 255));
    }
    uint32_t ma = power_estimate_ma(out, n);
    uint32_t idle = (uint32_t)n * POWER_MA_IDLE_PER_PIXEL;
    if (ma <= max_ma || ma <= idle) return ma;

    // Scale the LED (non-idle) part of the current down to fit the budget.
    uint32_t budget = max_ma > idle ? max_ma - idle : 0;
    uint32_t k = (uint32_t)(((uint64_t)budget * 256) / (ma - idle));  // 0..255 (fixed point /256)
    for (size_t i = 0; i < n; i++) {
        out[i].r = (uint8_t)((out[i].r * k) >> 8);
        out[i].g = (uint8_t)((out[i].g * k) >> 8);
        out[i].b = (uint8_t)((out[i].b * k) >> 8);
    }
    return power_estimate_ma(out, n);
}
