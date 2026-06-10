#include "webserver.h"
#include "timekeeper.h"
#include "display.h"
#include "hv.h"
#include "wifi_sta.h"
#include "ntp_sync.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "esp_app_desc.h"
#include "nvs.h"

static const char *TAG = "web";

static const char INDEX_HTML[] =
#include "index_html.inc"
;

#define OK_JSON "{\"ok\":true}"
#define CFG_NVS_NS        "web_cfg"
#define CFG_KEY_MOTION    "motion"
#define CFG_KEY_AP_ENABLE "ap_enable"
#define OTA_NVS_NS        "ota_cfg"
#define OTA_KEY_TOKEN     "token"
#define OTA_DEFAULT_TOKEN "nixie-ota"

static bool s_motion_enabled = false;
static char s_ota_token[65] = OTA_DEFAULT_TOKEN;

// ---------- minimal JSON helpers (no external dependency) ----------
// Request bodies are small and produced by our own page, so a strict flat
// key:value scanner is sufficient. Locate the value following "key":
static const char *jfind(const char *body, const char *key)
{
    char pat[40];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = strstr(body, pat);
    if (!p) return NULL;
    p += strlen(pat);
    while (*p == ' ' || *p == '\t') p++;
    if (*p != ':') return NULL;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

static bool jget_int(const char *body, const char *key, int *out)
{
    const char *p = jfind(body, key);
    if (!p) return false;
    char *end;
    long v = strtol(p, &end, 10);
    if (end == p) return false;
    *out = (int)v;
    return true;
}

static bool jget_bool(const char *body, const char *key, bool dflt)
{
    const char *p = jfind(body, key);
    if (!p) return dflt;
    if (!strncmp(p, "true", 4) || *p == '1') return true;
    if (!strncmp(p, "false", 5) || *p == '0') return false;
    return dflt;
}

static bool jget_str(const char *body, const char *key, char *out, size_t len)
{
    const char *p = jfind(body, key);
    if (!p || *p != '"') return false;
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < len) {
        if (*p == '\\' && p[1]) p++;   // minimal unescape for web form values
        out[i++] = *p++;
    }
    out[i] = 0;
    return true;
}

static int read_body(httpd_req_t *req, char *buf, size_t buflen)
{
    size_t total = req->content_len;
    if (total >= buflen) return -1;
    size_t off = 0;
    while (off < total) {
        int r = httpd_req_recv(req, buf + off, total - off);
        if (r <= 0) return -1;
        off += (size_t)r;
    }
    buf[off] = 0;
    return (int)off;
}

static void touch_wifi(void)
{
    wifi_sta_request_active_window(wifi_sta_get_active_window_minutes());
}

static esp_err_t send_json(httpd_req_t *req, const char *s)
{
    touch_wifi();
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, s);
}

static void json_escape(const char *in, char *out, size_t out_len)
{
    size_t n = 0;
    if (!out_len) return;
    for (; in && *in && n + 1 < out_len; in++) {
        unsigned char c = (unsigned char)*in;
        if ((c == '"' || c == '\\') && n + 2 < out_len) {
            out[n++] = '\\';
            out[n++] = (char)c;
        } else if (c >= 0x20) {
            out[n++] = (char)c;
        }
    }
    out[n] = 0;
}

static void save_bool_setting(const char *key, bool value)
{
    nvs_handle_t h;
    if (nvs_open(CFG_NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u8(h, key, value ? 1 : 0);
    nvs_commit(h);
    nvs_close(h);
}

static void load_ota_token(void)
{
    nvs_handle_t h;
    if (nvs_open(OTA_NVS_NS, NVS_READONLY, &h) != ESP_OK) return;
    size_t len = sizeof(s_ota_token);
    if (nvs_get_str(h, OTA_KEY_TOKEN, s_ota_token, &len) != ESP_OK || s_ota_token[0] == 0)
        strncpy(s_ota_token, OTA_DEFAULT_TOKEN, sizeof(s_ota_token) - 1);
    nvs_close(h);
}

static esp_err_t save_ota_token(const char *token)
{
    if (!token || strlen(token) < 4 || strlen(token) >= sizeof(s_ota_token))
        return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err = nvs_open(OTA_NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_str(h, OTA_KEY_TOKEN, token);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err == ESP_OK) {
        strncpy(s_ota_token, token, sizeof(s_ota_token) - 1);
        s_ota_token[sizeof(s_ota_token) - 1] = 0;
    }
    return err;
}

static void load_web_settings(void)
{
    nvs_handle_t h;
    if (nvs_open(CFG_NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        uint8_t v = 0;
        if (nvs_get_u8(h, CFG_KEY_MOTION, &v) == ESP_OK) s_motion_enabled = (v != 0);
        if (nvs_get_u8(h, CFG_KEY_AP_ENABLE, &v) == ESP_OK) timekeeper_set_antipoison(v != 0);
        nvs_close(h);
    }
    load_ota_token();
}

static wifi_sta_power_mode_t parse_wifi_power_mode(const char *mode)
{
    if (!strcmp(mode, "performance")) return WIFI_STA_PWR_PERFORMANCE;
    if (!strcmp(mode, "balanced"))    return WIFI_STA_PWR_BALANCED;
    if (!strcmp(mode, "max_save"))    return WIFI_STA_PWR_MAX_SAVE;
    if (!strcmp(mode, "on_demand"))   return WIFI_STA_PWR_ON_DEMAND;
    return WIFI_STA_PWR_BALANCED;
}

static bool token_equal(const char *a, const char *b)
{
    size_t la = strlen(a ? a : "");
    size_t lb = strlen(b ? b : "");
    unsigned char diff = (unsigned char)(la ^ lb);
    size_t n = la > lb ? la : lb;
    for (size_t i = 0; i < n; i++) {
        unsigned char ca = i < la ? (unsigned char)a[i] : 0;
        unsigned char cb = i < lb ? (unsigned char)b[i] : 0;
        diff |= ca ^ cb;
    }
    return diff == 0;
}

static bool token_from_headers(httpd_req_t *req, char *out, size_t out_len)
{
    if (!out || !out_len) return false;
    out[0] = 0;
    if (httpd_req_get_hdr_value_str(req, "X-Admin-Token", out, out_len) == ESP_OK && out[0])
        return true;
    if (httpd_req_get_hdr_value_str(req, "X-OTA-Token", out, out_len) == ESP_OK && out[0])
        return true;
    return false;
}

static bool request_is_authorized(httpd_req_t *req)
{
    char token[65];
    return token_from_headers(req, token, sizeof(token)) && token_equal(token, s_ota_token);
}

static esp_err_t require_admin(httpd_req_t *req)
{
    if (request_is_authorized(req)) return ESP_OK;
    httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "bad admin token");
    return ESP_FAIL;
}

// ---------- handlers ----------
static esp_err_t h_index(httpd_req_t *req)
{
    touch_wifi();
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_sendstr(req, INDEX_HTML);
}

static esp_err_t h_status(httpd_req_t *req)
{
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    char ts[16];
    strftime(ts, sizeof(ts), "%H:%M:%S", &tm);

    char ssid[80];
    char ap_ssid[80];
    char fw_ver[48];
    json_escape(wifi_sta_current_ssid(), ssid, sizeof(ssid));
    json_escape(wifi_sta_ap_ssid(), ap_ssid, sizeof(ap_ssid));
    const esp_app_desc_t *app = esp_app_get_description();
    json_escape(app ? app->version : "unknown", fw_ver, sizeof(fw_ver));
    const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);

    char resp[1200];
    snprintf(resp, sizeof(resp),
             "{\"time\":\"%s\",\"time_valid\":%s,\"mode\":\"%s\","
             "\"hv\":%s,\"wifi\":%s,\"wifi_started\":%s,\"rssi\":%d,\"ssid\":\"%s\","
             "\"ap\":%s,\"ap_ssid\":\"%s\",\"antipoison\":%s,"
             "\"motion\":%s,\"ntp_interval_min\":%lu,"
             "\"wifi_power\":\"%s\",\"wifi_window_min\":%lu,"
             "\"ota_ready\":%s,\"fw_version\":\"%s\",\"hv_driver\":\"%s\"}",
             ts,
             timekeeper_time_valid() ? "true" : "false",
             timekeeper_get_mode() == CLK_MODE_NTP ? "ntp" : "manual",
             hv_is_on() ? "true" : "false",
             wifi_sta_is_connected() ? "true" : "false",
             wifi_sta_is_started() ? "true" : "false",
             wifi_sta_rssi(), ssid,
             wifi_sta_ap_is_active() ? "true" : "false", ap_ssid,
             timekeeper_get_antipoison() ? "true" : "false",
             s_motion_enabled ? "true" : "false",
             (unsigned long)ntp_sync_get_interval_minutes(),
             wifi_sta_power_mode_name(),
             (unsigned long)wifi_sta_get_active_window_minutes(),
             next ? "true" : "false",
             fw_ver, hv_driver_mode_name());
    return send_json(req, resp);
}

static esp_err_t h_hv(httpd_req_t *req)
{
    if (require_admin(req) != ESP_OK) return ESP_FAIL;
    char body[128];
    if (read_body(req, body, sizeof(body)) < 0)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad body");

    if (jget_bool(body, "on", false)) {
        // HV may be enabled only after a successful full display write.
        esp_err_t r = display_blank_all();
        if (r != ESP_OK || !display_is_healthy())
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "display/I2C not healthy");
        r = hv_on();
        if (r != ESP_OK)
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "HV cooldown or invalid state");
        timekeeper_notify();          // immediate re-render after enabling HV
    } else {
        display_blank_all();          // no cathode drive while HV is disabled
        hv_off();
    }
    return send_json(req, OK_JSON);
}

static esp_err_t h_mode(httpd_req_t *req)
{
    if (require_admin(req) != ESP_OK) return ESP_FAIL;
    char body[128];
    if (read_body(req, body, sizeof(body)) < 0)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad body");
    char mode[16];
    if (jget_str(body, "mode", mode, sizeof(mode))) {
        if (!strcmp(mode, "ntp"))          timekeeper_set_mode(CLK_MODE_NTP);
        else if (!strcmp(mode, "manual")) timekeeper_set_mode(CLK_MODE_MANUAL);
    }
    return send_json(req, OK_JSON);
}

static esp_err_t h_time(httpd_req_t *req)
{
    if (require_admin(req) != ESP_OK) return ESP_FAIL;
    char body[128];
    if (read_body(req, body, sizeof(body)) < 0)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad body");
    int hh, mm;
    if (!jget_int(body, "hh", &hh) || !jget_int(body, "mm", &mm) ||
        hh < 0 || hh > 23 || mm < 0 || mm > 59)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid time");
    timekeeper_set_manual_time(hh, mm);
    return send_json(req, OK_JSON);
}

static esp_err_t h_sync(httpd_req_t *req)
{
    if (require_admin(req) != ESP_OK) return ESP_FAIL;
    ntp_sync_now();
    return send_json(req, OK_JSON);
}

static esp_err_t h_ntp(httpd_req_t *req)
{
    if (require_admin(req) != ESP_OK) return ESP_FAIL;
    char body[128];
    if (read_body(req, body, sizeof(body)) < 0)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad body");
    int interval = 0;
    esp_err_t r = ESP_OK;
    if (jget_int(body, "interval_min", &interval))
        r = ntp_sync_set_interval_minutes((uint32_t)interval, true);
    return send_json(req, r == ESP_OK ? OK_JSON : "{\"ok\":false}");
}

static esp_err_t h_antipoison(httpd_req_t *req)
{
    if (require_admin(req) != ESP_OK) return ESP_FAIL;
    char body[128];
    if (read_body(req, body, sizeof(body)) < 0)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad body");

    if (jfind(body, "enabled")) {
        bool en = jget_bool(body, "enabled", true);
        timekeeper_set_antipoison(en);
        save_bool_setting(CFG_KEY_AP_ENABLE, en);
    }
    if (jget_bool(body, "run", false)) {
        esp_err_t r = timekeeper_run_antipoison();
        if (r != ESP_OK) return send_json(req, "{\"ok\":false,\"reason\":\"busy_or_cooldown\"}");
    }
    return send_json(req, OK_JSON);
}

static esp_err_t h_motion(httpd_req_t *req)
{
    if (require_admin(req) != ESP_OK) return ESP_FAIL;
    char body[128];
    if (read_body(req, body, sizeof(body)) < 0)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad body");
    s_motion_enabled = jget_bool(body, "enabled", s_motion_enabled);
    save_bool_setting(CFG_KEY_MOTION, s_motion_enabled);
    return send_json(req, OK_JSON);
}

static esp_err_t h_wifi(httpd_req_t *req)
{
    if (require_admin(req) != ESP_OK) return ESP_FAIL;
    char body[256];
    if (read_body(req, body, sizeof(body)) < 0)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad body");
    char ssid[33] = {0};
    char pass[65] = {0};
    esp_err_t r = ESP_ERR_INVALID_ARG;
    if (jget_str(body, "ssid", ssid, sizeof(ssid))) {
        jget_str(body, "pass", pass, sizeof(pass));
        r = wifi_sta_save_credentials_and_reconnect(ssid, pass);
    }
    return send_json(req, r == ESP_OK ? OK_JSON : "{\"ok\":false}");
}

static esp_err_t h_wifi_scan(httpd_req_t *req)
{
    char resp[2048];
    if (wifi_sta_scan_json(resp, sizeof(resp)) != ESP_OK) strcpy(resp, "[]");
    return send_json(req, resp);
}

static esp_err_t h_wifi_power(httpd_req_t *req)
{
    if (require_admin(req) != ESP_OK) return ESP_FAIL;
    char body[160];
    if (read_body(req, body, sizeof(body)) < 0)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad body");
    char mode[24];
    esp_err_t r = ESP_OK;
    if (jget_str(body, "mode", mode, sizeof(mode)))
        r = wifi_sta_set_power_mode(parse_wifi_power_mode(mode), true);
    int window = 0;
    if (jget_int(body, "window_min", &window) && r == ESP_OK)
        r = wifi_sta_set_active_window_minutes((uint32_t)window, true);
    return send_json(req, r == ESP_OK ? OK_JSON : "{\"ok\":false}");
}


static hv_driver_mode_t parse_hv_driver_mode(const char *mode)
{
    if (!strcmp(mode, "open_drain_pullup") || !strcmp(mode, "open_drain"))
        return HV_DRIVER_OPEN_DRAIN_PULLUP;
    if (!strcmp(mode, "mmbta_direct_base") || !strcmp(mode, "mmbta"))
        return HV_DRIVER_MMBTA_DIRECT_BASE;
    return (hv_driver_mode_t)-1;
}

static esp_err_t h_hv_driver(httpd_req_t *req)
{
    if (require_admin(req) != ESP_OK) return ESP_FAIL;
    char body[128];
    if (read_body(req, body, sizeof(body)) < 0)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad body");
    char mode[32];
    if (!jget_str(body, "mode", mode, sizeof(mode)))
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing mode");

    hv_driver_mode_t m = parse_hv_driver_mode(mode);
    if (m != HV_DRIVER_OPEN_DRAIN_PULLUP && m != HV_DRIVER_MMBTA_DIRECT_BASE)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad mode");

    // Mode change always forces HV OFF inside hv_set_driver_mode().
    esp_err_t r = hv_set_driver_mode(m, true);
    if (r != ESP_OK)
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "driver mode save/drive failed");
    display_blank_all();
    return send_json(req, OK_JSON);
}

static esp_err_t h_calib(httpd_req_t *req)
{
    if (require_admin(req) != ESP_OK) return ESP_FAIL;
    char body[96];
    if (read_body(req, body, sizeof(body)) < 0)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad body");
    int idx;
    if (!jget_int(body, "index", &idx)) return send_json(req, "{\"ok\":false}");
    char desc[96];
    display_describe_bit(idx, desc, sizeof(desc));
    esp_err_t r = display_calib(idx);
    char resp[160];
    if (r == ESP_OK)
        snprintf(resp, sizeof(resp), "{\"ok\":true,\"bit\":\"%s\"}", desc);
    else
        snprintf(resp, sizeof(resp), "{\"ok\":false,\"bit\":\"%s\"}", desc);
    return send_json(req, resp);
}

static esp_err_t h_ota_token(httpd_req_t *req)
{
    if (require_admin(req) != ESP_OK) return ESP_FAIL;
    char body[128];
    if (read_body(req, body, sizeof(body)) < 0)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad body");
    char token[65] = {0};
    esp_err_t r = ESP_ERR_INVALID_ARG;
    if (jget_str(body, "new_token", token, sizeof(token)) ||
        jget_str(body, "token", token, sizeof(token))) {
        r = save_ota_token(token);
    }
    return send_json(req, r == ESP_OK ? OK_JSON : "{\"ok\":false}");
}

static bool ota_token_from_request(httpd_req_t *req, char *out, size_t out_len)
{
    return token_from_headers(req, out, out_len);
}

static void restart_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(500));
    display_blank_all();
    hv_off();
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
}

static esp_err_t h_ota(httpd_req_t *req)
{
    char token[65];
    if (!ota_token_from_request(req, token, sizeof(token)) || !token_equal(token, s_ota_token)) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "bad OTA token");
    }

    if (req->content_len == 0) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "empty firmware");
    }

    const esp_partition_t *part = esp_ota_get_next_update_partition(NULL);
    if (!part) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no OTA partition");
    if ((uint32_t)req->content_len > part->size) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "firmware too large");
    }

    wifi_sta_set_critical_activity(true);
    ESP_LOGI(TAG, "OTA upload: %d bytes -> %s @ 0x%lx",
             req->content_len, part->label, (unsigned long)part->address);

    esp_ota_handle_t ota = 0;
    esp_err_t err = esp_ota_begin(part, OTA_SIZE_UNKNOWN, &ota);
    if (err != ESP_OK) {
        wifi_sta_set_critical_activity(false);
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "ota begin failed");
    }

    char buf[2048];
    int remaining = req->content_len;
    while (remaining > 0) {
        int to_read = remaining < (int)sizeof(buf) ? remaining : (int)sizeof(buf);
        int r = httpd_req_recv(req, buf, to_read);
        if (r == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (r <= 0) {
            ESP_LOGE(TAG, "OTA recv failed");
            esp_ota_abort(ota);
            wifi_sta_set_critical_activity(false);
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "recv failed");
        }
        err = esp_ota_write(ota, buf, r);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
            esp_ota_abort(ota);
            wifi_sta_set_critical_activity(false);
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "write failed");
        }
        remaining -= r;
    }

    err = esp_ota_end(ota);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        wifi_sta_set_critical_activity(false);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid firmware image");
    }

    err = esp_ota_set_boot_partition(part);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        wifi_sta_set_critical_activity(false);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "boot partition failed");
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true,\"rebooting\":true}");
    xTaskCreate(restart_task, "ota_reboot", 2048, NULL, 5, NULL);
    return ESP_OK;
}

static httpd_uri_t routes[] = {
    {.uri = "/",               .method = HTTP_GET,  .handler = h_index},
    {.uri = "/api/status",     .method = HTTP_GET,  .handler = h_status},
    {.uri = "/api/wifi_scan",  .method = HTTP_GET,  .handler = h_wifi_scan},
    {.uri = "/api/hv",         .method = HTTP_POST, .handler = h_hv},
    {.uri = "/api/hv_driver",  .method = HTTP_POST, .handler = h_hv_driver},
    {.uri = "/api/mode",       .method = HTTP_POST, .handler = h_mode},
    {.uri = "/api/time",       .method = HTTP_POST, .handler = h_time},
    {.uri = "/api/sync",       .method = HTTP_POST, .handler = h_sync},
    {.uri = "/api/ntp",        .method = HTTP_POST, .handler = h_ntp},
    {.uri = "/api/antipoison", .method = HTTP_POST, .handler = h_antipoison},
    {.uri = "/api/motion",     .method = HTTP_POST, .handler = h_motion},
    {.uri = "/api/wifi",       .method = HTTP_POST, .handler = h_wifi},
    {.uri = "/api/wifi_power", .method = HTTP_POST, .handler = h_wifi_power},
    {.uri = "/api/calib",      .method = HTTP_POST, .handler = h_calib},
    {.uri = "/api/ota_token",  .method = HTTP_POST, .handler = h_ota_token},
    {.uri = "/api/ota",        .method = HTTP_POST, .handler = h_ota},
};

esp_err_t webserver_start(void)
{
    load_web_settings();

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_uri_handlers = 22;
    cfg.stack_size       = 8192;
    cfg.lru_purge_enable = true;
    httpd_handle_t srv = NULL;
    esp_err_t err = httpd_start(&srv, &cfg);
    if (err != ESP_OK) { ESP_LOGE(TAG, "httpd_start failed"); return err; }
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++)
        httpd_register_uri_handler(srv, &routes[i]);
    ESP_LOGI(TAG, "web server up");
    return ESP_OK;
}
