// Runtime settings shared by the render loop, the button and the web UI,
// persisted to NVS so the sign comes back the way it was left.
#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "effects.h"

typedef struct {
    fx_settings_t fx;
    uint8_t brightness;  // 0..255 master brightness (before the current limiter)
} app_settings_t;

void settings_init(void);  // loads from NVS (or defaults)
app_settings_t settings_get(void);
void settings_set(const app_settings_t *s);
void settings_save_if_dirty(uint32_t now_ms);  // debounced NVS write

// Last frame's estimated LED current, published by the render loop.
void settings_publish_ma(uint32_t ma);
uint32_t settings_last_ma(void);
