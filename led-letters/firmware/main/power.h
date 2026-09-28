// Output stage: master brightness, gamma correction and a current limiter so
// the LEDs never draw more than the power supply is rated for. Pure C, unit
// tested on the host (firmware/test/).
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "effects.h"

// WS2812B: ~20 mA per colour channel at full duty, ~1 mA quiescent per pixel.
#define POWER_MA_PER_CHANNEL 20u
#define POWER_MA_IDLE_PER_PIXEL 1u

// Estimated current (mA) of a frame exactly as it will be sent.
uint32_t power_estimate_ma(const rgb_t *px, size_t n);

// Applies gamma + `brightness` to `in`, writing `out`, then scales the whole
// frame down (never up) so the estimate stays <= max_ma. Returns the
// estimated current of `out`.
uint32_t power_finalize(const rgb_t *in, rgb_t *out, size_t n, uint8_t brightness, uint32_t max_ma);
