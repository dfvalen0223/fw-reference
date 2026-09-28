// Host unit tests for the pure-C parts of the firmware (effects + power).
// Build and run: make -C led-letters/firmware/test
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "effects.h"
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
static const led_letter_t LETTERS[] = {{'H', 0, 3}, {'I', 3, 2}};
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
    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("all firmware host tests passed\n");
    return 0;
}
