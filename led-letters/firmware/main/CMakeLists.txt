idf_component_register(
    SRCS "main.c" "effects.c" "power.c" "lanes.c" "output.c" "settings.c" "web.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES driver esp_lcd esp_timer nvs_flash esp_wifi esp_netif esp_event esp_http_server
)
