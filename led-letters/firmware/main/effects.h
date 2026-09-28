// Effect engine. Pure C (no ESP-IDF dependency) so it is unit-tested on the
// host: see firmware/test/.
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "led_layout.h"

typedef struct {
    uint8_t r, g, b;
} rgb_t;

typedef enum {
    FX_SOLID = 0,     // whole word in one colour
    FX_RAINBOW,       // hue sweeps across the word by physical X position
    FX_LETTER_CHASE,  // one letter lit at a time, left to right
    FX_WIPE,          // colour wipes left to right, then clears
    FX_BREATHE,       // whole word fades in and out
    FX_TWINKLE,       // random-looking sparkle over a dim base
    FX_LETTERS,       // each letter a different hue (handy to check wiring)
    FX_CHAIN_TEST,    // one pixel at a time in data-chain order (assembly check)
    FX_COUNT
} fx_id_t;

typedef struct {
    fx_id_t effect;
    rgb_t color;    // base colour for SOLID / WIPE / BREATHE / TWINKLE / CHASE
    uint8_t speed;  // 1 (slow) .. 255 (fast)
} fx_settings_t;

// Physical layout the effects are drawn on (normally LED_MAP / LED_LETTERS
// from the generated letters_config.h).
typedef struct {
    const led_pos_t *map;
    size_t count;
    const led_letter_t *letters;
    size_t letter_count;
    int32_t width_mm;
    int32_t height_mm;
} fx_layout_t;

const char *fx_name(fx_id_t id);

// Render one frame at time t_ms into out[0..layout->count).
void fx_render(const fx_settings_t *s, const fx_layout_t *layout, uint32_t t_ms, rgb_t *out);

rgb_t fx_hsv(uint8_t h, uint8_t s, uint8_t v);
