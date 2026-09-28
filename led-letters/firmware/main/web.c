#include "web.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "sdkconfig.h"

#include "settings.h"

static const char *TAG = "web";
static const fx_layout_t *s_layout;

static const char PAGE[] =
    "<!doctype html><html><head><meta charset=utf-8>"
    "<meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>Letras LED</title><style>"
    "body{font-family:sans-serif;margin:16px;background:#111;color:#eee}"
    "h1{font-size:1.4em}button{margin:4px;padding:12px 14px;border:0;border-radius:8px;"
    "background:#333;color:#eee;font-size:1em}button.on{background:#e80;color:#000}"
    "label{display:block;margin-top:16px}input[type=range]{width:100%}"
    "input[type=color]{width:100%;height:48px;border:0;background:none}small{color:#999}"
    "</style></head><body><h1 id=t>Letras LED</h1><div id=fx></div>"
    "<label>Color<input type=color id=c></label>"
    "<label>Brillo <span id=bv></span><input type=range id=b min=0 max=255></label>"
    "<label>Velocidad <span id=sv></span><input type=range id=s min=1 max=255></label>"
    "<p><small id=info></small></p><script>"
    "const $=i=>document.getElementById(i);let st={};"
    "function draw(){$('t').textContent=st.text;$('fx').innerHTML='';"
    "st.effects.forEach(e=>{const b=document.createElement('button');b.textContent=e;"
    "b.className=e==st.effect?'on':'';b.onclick=()=>set({effect:e});$('fx').appendChild(b)});"
    "$('c').value='#'+st.color;$('b').value=st.brightness;$('s').value=st.speed;"
    "$('bv').textContent=st.brightness;$('sv').textContent=st.speed;"
    "$('info').textContent=st.leds+' LEDs, ~'+(st.ma/1000).toFixed(1)+' A (limit '+(st.limit_ma/1000)+' A)'}"
    "function set(p){fetch('/api/set?'+new URLSearchParams(p)).then(r=>r.json()).then(j=>{st=j;draw()})}"
    "$('c').oninput=e=>set({color:e.target.value.slice(1)});"
    "$('b').onchange=e=>set({brightness:e.target.value});"
    "$('s').onchange=e=>set({speed:e.target.value});"
    "fetch('/api/state').then(r=>r.json()).then(j=>{st=j;draw()});"
    "setInterval(()=>fetch('/api/state').then(r=>r.json()).then(j=>{st=j;"
    "$('info').textContent=st.leds+' LEDs, ~'+(st.ma/1000).toFixed(1)+' A (limit '+(st.limit_ma/1000)+' A)'}),2000);"
    "</script></body></html>";

static esp_err_t send_state(httpd_req_t *req)
{
    app_settings_t s = settings_get();
    char text[64] = {0};
    size_t n = 0;
    for (size_t i = 0; i < s_layout->letter_count && n + 1 < sizeof(text); i++) {
        char g = s_layout->letters[i].glyph;
        // Only plain printable ASCII goes into the JSON string.
        if (g >= ' ' && g <= '~' && g != '"' && g != '\\') text[n++] = g;
    }
    char effects[256] = {0};
    size_t e = 0;
    for (int i = 0; i < FX_COUNT && e < sizeof(effects); i++)
        e += (size_t)snprintf(effects + e, sizeof(effects) - e, "%s\"%s\"", i ? "," : "", fx_name((fx_id_t)i));

    char body[512];
    int len = snprintf(body, sizeof(body),
                       "{\"text\":\"%s\",\"effect\":\"%s\",\"effects\":[%s],\"color\":\"%02x%02x%02x\","
                       "\"brightness\":%u,\"speed\":%u,\"leds\":%u,\"ma\":%lu,\"limit_ma\":%d}",
                       text, fx_name(s.fx.effect), effects, s.fx.color.r, s.fx.color.g, s.fx.color.b,
                       s.brightness, s.fx.speed, (unsigned)s_layout->count,
                       (unsigned long)settings_last_ma(), CONFIG_LETTERS_MAX_CURRENT_MA);
    if (len < 0) return httpd_resp_send_500(req);
    if ((size_t)len >= sizeof(body)) len = sizeof(body) - 1;
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, body, len);
}

static esp_err_t page_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, PAGE, sizeof(PAGE) - 1);
}

static esp_err_t state_get(httpd_req_t *req) { return send_state(req); }

static int clamp_u8(const char *v, int lo)
{
    long x = strtol(v, NULL, 10);
    return x < lo ? lo : x > 255 ? 255 : (int)x;
}

static esp_err_t set_get(httpd_req_t *req)
{
    char q[160], v[32];
    if (httpd_req_get_url_query_str(req, q, sizeof(q)) != ESP_OK) return send_state(req);

    app_settings_t s = settings_get();
    if (httpd_query_key_value(q, "effect", v, sizeof(v)) == ESP_OK) {
        for (int i = 0; i < FX_COUNT; i++)
            if (strcmp(v, fx_name((fx_id_t)i)) == 0) s.fx.effect = (fx_id_t)i;
    }
    if (httpd_query_key_value(q, "color", v, sizeof(v)) == ESP_OK && strlen(v) == 6) {
        char *end;
        unsigned long rgb = strtoul(v, &end, 16);
        if (*end == '\0') s.fx.color = (rgb_t){(uint8_t)(rgb >> 16), (uint8_t)(rgb >> 8), (uint8_t)rgb};
    }
    if (httpd_query_key_value(q, "brightness", v, sizeof(v)) == ESP_OK) s.brightness = (uint8_t)clamp_u8(v, 0);
    if (httpd_query_key_value(q, "speed", v, sizeof(v)) == ESP_OK) s.fx.speed = (uint8_t)clamp_u8(v, 1);
    settings_set(&s);
    return send_state(req);
}

static void wifi_ap_start(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wc = {
        .ap = {
            .channel = CONFIG_LETTERS_AP_CHANNEL,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    strlcpy((char *)wc.ap.ssid, CONFIG_LETTERS_AP_SSID, sizeof(wc.ap.ssid));
    wc.ap.ssid_len = (uint8_t)strlen((char *)wc.ap.ssid);
    strlcpy((char *)wc.ap.password, CONFIG_LETTERS_AP_PASSWORD, sizeof(wc.ap.password));
    if (strlen(CONFIG_LETTERS_AP_PASSWORD) < 8) {
        ESP_LOGW(TAG, "AP password shorter than 8 characters: access point is OPEN");
        wc.ap.authmode = WIFI_AUTH_OPEN;
    }
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Wi-Fi AP \"%s\" up, open http://192.168.4.1", CONFIG_LETTERS_AP_SSID);
}

void web_start(const fx_layout_t *layout)
{
    s_layout = layout;
    wifi_ap_start();

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t srv = NULL;
    ESP_ERROR_CHECK(httpd_start(&srv, &cfg));
    static const httpd_uri_t routes[] = {
        {.uri = "/", .method = HTTP_GET, .handler = page_get},
        {.uri = "/api/state", .method = HTTP_GET, .handler = state_get},
        {.uri = "/api/set", .method = HTTP_GET, .handler = set_get},
    };
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++)
        ESP_ERROR_CHECK(httpd_register_uri_handler(srv, &routes[i]));
}
