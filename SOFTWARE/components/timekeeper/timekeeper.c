#include "timekeeper.h"
#include "display.h"
#include "hv.h"
#include "esp_timer.h"
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "timekeeper";

static TaskHandle_t s_task;
static volatile bool       s_valid    = false;
static volatile clk_mode_t s_mode     = CLK_MODE_NTP;
static volatile bool       s_override = false;
static volatile int        s_ov[4]    = {DISPLAY_BLANK, DISPLAY_BLANK, DISPLAY_BLANK, DISPLAY_BLANK};

// Anti-poisoning (nightly cathode cleaning): cycle every cathode of every tube.
#define AP_HOUR        3
#define AP_MIN         0
#define AP_DURATION_S  120
#define AP_STEP_MS     700
#define AP_BLANK_MS     70
static volatile bool s_ap_enabled   = true;
static volatile bool s_ap_request   = false;
static volatile bool s_ap_force_req = false;
static volatile bool s_ap_running   = false;
static volatile int64_t s_ap_last_manual_us = 0;
static int           s_ap_last_yday = -1;
#define AP_MANUAL_COOLDOWN_US (10LL * 60LL * 1000000LL)

static uint32_t ms_to_next_minute(void)
{
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    uint32_t ms = (uint32_t)(60 - tm.tm_sec) * 1000U;
    return ms ? ms : 60000U;
}

static void render_current(void)
{
    if (s_override) {
        display_set_raw(s_ov[0], s_ov[1], s_ov[2], s_ov[3]);
        return;
    }
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    display_set_time(tm.tm_hour, tm.tm_min);
}

static int ap_digit_if_present(tube_t tube, int digit)
{
    display_cell_t c = {0};
    if (display_lut_get(tube, digit, &c) == ESP_OK && c.present) return digit;
    return DISPLAY_BLANK;
}

static void antipoison_run(bool force)
{
    if (!s_ap_enabled && !force) { ESP_LOGI(TAG, "anti-poison skipped (disabled)"); return; }
    if (!hv_is_on()) { ESP_LOGW(TAG, "anti-poison skipped (HV off)"); return; }

    bool auto_enabled_at_start = s_ap_enabled;
    s_ap_running = true;
    ESP_LOGI(TAG, "anti-poisoning: start (%d s)", AP_DURATION_S);
    int64_t end = esp_timer_get_time() + (int64_t)AP_DURATION_S * 1000000;
    int d = 0;
    while (esp_timer_get_time() < end) {
        if (auto_enabled_at_start && !s_ap_enabled) {
            ESP_LOGI(TAG, "anti-poisoning: stopped because auto mode was disabled");
            break;
        }
        if (!hv_is_on()) {
            ESP_LOGW(TAG, "anti-poisoning: stopped because HV is off");
            break;
        }
        // Classic cathode anti-poisoning: cycle only the cathodes that really
        // exist in the LUT. Short blanking between steps limits ghosting and
        // avoids forcing unsupported digits on H10/M10 tubes.
        display_blank_all();
        vTaskDelay(pdMS_TO_TICKS(AP_BLANK_MS));
        display_set_raw(ap_digit_if_present(TUBE_H10, d),
                        ap_digit_if_present(TUBE_H1,  d),
                        ap_digit_if_present(TUBE_M10, d),
                        ap_digit_if_present(TUBE_M1,  d));

        // A notification (user action) cuts the routine short.
        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(AP_STEP_MS)) != 0) {
            ESP_LOGI(TAG, "anti-poisoning: interrupted");
            break;
        }
        d = (d + 1) % 10;
    }
    display_blank_all();
    s_ap_running = false;
    ESP_LOGI(TAG, "anti-poisoning: end");
}

static void timekeeper_task(void *arg)
{
    bool blink_on = false;
    while (1) {
        if (s_ap_request) {
            bool force = s_ap_force_req;
            s_ap_request = false;
            s_ap_force_req = false;
            antipoison_run(force);
            continue;
        }

        if (!s_valid) {
            // Waiting for first time: blink "1" on all tubes at 1 Hz.
            blink_on = !blink_on;
            if (blink_on) display_set_raw(1, 1, 1, 1);
            else          display_blank_all();
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
        } else {
            time_t now = time(NULL);
            struct tm tm;
            localtime_r(&now, &tm);
            if (s_ap_enabled && !s_override &&
                tm.tm_hour == AP_HOUR && tm.tm_min == AP_MIN &&
                tm.tm_yday != s_ap_last_yday) {
                s_ap_last_yday = tm.tm_yday;
                antipoison_run(false);
            }
            render_current();
            if (s_override)
                ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            else
                ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(ms_to_next_minute()));
        }
    }
}

void timekeeper_start(void)
{
    xTaskCreate(timekeeper_task, "timekeeper", 4096, NULL, 6, &s_task);
}

void timekeeper_notify(void)
{
    if (s_task) xTaskNotifyGive(s_task);
}

void timekeeper_on_ntp_synced(void)
{
    s_valid = true;
    if (s_mode == CLK_MODE_NTP) s_override = false;
    ESP_LOGI(TAG, "time valid (NTP)");
    timekeeper_notify();
}

bool timekeeper_time_valid(void) { return s_valid; }

void timekeeper_set_mode(clk_mode_t m)
{
    s_mode = m;
    s_override = false;
    timekeeper_notify();
}

clk_mode_t timekeeper_get_mode(void) { return s_mode; }

void timekeeper_set_manual_time(int hh, int mm)
{
    if (hh < 0 || hh > 23 || mm < 0 || mm > 59) {
        ESP_LOGW(TAG, "manual time rejected: %02d:%02d", hh, mm);
        return;
    }
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    tm.tm_hour = hh;
    tm.tm_min  = mm;
    tm.tm_sec  = 0;
    time_t t = mktime(&tm);              // interpreted in local TZ
    struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
    settimeofday(&tv, NULL);
    s_mode     = CLK_MODE_MANUAL;
    s_valid    = true;
    s_override = false;
    ESP_LOGI(TAG, "manual time set %02d:%02d", hh, mm);
    timekeeper_notify();
}

void timekeeper_set_override(int h10, int h1, int m10, int m1)
{
    s_ov[0] = h10; s_ov[1] = h1; s_ov[2] = m10; s_ov[3] = m1;
    s_override = true;
    s_valid    = true;   // ensure we leave the blink state
    timekeeper_notify();
}

void timekeeper_clear_override(void)
{
    s_override = false;
    timekeeper_notify();
}

void timekeeper_set_antipoison(bool en)
{
    s_ap_enabled = en;
    ESP_LOGI(TAG, "anti-poisoning %s", en ? "enabled" : "disabled");
    if (!en && s_ap_running) timekeeper_notify();
}

bool timekeeper_get_antipoison(void) { return s_ap_enabled; }

esp_err_t timekeeper_run_antipoison(void)
{
    if (s_ap_running || s_ap_request) return ESP_ERR_INVALID_STATE;
    int64_t now = esp_timer_get_time();
    if (s_ap_last_manual_us != 0 && now - s_ap_last_manual_us < AP_MANUAL_COOLDOWN_US) {
        ESP_LOGW(TAG, "anti-poisoning manual run blocked by cooldown");
        return ESP_ERR_INVALID_STATE;
    }
    s_ap_last_manual_us = now;
    s_ap_force_req = true;
    s_ap_request = true;
    timekeeper_notify();
    return ESP_OK;
}
