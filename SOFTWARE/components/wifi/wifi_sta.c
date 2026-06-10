#include "wifi_sta.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"

static const char *TAG = "wifi";

#define BIT_CONNECTED       BIT0
#define WIFI_NVS_NS         "wifi_cfg"
#define WIFI_NVS_SSID       "ssid"
#define WIFI_NVS_PASS       "pass"
#define WIFI_NVS_PWR        "pwr_mode"
#define WIFI_NVS_WINDOW     "active_min"
#define FALLBACK_AP_SSID    "NIXIE-SETUP"
#define FALLBACK_AP_PASS    "nixie1234"
#define FALLBACK_AP_CHANNEL 6
#define FALLBACK_TIMEOUT_MS 15000
#define FALLBACK_RETRY_CNT  5
#define FALLBACK_BACKOFF_MS  300000
#define WIFI_SCAN_MAX_AP    20
#define WIFI_WINDOW_MIN_MINUTES 1u
#define WIFI_WINDOW_MAX_MINUTES 60u
#define WIFI_WINDOW_DEFAULT_MINUTES 5u

static EventGroupHandle_t s_eg;
static volatile bool s_connected = false;
static volatile bool s_ap_active = false;
static volatile bool s_wifi_started = false;
static volatile bool s_wifi_stopping = false;
static int s_disconnects = 0;
static char s_ssid[33] = {0};
static char s_pass[65] = {0};
static volatile wifi_sta_power_mode_t s_power_mode = WIFI_STA_PWR_BALANCED;
static volatile uint32_t s_active_window_min = WIFI_WINDOW_DEFAULT_MINUTES;
static volatile int64_t s_active_until_us = 0;
static volatile bool s_critical_activity = false;
static volatile bool s_retry_paused = false;

static void start_fallback_ap(void);
static void apply_power_save(void);
static esp_err_t start_wifi_if_needed(void);

static uint32_t clamp_window(uint32_t minutes)
{
    if (minutes < WIFI_WINDOW_MIN_MINUTES) return WIFI_WINDOW_MIN_MINUTES;
    if (minutes > WIFI_WINDOW_MAX_MINUTES) return WIFI_WINDOW_MAX_MINUTES;
    return minutes;
}

static const char *power_mode_name(wifi_sta_power_mode_t mode)
{
    switch (mode) {
    case WIFI_STA_PWR_PERFORMANCE: return "performance";
    case WIFI_STA_PWR_BALANCED:    return "balanced";
    case WIFI_STA_PWR_MAX_SAVE:    return "max_save";
    case WIFI_STA_PWR_ON_DEMAND:   return "on_demand";
    default:                       return "balanced";
    }
}

static void json_append_escaped(char *out, size_t out_len, size_t *n, const char *s)
{
    if (*n >= out_len) return;
    for (; s && *s && *n + 2 < out_len; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') {
            if (*n + 3 >= out_len) break;
            out[(*n)++] = '\\';
            out[(*n)++] = (char)c;
        } else if (c >= 0x20) {
            out[(*n)++] = (char)c;
        }
    }
    out[*n] = 0;
}

static bool load_saved_credentials(char *ssid, size_t ssid_len, char *pass, size_t pass_len)
{
    nvs_handle_t h;
    if (nvs_open(WIFI_NVS_NS, NVS_READONLY, &h) != ESP_OK) return false;

    size_t sl = ssid_len;
    size_t pl = pass_len;
    esp_err_t es = nvs_get_str(h, WIFI_NVS_SSID, ssid, &sl);
    esp_err_t ep = nvs_get_str(h, WIFI_NVS_PASS, pass, &pl);
    nvs_close(h);

    if (es != ESP_OK || ssid[0] == 0) return false;
    if (ep != ESP_OK) pass[0] = 0;
    return true;
}

static void load_power_settings(void)
{
    nvs_handle_t h;
    if (nvs_open(WIFI_NVS_NS, NVS_READONLY, &h) != ESP_OK) return;
    uint8_t p = WIFI_STA_PWR_BALANCED;
    uint32_t w = WIFI_WINDOW_DEFAULT_MINUTES;
    if (nvs_get_u8(h, WIFI_NVS_PWR, &p) == ESP_OK && p <= WIFI_STA_PWR_ON_DEMAND)
        s_power_mode = (wifi_sta_power_mode_t)p;
    if (nvs_get_u32(h, WIFI_NVS_WINDOW, &w) == ESP_OK)
        s_active_window_min = clamp_window(w);
    nvs_close(h);
}

static esp_err_t save_credentials(const char *ssid, const char *pass)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(WIFI_NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_str(h, WIFI_NVS_SSID, ssid ? ssid : "");
    if (err == ESP_OK) err = nvs_set_str(h, WIFI_NVS_PASS, pass ? pass : "");
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

static esp_err_t save_power_settings(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(WIFI_NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_u8(h, WIFI_NVS_PWR, (uint8_t)s_power_mode);
    if (err == ESP_OK) err = nvs_set_u32(h, WIFI_NVS_WINDOW, s_active_window_min);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

static void apply_sta_config(void)
{
    wifi_config_t wc = { 0 };
    strncpy((char *)wc.sta.ssid, s_ssid, sizeof(wc.sta.ssid) - 1);
    strncpy((char *)wc.sta.password, s_pass, sizeof(wc.sta.password) - 1);
    wc.sta.threshold.authmode = s_pass[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    // Used by WIFI_PS_MAX_MODEM. Higher value saves more power but increases latency.
    wc.sta.listen_interval = (s_power_mode == WIFI_STA_PWR_MAX_SAVE ||
                              s_power_mode == WIFI_STA_PWR_ON_DEMAND) ? 10 : 3;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
}

static void apply_power_save(void)
{
    wifi_ps_type_t ps = WIFI_PS_MIN_MODEM;
    if (s_power_mode == WIFI_STA_PWR_PERFORMANCE) ps = WIFI_PS_NONE;
    else if (s_power_mode == WIFI_STA_PWR_BALANCED) ps = WIFI_PS_MIN_MODEM;
    else ps = WIFI_PS_MAX_MODEM;

    esp_err_t err = esp_wifi_set_ps(ps);
    if (err != ESP_OK) ESP_LOGW(TAG, "esp_wifi_set_ps failed: %s", esp_err_to_name(err));
    else ESP_LOGI(TAG, "power mode: %s", power_mode_name(s_power_mode));
}

static void stop_fallback_ap_if_needed(void)
{
    if (!s_ap_active) return;
    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err == ESP_OK) {
        s_ap_active = false;
        ESP_LOGI(TAG, "fallback AP stopped");
    } else {
        ESP_LOGW(TAG, "fallback AP stop failed: %s", esp_err_to_name(err));
    }
}

static void start_fallback_ap(void)
{
    if (s_ap_active) return;

    ESP_LOGW(TAG, "STA not connected - starting fallback AP: %s / %s",
             FALLBACK_AP_SSID, FALLBACK_AP_PASS);

    start_wifi_if_needed();

    wifi_config_t ap = { 0 };
    strncpy((char *)ap.ap.ssid, FALLBACK_AP_SSID, sizeof(ap.ap.ssid) - 1);
    strncpy((char *)ap.ap.password, FALLBACK_AP_PASS, sizeof(ap.ap.password) - 1);
    ap.ap.ssid_len = strlen(FALLBACK_AP_SSID);
    ap.ap.channel = FALLBACK_AP_CHANNEL;
    ap.ap.max_connection = 4;
    ap.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    ap.ap.pmf_cfg.required = false;
    if (strlen(FALLBACK_AP_PASS) == 0) ap.ap.authmode = WIFI_AUTH_OPEN;

    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "fallback AP mode failed: %s", esp_err_to_name(err));
        return;
    }
    err = esp_wifi_set_config(WIFI_IF_AP, &ap);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "fallback AP config failed: %s", esp_err_to_name(err));
        return;
    }
    s_ap_active = true;
    wifi_sta_request_active_window(s_active_window_min);
}

static esp_err_t start_wifi_if_needed(void)
{
    if (s_wifi_started) return ESP_OK;
    esp_err_t err = esp_wifi_start();
    if (err == ESP_OK) {
        s_wifi_started = true;
        s_wifi_stopping = false;
        apply_power_save();
        return ESP_OK;
    }
    ESP_LOGW(TAG, "esp_wifi_start failed: %s", esp_err_to_name(err));
    return err;
}

static void stop_wifi_for_on_demand(void)
{
    if (!s_wifi_started || s_ap_active || s_critical_activity) return;
    ESP_LOGI(TAG, "on-demand window expired - stopping WiFi");
    s_wifi_stopping = true;
    esp_wifi_disconnect();
    esp_err_t err = esp_wifi_stop();
    if (err == ESP_OK) {
        s_wifi_started = false;
        s_connected = false;
        xEventGroupClearBits(s_eg, BIT_CONNECTED);
    } else {
        ESP_LOGW(TAG, "esp_wifi_stop failed: %s", esp_err_to_name(err));
    }
}

static void power_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        if (s_power_mode != WIFI_STA_PWR_ON_DEMAND) continue;
        if (s_active_until_us == 0) continue;
        if (esp_timer_get_time() > s_active_until_us) stop_wifi_for_on_demand();
    }
}

static void fallback_task(void *arg)
{
    if (!wifi_sta_wait_ip(FALLBACK_TIMEOUT_MS)) start_fallback_ap();
    vTaskDelete(NULL);
}

static void retry_backoff_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(FALLBACK_BACKOFF_MS));
        if (s_retry_paused && s_wifi_started && !s_connected && s_ssid[0] != 0) {
            ESP_LOGI(TAG, "retrying STA connection after backoff");
            s_disconnects = 0;
            s_retry_paused = false;
            apply_sta_config();
            esp_wifi_connect();
        }
    }
}

static void on_wifi(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        s_wifi_started = true;
        s_wifi_stopping = false;
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_STOP) {
        s_wifi_started = false;
        s_wifi_stopping = false;
        s_connected = false;
        xEventGroupClearBits(s_eg, BIT_CONNECTED);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_connected = false;
        xEventGroupClearBits(s_eg, BIT_CONNECTED);
        if (!s_wifi_started || s_wifi_stopping) return;
        s_disconnects++;
        if (s_disconnects >= FALLBACK_RETRY_CNT) {
            ESP_LOGW(TAG, "disconnected (%d), entering AP fallback and STA backoff", s_disconnects);
            start_fallback_ap();
            s_retry_paused = true;
            return;
        }
        ESP_LOGW(TAG, "disconnected (%d), reconnecting...", s_disconnects);
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "got IP " IPSTR, IP2STR(&e->ip_info.ip));
        s_connected = true;
        s_disconnects = 0;
        s_retry_paused = false;
        xEventGroupSetBits(s_eg, BIT_CONNECTED);
        stop_fallback_ap_if_needed();
    }
}

void wifi_sta_start(const char *default_ssid, const char *default_pass)
{
    s_eg = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_wifi, NULL, NULL));

    load_power_settings();
    if (!load_saved_credentials(s_ssid, sizeof(s_ssid), s_pass, sizeof(s_pass))) {
        strncpy(s_ssid, default_ssid ? default_ssid : "", sizeof(s_ssid) - 1);
        strncpy(s_pass, default_pass ? default_pass : "", sizeof(s_pass) - 1);
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    apply_sta_config();
    ESP_ERROR_CHECK(start_wifi_if_needed());
    apply_power_save();

    // Webserver/configuration window after boot. In ON_DEMAND mode WiFi will
    // stop after this window expires, unless AP fallback is active.
    wifi_sta_request_active_window(s_active_window_min);

    ESP_LOGI(TAG, "started, STA SSID=%s", s_ssid);

    xTaskCreate(fallback_task, "wifi_fallback", 3072, NULL, 4, NULL);
    xTaskCreate(power_task, "wifi_power", 3072, NULL, 3, NULL);
    xTaskCreate(retry_backoff_task, "wifi_retry", 3072, NULL, 3, NULL);
}

bool wifi_sta_wait_ip(uint32_t timeout_ms)
{
    TickType_t to = timeout_ms ? pdMS_TO_TICKS(timeout_ms) : portMAX_DELAY;
    EventBits_t b = xEventGroupWaitBits(s_eg, BIT_CONNECTED, pdFALSE, pdTRUE, to);
    return (b & BIT_CONNECTED) != 0;
}

bool wifi_sta_is_connected(void) { return s_connected; }
bool wifi_sta_is_started(void) { return s_wifi_started; }

int8_t wifi_sta_rssi(void)
{
    wifi_ap_record_t ap;
    if (s_wifi_started && esp_wifi_sta_get_ap_info(&ap) == ESP_OK) return ap.rssi;
    return 0;
}

esp_err_t wifi_sta_request_active_window(uint32_t minutes)
{
    uint32_t m = clamp_window(minutes ? minutes : s_active_window_min);
    int64_t until = esp_timer_get_time() + (int64_t)m * 60LL * 1000000LL;
    if (until > s_active_until_us) s_active_until_us = until;
    if (s_power_mode == WIFI_STA_PWR_ON_DEMAND) {
        esp_err_t err = start_wifi_if_needed();
        if (err != ESP_OK) return err;
        if (!s_connected) esp_wifi_connect();
    }
    return ESP_OK;
}

bool wifi_sta_ap_is_active(void) { return s_ap_active; }
const char *wifi_sta_ap_ssid(void) { return FALLBACK_AP_SSID; }
const char *wifi_sta_current_ssid(void) { return s_ssid; }

esp_err_t wifi_sta_save_credentials_and_reconnect(const char *ssid, const char *pass)
{
    if (!ssid || ssid[0] == 0 || strlen(ssid) > 32 || (pass && strlen(pass) > 64))
        return ESP_ERR_INVALID_ARG;

    strncpy(s_ssid, ssid, sizeof(s_ssid) - 1);
    s_ssid[sizeof(s_ssid) - 1] = 0;
    strncpy(s_pass, pass ? pass : "", sizeof(s_pass) - 1);
    s_pass[sizeof(s_pass) - 1] = 0;

    esp_err_t err = save_credentials(s_ssid, s_pass);
    if (err != ESP_OK) return err;

    wifi_sta_request_active_window(s_active_window_min);
    start_wifi_if_needed();
    s_connected = false;
    s_disconnects = 0;
    s_retry_paused = false;
    xEventGroupClearBits(s_eg, BIT_CONNECTED);

    ESP_ERROR_CHECK(esp_wifi_set_mode(s_ap_active ? WIFI_MODE_APSTA : WIFI_MODE_STA));
    apply_sta_config();
    apply_power_save();
    esp_err_t derr = esp_wifi_disconnect();
    if (derr == ESP_ERR_WIFI_NOT_CONNECT) esp_wifi_connect();
    // If disconnect was OK, the STA_DISCONNECTED event handler reconnects using
    // the new config. Return OK after NVS/config update, not after DHCP.
    ESP_LOGI(TAG, "saved WiFi credentials and reconnecting to SSID=%s", s_ssid);
    return ESP_OK;
}

esp_err_t wifi_sta_set_power_mode(wifi_sta_power_mode_t mode, bool save)
{
    if (mode > WIFI_STA_PWR_ON_DEMAND) return ESP_ERR_INVALID_ARG;
    s_power_mode = mode;

    esp_err_t err = start_wifi_if_needed();
    if (err != ESP_OK) return err;
    apply_sta_config();
    apply_power_save();
    if (!s_connected) esp_wifi_connect();
    wifi_sta_request_active_window(s_active_window_min);

    err = save ? save_power_settings() : ESP_OK;
    return err;
}

wifi_sta_power_mode_t wifi_sta_get_power_mode(void) { return s_power_mode; }
const char *wifi_sta_power_mode_name(void) { return power_mode_name(s_power_mode); }

esp_err_t wifi_sta_set_active_window_minutes(uint32_t minutes, bool save)
{
    s_active_window_min = clamp_window(minutes);
    wifi_sta_request_active_window(s_active_window_min);
    return save ? save_power_settings() : ESP_OK;
}

uint32_t wifi_sta_get_active_window_minutes(void) { return s_active_window_min; }

void wifi_sta_set_critical_activity(bool active)
{
    s_critical_activity = active;
    if (active) wifi_sta_request_active_window(15);
}

bool wifi_sta_is_critical_activity(void)
{
    return s_critical_activity;
}

esp_err_t wifi_sta_scan_json(char *out, size_t out_len)
{
    if (!out || out_len < 4) return ESP_ERR_INVALID_ARG;
    out[0] = '[';
    out[1] = ']';
    out[2] = 0;

    wifi_sta_request_active_window(s_active_window_min);

    wifi_scan_config_t sc = { 0 };
    esp_err_t err = esp_wifi_scan_start(&sc, true);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "scan failed: %s", esp_err_to_name(err));
        return err;
    }

    uint16_t count = WIFI_SCAN_MAX_AP;
    wifi_ap_record_t rec[WIFI_SCAN_MAX_AP];
    memset(rec, 0, sizeof(rec));
    err = esp_wifi_scan_get_ap_records(&count, rec);
    if (err != ESP_OK) return err;

    size_t n = 0;
    out[n++] = '[';
    for (int i = 0; i < count && n + 64 < out_len; i++) {
        n += snprintf(out + n, out_len - n, "%s{\"ssid\":\"", i ? "," : "");
        json_append_escaped(out, out_len, &n, (const char *)rec[i].ssid);
        n += snprintf(out + n, out_len - n, "\",\"rssi\":%d,\"auth\":%d}",
                      rec[i].rssi, rec[i].authmode);
    }
    if (n + 2 >= out_len) n = out_len - 2;
    out[n++] = ']';
    out[n] = 0;
    return ESP_OK;
}
