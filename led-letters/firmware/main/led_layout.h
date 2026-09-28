// Types for the physical LED layout. The data itself lives in the generated
// letters_config.h, which only main.c includes (it defines static arrays).
#pragma once
#include <stdint.h>

typedef struct {
    char glyph;
    uint16_t first_led;  // index of the letter's first pixel in the data chain
    uint16_t led_count;
} led_letter_t;

typedef struct {
    int16_t x_mm;    // pixel position in the word, origin bottom-left
    int16_t y_mm;
    uint8_t letter;  // index into LED_LETTERS
} led_pos_t;
