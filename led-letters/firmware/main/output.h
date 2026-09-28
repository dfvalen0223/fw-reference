// One WS2812 data output per letter (OUT 1..N on the controller), all sent
// in parallel by the ESP32-S3 LCD peripheral over DMA: no RMT channels, no
// CPU work while a frame is on the wire, up to 16 letters.
#pragma once
#include <stddef.h>

#include "effects.h"
#include "esp_err.h"
#include "led_layout.h"

esp_err_t output_init(const led_letter_t *letters, size_t letter_count);

// Sends one frame. px holds every LED of the word (LED_MAP order).
esp_err_t output_show(const rgb_t *px);
