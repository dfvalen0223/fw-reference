#include "output.h"

#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_io_i80.h"
#include "esp_lcd_panel_io.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"

#include "lanes.h"

static const char *TAG = "output";

static esp_lcd_panel_io_handle_t s_io;
static SemaphoreHandle_t s_done;
static void *s_buf;
static size_t s_buf_bytes;
static int s_bus_bits;
static size_t s_lanes;
static size_t s_max_leds;
static const led_letter_t *s_letters;
static size_t s_letter_count;
static uint8_t *s_wire[LANES_MAX];   // per-lane LED bytes in wire order
static size_t s_lane_leds[LANES_MAX];

static bool on_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *ev, void *ctx)
{
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(s_done, &woken);
    return woken == pdTRUE;
}

// "4,5,6" -> {4, 5, 6}; returns how many were parsed.
static size_t parse_gpios(const char *s, int *out, size_t max)
{
    size_t n = 0;
    while (*s && n < max) {
        char *end;
        long v = strtol(s, &end, 10);
        if (end == s) break;
        out[n++] = (int)v;
        s = (*end == ',') ? end + 1 : end;
    }
    return n;
}

esp_err_t output_init(const led_letter_t *letters, size_t letter_count)
{
    s_letters = letters;
    s_letter_count = letter_count;
    for (size_t l = 0; l < letter_count; l++) {
        size_t k = letters[l].output;
        ESP_RETURN_ON_FALSE(k < LANES_MAX, ESP_ERR_INVALID_ARG, TAG, "output %u out of range", (unsigned)k);
        s_lane_leds[k] += letters[l].led_count;
        if (k + 1 > s_lanes) s_lanes = k + 1;
    }
    for (size_t k = 0; k < s_lanes; k++) {
        if (s_lane_leds[k] > s_max_leds) s_max_leds = s_lane_leds[k];
        s_wire[k] = calloc(s_lane_leds[k] ? s_lane_leds[k] * 3 : 1, 1);
        ESP_RETURN_ON_FALSE(s_wire[k], ESP_ERR_NO_MEM, TAG, "no memory");
    }

    // The LCD bus is 8 or 16 lines wide and the driver needs a real pin for
    // every line, even the ones no letter uses (they just stay low).
    s_bus_bits = s_lanes <= 8 ? 8 : 16;
    int gpios[LANES_MAX];
    size_t n = parse_gpios(CONFIG_LETTERS_OUT_GPIOS, gpios, LANES_MAX);
    ESP_RETURN_ON_FALSE(n >= (size_t)s_bus_bits, ESP_ERR_INVALID_ARG, TAG,
                        "%u outputs need a %d-line bus but LETTERS_OUT_GPIOS lists %u pins",
                        (unsigned)s_lanes, s_bus_bits, (unsigned)n);
    s_buf_bytes = lanes_buffer_bytes(s_max_leds, s_bus_bits);

    esp_lcd_i80_bus_config_t bus_cfg = {
        .dc_gpio_num = CONFIG_LETTERS_BUS_DC_GPIO,
        .wr_gpio_num = CONFIG_LETTERS_BUS_WR_GPIO,
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .bus_width = (size_t)s_bus_bits,
        .max_transfer_bytes = s_buf_bytes,
        .dma_burst_size = 64,
    };
    for (int i = 0; i < s_bus_bits; i++) bus_cfg.data_gpio_nums[i] = gpios[i];
    esp_lcd_i80_bus_handle_t bus;
    ESP_RETURN_ON_ERROR(esp_lcd_new_i80_bus(&bus_cfg, &bus), TAG, "i80 bus");

    s_done = xSemaphoreCreateBinary();
    ESP_RETURN_ON_FALSE(s_done, ESP_ERR_NO_MEM, TAG, "no memory");
    xSemaphoreGive(s_done);
    esp_lcd_panel_io_i80_config_t io_cfg = {
        .cs_gpio_num = -1,
        .pclk_hz = LANES_SLOT_HZ,
        .trans_queue_depth = 2,
        .on_color_trans_done = on_done,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .dc_levels = {.dc_data_level = 1},
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i80(bus, &io_cfg, &s_io), TAG, "i80 io");

    s_buf = esp_lcd_i80_alloc_draw_buffer(s_io, s_buf_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    ESP_RETURN_ON_FALSE(s_buf, ESP_ERR_NO_MEM, TAG, "no DMA memory for %u bytes", (unsigned)s_buf_bytes);
    ESP_LOGI(TAG, "%u outputs on a %d-bit bus, longest %u LEDs, %u-byte DMA frame (%u us on the wire)",
             (unsigned)s_lanes, s_bus_bits, (unsigned)s_max_leds, (unsigned)s_buf_bytes,
             (unsigned)(s_max_leds * 30));
    return ESP_OK;
}

esp_err_t output_show(const rgb_t *px)
{
    // Wait for the previous frame to leave the DMA buffer before reusing it.
    if (xSemaphoreTake(s_done, pdMS_TO_TICKS(100)) != pdTRUE) return ESP_ERR_TIMEOUT;

    size_t fill[LANES_MAX] = {0};
    for (size_t l = 0; l < s_letter_count; l++) {
        const led_letter_t *le = &s_letters[l];
        uint8_t *w = s_wire[le->output] + fill[le->output] * 3;
        for (size_t i = 0; i < le->led_count; i++) {
            rgb_t c = px[le->first_led + i];
#if CONFIG_LETTERS_COLOR_ORDER_RGB
            w[i * 3 + 0] = c.r, w[i * 3 + 1] = c.g, w[i * 3 + 2] = c.b;
#else
            w[i * 3 + 0] = c.g, w[i * 3 + 1] = c.r, w[i * 3 + 2] = c.b;
#endif
        }
        fill[le->output] += le->led_count;
    }
    lanes_encode(s_buf, s_bus_bits, s_lanes, (const uint8_t *const *)s_wire, s_lane_leds, s_max_leds);
    // lcd_cmd -1: no command phase, only the data stream.
    return esp_lcd_panel_io_tx_color(s_io, -1, s_buf, s_buf_bytes);
}
