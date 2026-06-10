#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/i2c_master.h"
#include "esp_pm.h"
#include "esp_ota_ops.h"

#include "app_config.h"
#if __has_include("secrets.h")
#include "secrets.h"
#else
#define WIFI_SSID ""
#define WIFI_PASS ""
#endif
#include "tca9535.h"
#include "display.h"
#include "hv.h"
#include "timekeeper.h"
#include "wifi_sta.h"
#include "ntp_sync.h"
#include "webserver.h"

static const char *TAG = "main";
static i2c_master_bus_handle_t s_bus;

static void display_fault_cb(esp_err_t err)
{
    ESP_LOGE(TAG, "display fault detected (%s) - HV forced OFF", esp_err_to_name(err));
    hv_off();
}

static void ota_rollback_check_start(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    if (running && esp_ota_get_state_partition(running, &state) == ESP_OK &&
        state == ESP_OTA_IMG_PENDING_VERIFY) {
        ESP_LOGW(TAG, "OTA image is pending verification; will mark valid after init self-test");
    }
}

static void ota_mark_valid_after_selftest(bool ok)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    if (!running || esp_ota_get_state_partition(running, &state) != ESP_OK ||
        state != ESP_OTA_IMG_PENDING_VERIFY) {
        return;
    }

    if (ok) {
        esp_err_t r = esp_ota_mark_app_valid_cancel_rollback();
        ESP_LOGI(TAG, "OTA app validation: %s", esp_err_to_name(r));
    } else {
        ESP_LOGE(TAG, "OTA app not marked valid because init self-test failed");
    }
}

static void nvs_init(void)
{
    esp_err_t r = nvs_flash_init();
    if (r == ESP_ERR_NVS_NO_FREE_PAGES || r == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        r = nvs_flash_init();
    }
    ESP_ERROR_CHECK(r);
}

static void i2c_bus_init(void)
{
    i2c_master_bus_config_t cfg = {
        .clk_source                 = I2C_CLK_SRC_DEFAULT,
        .i2c_port                   = I2C_PORT_NUM,
        .sda_io_num                 = PIN_I2C_SDA,
        .scl_io_num                 = PIN_I2C_SCL,
        .glitch_ignore_cnt          = 7,
        .flags.enable_internal_pullup = true,   // board also has 4.7k pull-ups
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&cfg, &s_bus));

    ESP_LOGI(TAG, "I2C scan:");
    for (uint8_t a = 1; a < 0x7F; a++) {
        if (i2c_master_probe(s_bus, a, 20) == ESP_OK)
            ESP_LOGI(TAG, "  found device at 0x%02X", a);
    }
}

static void power_mgmt_init(void)
{
#if CONFIG_PM_ENABLE
    esp_pm_config_t pm = {
        .max_freq_mhz       = 160,
        .min_freq_mhz       = 80,
        .light_sleep_enable = true,
    };
    esp_err_t err = esp_pm_configure(&pm);
    if (err == ESP_OK)
        ESP_LOGI(TAG, "automatic light sleep enabled");
    else
        ESP_LOGW(TAG, "esp_pm_configure failed: %s (check tickless idle config)",
                 esp_err_to_name(err));
#endif
}

void app_main(void)
{
    nvs_init();
    ota_rollback_check_start();

    setenv("TZ", TZ_POLAND, 1);
    tzset();

    hv_init(PIN_HV_EN);          // starts OFF (safe)

    i2c_bus_init();

    tca9535_handle_t u500, u501;
    ESP_ERROR_CHECK(tca9535_create(s_bus, TCA_ADDR_U500, I2C_FREQ_HZ, &u500));
    ESP_ERROR_CHECK(tca9535_create(s_bus, TCA_ADDR_U501, I2C_FREQ_HZ, &u501));

    esp_err_t e1 = tca9535_config_all_outputs(u500);
    esp_err_t e2 = tca9535_config_all_outputs(u501);
    if (e1 != ESP_OK || e2 != ESP_OK)
        ESP_LOGW(TAG, "expander config failed (u500=%s, u501=%s) - check wiring/addresses",
                 esp_err_to_name(e1), esp_err_to_name(e2));

    display_set_error_callback(display_fault_cb);
    esp_err_t ed = display_init(u500, u501);     // all tubes blanked with full I2C write
    bool display_ok = (e1 == ESP_OK && e2 == ESP_OK && ed == ESP_OK && display_is_healthy());
    if (!display_ok) {
        ESP_LOGW(TAG, "display self-test failed (u500=%s, u501=%s, init=%s) - HV locked OFF, web diagnostics still available",
                 esp_err_to_name(e1), esp_err_to_name(e2), esp_err_to_name(ed));
        hv_off();
    } else {
        ESP_LOGI(TAG, "display self-test OK; using compiled fixed LUT");
    }

#if HV_ON_AT_BOOT
    if (display_ok) {
        if (hv_on() != ESP_OK) ESP_LOGW(TAG, "HV ON at boot blocked by cooldown/state");
    }
#endif

    power_mgmt_init();

    timekeeper_start();                            // blinks "1" until time is valid
    timekeeper_set_antipoison(ANTIPOISON_ENABLE);

    wifi_sta_start(WIFI_SSID, WIFI_PASS);
    ntp_sync_start(NTP_SERVER_PRIMARY, NTP_SERVER_BACKUP);

    ESP_ERROR_CHECK(webserver_start());

    ota_mark_valid_after_selftest(display_ok);
    ESP_LOGI(TAG, "init complete");
}
