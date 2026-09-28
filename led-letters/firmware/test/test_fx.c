// Host unit tests for the pure-C parts of the firmware (effects + power).
// Build and run: make -C firmware/test
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "effects.h"
#include "lanes.h"
#include "power.h"

static int failures;
#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);         \
            failures++;                                                      \
        }                                                                    \
    } while (0)

// Two letters, 3 + 2 pixels, spread across a 400 mm wide word.
static const led_letter_t LETTERS[] = {{'H', 0, 3, 0, 1000}, {'I', 3, 2, 1, 1000}};
static const led_pos_t MAP[] = {
    {0, 10, 0}, {50, 100, 0}, {100, 190, 0}, {300, 20, 1}, {400, 180, 1},
};
#define N (sizeof(MAP) / sizeof(MAP[0]))
static const fx_layout_t LAYOUT = {MAP, N, LETTERS, 2, 400, 200};

static int is_black(rgb_t c) { return c.r == 0 && c.g == 0 && c.b == 0; }
static int eq(rgb_t a, rgb_t b) { return a.r == b.r && a.g == b.g && a.b == b.b; }

static void test_solid(void)
{
    rgb_t out[N];
    fx_settings_t s = {FX_SOLID, {10, 20, 30}, 128};
    fx_render(&s, &LAYOUT, 1234, out);
    for (size_t i = 0; i < N; i++) CHECK(eq(out[i], s.color));
}

static void test_rainbow_varies_with_x(void)
{
    rgb_t out[N];
    fx_settings_t s = {FX_RAINBOW, {0, 0, 0}, 128};
    fx_render(&s, &LAYOUT, 0, out);
    CHECK(!eq(out[0], out[3]));  // x=0 vs x=300 -> different hue
    for (size_t i = 0; i < N; i++) CHECK(!is_black(out[i]));
}

static void test_letter_chase_one_letter_at_a_time(void)
{
    rgb_t out[N];
    fx_settings_t s = {FX_LETTER_CHASE, {255, 0, 0}, 128};
    fx_render(&s, &LAYOUT, 0, out);  // first letter
    CHECK(!is_black(out[0]) && !is_black(out[2]) && is_black(out[3]) && is_black(out[4]));
    fx_render(&s, &LAYOUT, 600, out);  // second letter
    CHECK(is_black(out[0]) && !is_black(out[3]) && !is_black(out[4]));
}

static void test_chain_test_lights_exactly_one(void)
{
    rgb_t out[N];
    fx_settings_t s = {FX_CHAIN_TEST, {0, 0, 0}, 128};
    for (uint32_t t = 0; t < 250 * N * 2; t += 250) {
        fx_render(&s, &LAYOUT, t, out);
        int lit = 0;
        for (size_t i = 0; i < N; i++) lit += !is_black(out[i]);
        CHECK(lit == 1);
        CHECK(!is_black(out[(t / 250) % N]));
    }
}

static void test_wipe_goes_left_to_right(void)
{
    rgb_t out[N];
    fx_settings_t s = {FX_WIPE, {0, 255, 0}, 128};
    fx_render(&s, &LAYOUT, 4 * 60, out);  // front at x=60
    CHECK(!is_black(out[0]) && !is_black(out[1]) && is_black(out[2]) && is_black(out[4]));
}

static void test_breathe_never_fully_off(void)
{
    rgb_t out[N];
    fx_settings_t s = {FX_BREATHE, {255, 255, 255}, 128};
    for (uint32_t t = 0; t < 5000; t += 37) {
        fx_render(&s, &LAYOUT, t, out);
        CHECK(!is_black(out[0]));
    }
}

static void test_every_effect_is_safe(void)
{
    rgb_t out[N];
    for (int e = 0; e < FX_COUNT; e++) {
        fx_settings_t s = {(fx_id_t)e, {200, 100, 50}, 255};
        for (uint32_t t = 0; t < 100000; t += 997) fx_render(&s, &LAYOUT, t, out);
        CHECK(strcmp(fx_name((fx_id_t)e), "?") != 0);
    }
    fx_layout_t empty = {MAP, 0, LETTERS, 0, 0, 0};
    for (int e = 0; e < FX_COUNT; e++) {
        fx_settings_t s = {(fx_id_t)e, {1, 2, 3}, 0};
        fx_render(&s, &empty, 5000, out);  // must not divide by zero / index out of range
    }
}

static void test_power_limits_current(void)
{
    enum { M = 400 };
    static rgb_t in[M], out[M];
    for (int i = 0; i < M; i++) in[i] = (rgb_t){255, 255, 255};
    // Unlimited: full white = 400 * (1 + 60) mA.
    uint32_t full = power_finalize(in, out, M, 255, 100000);
    CHECK(full == M * (POWER_MA_IDLE_PER_PIXEL + 3 * POWER_MA_PER_CHANNEL));
    // A 5 A budget must be respected, and the frame must still be lit.
    uint32_t ma = power_finalize(in, out, M, 255, 5000);
    CHECK(ma <= 5000);
    CHECK(ma > 4000);
    CHECK(!is_black(out[0]));
    // Under budget: untouched.
    for (int i = 0; i < M; i++) in[i] = (rgb_t){60, 0, 0};  // dim, but not lost to gamma
    uint32_t low = power_finalize(in, out, M, 255, 5000);
    CHECK(low < 5000);
    CHECK(out[0].r > 0);
}

static void test_brightness_and_gamma(void)
{
    rgb_t in[1] = {{255, 128, 0}}, out[1];
    power_finalize(in, out, 1, 0, 1000);
    CHECK(is_black(out[0]));
    power_finalize(in, out, 1, 255, 1000);
    CHECK(out[0].r == 255 && out[0].b == 0);
    CHECK(out[0].g > 45 && out[0].g < 65);  // 128 -> ~56 with gamma 2.2
}

// Reads lane k back out of an encoded buffer, checking the slot pattern of
// every bit. Returns the number of LEDs recovered before the lane went idle.
static size_t decode_lane(const void *buf, int bus_bits, int k, size_t max_leds, uint8_t *out)
{
    size_t slot = 0, leds = 0;
    for (size_t led = 0; led < max_leds; led++) {
        int active = -1;
        for (int byte = 0; byte < 3; byte++) {
            uint8_t v = 0;
            for (int bit = 7; bit >= 0; bit--) {
                uint16_t s[4];
                for (int i = 0; i < 4; i++)
                    s[i] = bus_bits == 16 ? ((const uint16_t *)buf)[slot + i] : ((const uint8_t *)buf)[slot + i];
                slot += 4;
                int start = s[0] >> k & 1, one = s[1] >> k & 1;
                CHECK((s[2] >> k & 1) == 0 && (s[3] >> k & 1) == 0);  // every bit ends low
                CHECK(!one || start);                                  // high can't start late
                if (active < 0) active = start;
                CHECK(start == active);                                // whole LED on or idle
                v = (uint8_t)(v << 1 | one);
            }
            if (active) out[led * 3 + byte] = v;
        }
        if (active) leds++;
    }
    return leds;
}

static void test_lanes_roundtrip(int bus_bits, size_t lanes)
{
    enum { MAXL = 7 };
    static uint8_t data[LANES_MAX][MAXL * 3], back[MAXL * 3];
    const uint8_t *ptr[LANES_MAX];
    size_t len[LANES_MAX];
    uint32_t seed = 12345;
    for (size_t k = 0; k < lanes; k++) {
        len[k] = (k * 3) % (MAXL + 1);  // includes empty and full-length lanes
        for (size_t i = 0; i < MAXL * 3; i++) {
            seed = seed * 1103515245u + 12345u;
            data[k][i] = (uint8_t)(seed >> 16);
        }
        ptr[k] = data[k];
    }
    size_t bytes = lanes_buffer_bytes(MAXL, bus_bits);
    CHECK(bytes == (size_t)MAXL * 24 * 4 * (bus_bits / 8));
    uint8_t *buf = calloc(1, bytes);
    lanes_encode(buf, bus_bits, lanes, ptr, len, MAXL);
    for (size_t k = 0; k < lanes; k++) {
        memset(back, 0, sizeof(back));
        CHECK(decode_lane(buf, bus_bits, (int)k, MAXL, back) == len[k]);
        CHECK(memcmp(back, data[k], len[k] * 3) == 0);
    }
    // Unused bus lines stay low for the whole frame.
    for (size_t i = 0; i < bytes / (bus_bits / 8); i++) {
        uint16_t w = bus_bits == 16 ? ((uint16_t *)buf)[i] : buf[i];
        CHECK((w >> lanes) == 0);
    }
    free(buf);
}

static void test_lane_timing(void)
{
    // 3.2 MHz slots: '0' = 312.5 ns high, '1' = 625 ns high, 1.25 us per bit.
    CHECK(LANES_SLOT_HZ / 1000 * LANES_SLOTS_PER_BIT == 12800);
    CHECK(1000000000u / LANES_SLOT_HZ == 312);
}

int main(void)
{
    test_solid();
    test_rainbow_varies_with_x();
    test_letter_chase_one_letter_at_a_time();
    test_chain_test_lights_exactly_one();
    test_wipe_goes_left_to_right();
    test_breathe_never_fully_off();
    test_every_effect_is_safe();
    test_power_limits_current();
    test_brightness_and_gamma();
    test_lanes_roundtrip(8, 8);
    test_lanes_roundtrip(8, 3);
    test_lanes_roundtrip(16, 16);
    test_lanes_roundtrip(16, 11);
    test_lane_timing();
    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("all firmware host tests passed\n");
    return 0;
}
