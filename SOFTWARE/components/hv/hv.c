#include "hv.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"
#include <stdint.h>

static const char *TAG = "hv";
static int  s_gpio = -1;
static bool s_on   = false;
static int64_t s_last_change_us = 0;
static hv_driver_mode_t s_mode = HV_DRIVER_MMBTA_DIRECT_BASE;

// Protect boost EN from rapid ON/OFF/ON spam.
#define HV_ON_COOLDOWN_US (1000LL * 1000LL)
#define HV_NVS_NS         "hv_cfg"
#define HV_NVS_KEY_DRIVER "driver"

static const char *mode_to_name(hv_driver_mode_t mode)
{
    switch (mode) {
    case HV_DRIVER_OPEN_DRAIN_PULLUP: return "open_drain_pullup";
    case HV_DRIVER_MMBTA_DIRECT_BASE:
    default: return "mmbta_direct_base";
    }
}

const char *hv_driver_mode_name(void)
{
    return mode_to_name(s_mode);
}

hv_driver_mode_t hv_get_driver_mode(void)
{
    return s_mode;
}

static bool mode_valid(hv_driver_mode_t mode)
{
    return mode == HV_DRIVER_MMBTA_DIRECT_BASE ||
           mode == HV_DRIVER_OPEN_DRAIN_PULLUP;
}

static void load_driver_mode(void)
{
    nvs_handle_t h;
    uint8_t v = (uint8_t)HV_DRIVER_MMBTA_DIRECT_BASE;
    if (nvs_open(HV_NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        if (nvs_get_u8(h, HV_NVS_KEY_DRIVER, &v) != ESP_OK)
            v = (uint8_t)HV_DRIVER_MMBTA_DIRECT_BASE;
        nvs_close(h);
    }

    hv_driver_mode_t m = (hv_driver_mode_t)v;
    if (!mode_valid(m)) m = HV_DRIVER_MMBTA_DIRECT_BASE;
    s_mode = m;
}

static esp_err_t save_driver_mode(hv_driver_mode_t mode)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(HV_NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_u8(h, HV_NVS_KEY_DRIVER, (uint8_t)mode);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

static esp_err_t gpio_input_pullup(void)
{
    if (s_gpio < 0) return ESP_ERR_INVALID_STATE;
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << s_gpio,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    return gpio_config(&io);
}

static esp_err_t gpio_output_level(int level)
{
    if (s_gpio < 0) return ESP_ERR_INVALID_STATE;

    // Preload output latch before enabling output driver.
    gpio_set_level(s_gpio, level ? 1 : 0);

    gpio_config_t io = {
        .pin_bit_mask = 1ULL << s_gpio,
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&io);
    if (err == ESP_OK) gpio_set_level(s_gpio, level ? 1 : 0);
    return err;
}

static esp_err_t gpio_output_od_high_with_pullup(void)
{
    if (s_gpio < 0) return ESP_ERR_INVALID_STATE;

    // Open-drain HIGH means the output transistor is off. The internal pull-up
    // brings the external MOSFET gate/base high weakly, without ever driving a
    // hard push-pull HIGH into a temporary BJT base connection.
    gpio_set_level(s_gpio, 1);

    gpio_config_t io = {
        .pin_bit_mask = 1ULL << s_gpio,
        .mode         = GPIO_MODE_OUTPUT_OD,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&io);
    if (err == ESP_OK) gpio_set_level(s_gpio, 1);
    return err;
}

static esp_err_t hv_drive_off(void)
{
    if (s_gpio < 0) return ESP_ERR_INVALID_STATE;

    switch (s_mode) {
    case HV_DRIVER_OPEN_DRAIN_PULLUP:
        // Open-drain HIGH + internal pull-up -> 2N7002 ON -> EN pulled to GND -> HV OFF.
        // This avoids push-pull HIGH and is safer during prototype experiments.
        return gpio_output_od_high_with_pullup();

    case HV_DRIVER_MMBTA_DIRECT_BASE:
    default:
        // Direct BJT-base temporary mode. Do not drive push-pull HIGH because there
        // may be no external base resistor. Internal pull-up weakly biases the base.
        return gpio_input_pullup();
    }
}

static esp_err_t hv_drive_on(void)
{
    if (s_gpio < 0) return ESP_ERR_INVALID_STATE;

    // Both supported modes turn HV ON by pulling the control transistor gate/base LOW:
    // MOSFET/BJT off -> EN rises using the module's built-in 5 V pull-up.
    return gpio_output_level(0);
}

esp_err_t hv_set_driver_mode(hv_driver_mode_t mode, bool save_to_nvs)
{
    if (!mode_valid(mode)) return ESP_ERR_INVALID_ARG;

    // Changing the driver while HV is running is not allowed. Force a safe OFF first.
    hv_driver_mode_t old = s_mode;
    esp_err_t err = ESP_OK;

    s_mode = mode;
    err = hv_drive_off();
    if (err != ESP_OK) {
        s_mode = old;
        hv_drive_off();
        return err;
    }

    s_on = false;
    s_last_change_us = esp_timer_get_time();

    if (save_to_nvs) {
        err = save_driver_mode(mode);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "failed to save HV driver mode: %s", esp_err_to_name(err));
            return err;
        }
    }

    ESP_LOGI(TAG, "HV driver mode: %s; HV forced OFF", mode_to_name(s_mode));
    return ESP_OK;
}

esp_err_t hv_init(int gpio_en)
{
    s_gpio = gpio_en;
    s_on = false;
    s_last_change_us = 0;

    load_driver_mode();

    // Start with HV forced OFF as early as this driver is initialized.
    esp_err_t err = hv_drive_off();
    ESP_LOGI(TAG, "HV OFF at init, driver=%s", mode_to_name(s_mode));
    return err;
}

esp_err_t hv_on(void)
{
    if (s_gpio < 0) return ESP_ERR_INVALID_STATE;
    if (s_on) return ESP_OK;

    int64_t now = esp_timer_get_time();
    if (s_last_change_us != 0 && now - s_last_change_us < HV_ON_COOLDOWN_US) {
        ESP_LOGW(TAG, "HV ON blocked by cooldown");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = hv_drive_on();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to drive HV ON: %s", esp_err_to_name(err));
        return err;
    }

    s_on = true;
    s_last_change_us = now;
    ESP_LOGI(TAG, "HV ON, driver=%s", mode_to_name(s_mode));
    return ESP_OK;
}

esp_err_t hv_off(void)
{
    if (s_gpio < 0) return ESP_ERR_INVALID_STATE;

    esp_err_t err = hv_drive_off();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to drive HV OFF: %s", esp_err_to_name(err));
        return err;
    }

    if (s_on) ESP_LOGI(TAG, "HV OFF");
    s_on = false;
    s_last_change_us = esp_timer_get_time();
    return ESP_OK;
}

bool hv_is_on(void) { return s_on; }
