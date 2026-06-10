#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

typedef enum {
    WIFI_STA_PWR_PERFORMANCE = 0,  // WiFi PS off: best web responsiveness
    WIFI_STA_PWR_BALANCED    = 1,  // WIFI_PS_MIN_MODEM
    WIFI_STA_PWR_MAX_SAVE    = 2,  // WIFI_PS_MAX_MODEM
    WIFI_STA_PWR_ON_DEMAND   = 3,  // WiFi is stopped after an active window
} wifi_sta_power_mode_t;

// Start WiFi station and auto-reconnect. Credentials passed from secrets.h are
// used only as defaults. If credentials were saved from the web UI, NVS wins.
void    wifi_sta_start(const char *default_ssid, const char *default_pass);

// Block until an IP is obtained (or timeout). portMAX_DELAY-ish via ms = 0 => wait forever.
bool    wifi_sta_wait_ip(uint32_t timeout_ms);

bool    wifi_sta_is_connected(void);
bool    wifi_sta_is_started(void);
int8_t  wifi_sta_rssi(void);   // current RSSI, 0 if unknown

// Keep WiFi/webserver reachable for at least this many minutes. In ON_DEMAND
// mode this starts WiFi if it was stopped. In other modes it is harmless.
esp_err_t wifi_sta_request_active_window(uint32_t minutes);

// Fallback setup AP. It starts automatically when STA cannot connect.
bool        wifi_sta_ap_is_active(void);
const char *wifi_sta_ap_ssid(void);
const char *wifi_sta_current_ssid(void);

// Save new STA credentials to NVS and reconnect immediately.
esp_err_t wifi_sta_save_credentials_and_reconnect(const char *ssid, const char *pass);

// Power policy, saved in NVS when save=true.
esp_err_t wifi_sta_set_power_mode(wifi_sta_power_mode_t mode, bool save);
wifi_sta_power_mode_t wifi_sta_get_power_mode(void);
const char *wifi_sta_power_mode_name(void);
esp_err_t wifi_sta_set_active_window_minutes(uint32_t minutes, bool save);
uint32_t wifi_sta_get_active_window_minutes(void);

// Prevent ON_DEMAND mode from stopping WiFi during critical operations such as OTA.
void wifi_sta_set_critical_activity(bool active);
bool wifi_sta_is_critical_activity(void);

// Blocking WiFi scan, formatted as JSON array:
// [{"ssid":"...","rssi":-55,"auth":3}, ...]
esp_err_t wifi_sta_scan_json(char *out, size_t out_len);
