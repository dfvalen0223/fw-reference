#include "settings.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

static const char *TAG = "settings";
static const char *NVS_NS = "letters";
static const char *NVS_KEY = "cfg";
#define SAVE_DELAY_MS 3000  // don't wear the flash while someone drags a slider

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static app_settings_t s_cur;
static bool s_dirty;
static uint32_t s_changed_ms;
static bool s_change_pending_stamp;
static volatile uint32_t s_last_ma;

void settings_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    s_cur = (app_settings_t){
        .fx = {.effect = FX_RAINBOW, .color = {255, 120, 20}, .speed = 128},
        .brightness = CONFIG_LETTERS_DEFAULT_BRIGHTNESS,
    };
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        app_settings_t stored;
        size_t len = sizeof(stored);
        if (nvs_get_blob(h, NVS_KEY, &stored, &len) == ESP_OK && len == sizeof(stored) &&
            stored.fx.effect < FX_COUNT) {
            s_cur = stored;
            ESP_LOGI(TAG, "restored effect=%s brightness=%u", fx_name(stored.fx.effect), stored.brightness);
        }
        nvs_close(h);
    }
}

app_settings_t settings_get(void)
{
    taskENTER_CRITICAL(&s_lock);
    app_settings_t s = s_cur;
    taskEXIT_CRITICAL(&s_lock);
    return s;
}

void settings_set(const app_settings_t *s)
{
    taskENTER_CRITICAL(&s_lock);
    s_cur = *s;
    if (s_cur.fx.effect >= FX_COUNT) s_cur.fx.effect = FX_SOLID;
    s_dirty = true;
    s_change_pending_stamp = true;
    taskEXIT_CRITICAL(&s_lock);
}

void settings_save_if_dirty(uint32_t now_ms)
{
    taskENTER_CRITICAL(&s_lock);
    if (s_change_pending_stamp) {
        s_changed_ms = now_ms;
        s_change_pending_stamp = false;
    }
    bool due = s_dirty && (now_ms - s_changed_ms) >= SAVE_DELAY_MS;
    app_settings_t snapshot = s_cur;
    if (due) s_dirty = false;
    taskEXIT_CRITICAL(&s_lock);
    if (!due) return;

    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    if (nvs_set_blob(h, NVS_KEY, &snapshot, sizeof(snapshot)) == ESP_OK) nvs_commit(h);
    nvs_close(h);
}

void settings_publish_ma(uint32_t ma) { s_last_ma = ma; }
uint32_t settings_last_ma(void) { return s_last_ma; }
