#include "ntp_sync.h"
#include "timekeeper.h"
#include "wifi_sta.h"
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "nvs.h"

static const char *TAG = "ntp";
static TaskHandle_t s_task;
static volatile uint32_t s_interval_min = 720;  // default: 12 h
static volatile bool s_sync_requested = false;

#define NTP_NVS_NS       "ntp_cfg"
#define NTP_NVS_INTERVAL "interval_min"
#define NTP_MIN_MINUTES  5u
#define NTP_MAX_MINUTES  1440u

static uint32_t clamp_interval(uint32_t minutes)
{
    if (minutes < NTP_MIN_MINUTES) return NTP_MIN_MINUTES;
    if (minutes > NTP_MAX_MINUTES) return NTP_MAX_MINUTES;
    return minutes;
}

static void load_interval_nvs(void)
{
    nvs_handle_t h;
    if (nvs_open(NTP_NVS_NS, NVS_READONLY, &h) != ESP_OK) return;
    uint32_t v = 0;
    if (nvs_get_u32(h, NTP_NVS_INTERVAL, &v) == ESP_OK) s_interval_min = clamp_interval(v);
    nvs_close(h);
}

static esp_err_t save_interval_nvs(uint32_t minutes)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NTP_NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_u32(h, NTP_NVS_INTERVAL, minutes);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

static void apply_sntp_poll_interval(void)
{
    uint64_t ms = (uint64_t)s_interval_min * 60ULL * 1000ULL;
    if (ms > UINT32_MAX) ms = UINT32_MAX;
    sntp_set_sync_interval((uint32_t)ms);
}

static void on_sync(struct timeval *tv)
{
    ESP_LOGI(TAG, "SNTP sync ok (epoch=%lld)", (long long)tv->tv_sec);
    timekeeper_on_ntp_synced();
}

static void request_task_wakeup(void)
{
    if (s_task) xTaskNotifyGive(s_task);
}

static void do_sntp_restart_when_wifi_ready(void)
{
    // In WiFi ON_DEMAND mode this powers STA up for the NTP transaction.
    wifi_sta_request_active_window(2);
    if (!wifi_sta_is_connected()) {
        if (!wifi_sta_wait_ip(30000)) {
            ESP_LOGW(TAG, "NTP skipped - WiFi did not get IP");
            return;
        }
    }
    esp_sntp_restart();
    // Keep webserver reachable shortly after a manual/scheduled sync.
    wifi_sta_request_active_window(1);
}

static void ntp_task(void *arg)
{
    do_sntp_restart_when_wifi_ready();   // first sync after boot when WiFi is up
    s_sync_requested = false;

    while (1) {
        if (!timekeeper_time_valid()) {
            // Not synced yet: retry every 30 s or immediately after Sync now.
            uint32_t notified = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(30000));
            if (notified == 0 || s_sync_requested) {
                s_sync_requested = false;
                do_sntp_restart_when_wifi_ready();
            }
        } else {
            uint32_t wait_s = ntp_sync_get_interval_minutes() * 60U;
            ESP_LOGI(TAG, "next scheduled sync in %lu s (%lu min)",
                     (unsigned long)wait_s, (unsigned long)ntp_sync_get_interval_minutes());
            TickType_t ticks = (TickType_t)((uint64_t)wait_s * configTICK_RATE_HZ);
            uint32_t notified = ulTaskNotifyTake(pdTRUE, ticks);

            if (notified == 0 || s_sync_requested) {
                s_sync_requested = false;
                do_sntp_restart_when_wifi_ready();
            }
            // If we were notified only because the interval changed, loop and recalc.
        }
    }
}

void ntp_sync_start(const char *primary, const char *backup)
{
    load_interval_nvs();

    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, primary);
    esp_sntp_setservername(1, backup);
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    sntp_set_time_sync_notification_cb(on_sync);
    apply_sntp_poll_interval();
    esp_sntp_init();
    xTaskCreate(ntp_task, "ntp", 4096, NULL, 5, &s_task);
    ESP_LOGI(TAG, "SNTP started: %s , %s ; interval=%lu min",
             primary, backup, (unsigned long)s_interval_min);
}

void ntp_sync_now(void)
{
    s_sync_requested = true;
    request_task_wakeup();
}

uint32_t ntp_sync_get_interval_minutes(void)
{
    return s_interval_min;
}

esp_err_t ntp_sync_set_interval_minutes(uint32_t minutes, bool save)
{
    uint32_t v = clamp_interval(minutes);
    s_interval_min = v;
    apply_sntp_poll_interval();
    if (save) {
        esp_err_t err = save_interval_nvs(v);
        if (err != ESP_OK) return err;
    }
    ESP_LOGI(TAG, "SNTP interval set to %lu min", (unsigned long)v);
    request_task_wakeup();
    return ESP_OK;
}
