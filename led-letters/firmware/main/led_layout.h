// Types for the physical LED layout. The data itself lives in the generated
// letters_config.h, which only main.c includes (it defines static arrays).
#pragma once
#include <stdint.h>

typedef struct {
    char glyph;
    uint16_t first_led;  // index of the letter's first pixel in LED_MAP
    uint16_t led_count;
    uint8_t output;      // controller output (OUT n) the letter is wired to
    uint32_t max_ma;     // current budget of the letter's own 5 V supply
} led_letter_t;

typedef struct {
    int16_t x_mm;    // pixel position in the word, origin bottom-left
    int16_t y_mm;
    uint8_t letter;  // index into LED_LETTERS
} led_pos_t;
