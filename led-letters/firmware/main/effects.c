#include "effects.h"

#include <string.h>

static const char *const NAMES[FX_COUNT] = {
    "solid", "rainbow", "letter_chase", "wipe", "breathe", "twinkle", "letters", "chain_test",
};

const char *fx_name(fx_id_t id)
{
    return (id < FX_COUNT) ? NAMES[id] : "?";
}

rgb_t fx_hsv(uint8_t h, uint8_t s, uint8_t v)
{
    uint8_t region = h / 43;
    uint8_t rem = (uint8_t)((h - region * 43) * 6);
    uint8_t p = (uint8_t)((v * (255 - s)) >> 8);
    uint8_t q = (uint8_t)((v * (255 - ((s * rem) >> 8))) >> 8);
    uint8_t t = (uint8_t)((v * (255 - ((s * (255 - rem)) >> 8))) >> 8);
    switch (region) {
    case 0: return (rgb_t){v, t, p};
    case 1: return (rgb_t){q, v, p};
    case 2: return (rgb_t){p, v, t};
    case 3: return (rgb_t){p, q, v};
    case 4: return (rgb_t){t, p, v};
    default: return (rgb_t){v, p, q};
    }
}

static rgb_t scale(rgb_t c, uint8_t k)
{
    return (rgb_t){(uint8_t)((c.r * k) / 255), (uint8_t)((c.g * k) / 255), (uint8_t)((c.b * k) / 255)};
}

// 0..255..0 triangle wave.
static uint8_t tri(uint32_t phase)
{
    uint32_t x = phase & 0x1FF;
    return (uint8_t)(x < 256 ? x : 511 - x);
}

// Stateless per-pixel pseudo-random value, so TWINKLE needs no buffers.
static uint32_t hash32(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

// Effect time base: speed 128 ~= 1x, 255 ~= 2x, 1 ~= 1/128x.
static uint32_t scaled_time(uint32_t t_ms, uint8_t speed)
{
    return (uint32_t)(((uint64_t)t_ms * (speed ? speed : 1)) / 128);
}

void fx_render(const fx_settings_t *s, const fx_layout_t *L, uint32_t t_ms, rgb_t *out)
{
    const uint32_t t = scaled_time(t_ms, s->speed);
    const int32_t w = L->width_mm > 0 ? L->width_mm : 1;
    memset(out, 0, L->count * sizeof(rgb_t));

    switch (s->effect) {
    case FX_SOLID:
        for (size_t i = 0; i < L->count; i++) out[i] = s->color;
        break;

    case FX_RAINBOW:
        // One full hue cycle across the word, scrolling.
        for (size_t i = 0; i < L->count; i++) {
            uint8_t h = (uint8_t)((L->map[i].x_mm * 255) / w + t / 8);
            out[i] = fx_hsv(h, 255, 255);
        }
        break;

    case FX_LETTER_CHASE: {
        if (L->letter_count == 0) break;
        size_t lit = (t / 600) % L->letter_count;
        const led_letter_t *le = &L->letters[lit];
        for (size_t i = le->first_led; i < (size_t)le->first_led + le->led_count && i < L->count; i++)
            out[i] = s->color;
        break;
    }

    case FX_WIPE: {
        // Front sweeps across (0..w) then back out, period 2*w mm at 0.25 mm/ms.
        int32_t pos = (int32_t)((t / 4) % (uint32_t)(2 * w));
        for (size_t i = 0; i < L->count; i++) {
            int32_t x = L->map[i].x_mm;
            int on = pos < w ? (x <= pos) : (x > pos - w);
            if (on) out[i] = s->color;
        }
        break;
    }

    case FX_BREATHE: {
        uint8_t k = tri(t / 4);
        // Keep a floor so the sign never looks switched off.
        k = (uint8_t)(24 + (k * (255 - 24)) / 255);
        for (size_t i = 0; i < L->count; i++) out[i] = scale(s->color, k);
        break;
    }

    case FX_TWINKLE:
        for (size_t i = 0; i < L->count; i++) {
            uint32_t hsh = hash32((uint32_t)i * 2654435761U);
            uint8_t k = tri(t / 3 + hsh);
            k = (uint8_t)((k * k) >> 8);  // sharpen: mostly dim, brief bright peaks
            out[i] = scale(s->color, (uint8_t)(40 + (k * 215) / 255));
        }
        break;

    case FX_LETTERS:
        for (size_t l = 0; l < L->letter_count; l++) {
            rgb_t c = fx_hsv((uint8_t)((l * 255) / (L->letter_count ? L->letter_count : 1)), 255, 255);
            const led_letter_t *le = &L->letters[l];
            for (size_t i = le->first_led; i < (size_t)le->first_led + le->led_count && i < L->count; i++)
                out[i] = c;
        }
        break;

    case FX_CHAIN_TEST: {
        // Walks the data chain at 4 pixels/s: compare with assembly_*.svg to
        // find a swapped or skipped socket.
        if (L->count == 0) break;
        size_t i = (t_ms / 250) % L->count;
        out[i] = (rgb_t){255, 255, 255};
        break;
    }

    default:
        break;
    }
}
