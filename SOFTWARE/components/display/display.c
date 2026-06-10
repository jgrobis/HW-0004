#include "display.h"
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "display";

static tca9535_handle_t s_dev[2];
static display_cell_t   s_lut[TUBE_COUNT][10];
static uint8_t          s_shadow[2][2];   // last written [exp][port]
static SemaphoreHandle_t s_mtx;
static display_error_cb_t s_error_cb = NULL;
static volatile bool s_healthy = false;

#define NVS_NS    "nixie"
#define NVS_KEY   "lut"
#define NVS_VKEY  "lut_ver"
#define LUT_VER   1u

static inline void cell(tube_t t, int d, uint8_t e, uint8_t p, uint8_t pin)
{
    s_lut[t][d] = (display_cell_t){ .exp = e, .port = p, .pin = pin, .present = true };
}

// Default mapping from HW-SW_ICD.xlsx + Schematic1 (REGxs -> expander/port).
// exp0 = U500: P0 = REG1s, P1 = REG2s ; exp1 = U501: P0 = REG3s, P1 = REG4s
static void load_default_lut(void)
{
    memset(s_lut, 0, sizeof(s_lut));

    // Hours tens: only 1,2 wired
    cell(TUBE_H10, 1, 0, 0, 0);   // REG1s0
    cell(TUBE_H10, 2, 0, 0, 1);   // REG1s1

    // Hours units 0-9
    cell(TUBE_H1, 0, 0, 1, 0);    // REG2s0
    cell(TUBE_H1, 1, 0, 0, 6);    // REG1s6
    cell(TUBE_H1, 2, 0, 0, 5);    // REG1s5
    cell(TUBE_H1, 3, 0, 0, 4);    // REG1s4
    cell(TUBE_H1, 4, 0, 0, 3);    // REG1s3
    cell(TUBE_H1, 5, 0, 0, 2);    // REG1s2
    cell(TUBE_H1, 6, 0, 0, 7);    // REG1s7
    cell(TUBE_H1, 7, 0, 1, 3);    // REG2s3
    cell(TUBE_H1, 8, 0, 1, 2);    // REG2s2
    cell(TUBE_H1, 9, 0, 1, 1);    // REG2s1

    // Minutes tens 0-5 (+6 wired but unused by clock)
    cell(TUBE_M10, 0, 1, 0, 2);   // REG3s2
    cell(TUBE_M10, 1, 1, 0, 0);   // REG3s0
    cell(TUBE_M10, 2, 0, 1, 7);   // REG2s7
    cell(TUBE_M10, 3, 0, 1, 6);   // REG2s6
    cell(TUBE_M10, 4, 0, 1, 5);   // REG2s5
    cell(TUBE_M10, 5, 0, 1, 4);   // REG2s4
    cell(TUBE_M10, 6, 1, 0, 1);   // REG3s1 (unused in normal clock)

    // Minutes units 0-9
    cell(TUBE_M1, 0, 1, 1, 2);    // REG4s2
    cell(TUBE_M1, 1, 1, 1, 1);    // REG4s1
    cell(TUBE_M1, 2, 1, 1, 0);    // REG4s0
    cell(TUBE_M1, 3, 1, 0, 7);    // REG3s7
    cell(TUBE_M1, 4, 1, 0, 6);    // REG3s6
    cell(TUBE_M1, 5, 1, 0, 5);    // REG3s5
    cell(TUBE_M1, 6, 1, 0, 4);    // REG3s4
    cell(TUBE_M1, 7, 1, 0, 3);    // REG3s3
    cell(TUBE_M1, 8, 1, 1, 4);    // REG4s4
    cell(TUBE_M1, 9, 1, 1, 3);    // REG4s3
}

// REG bit index 0..28 -> expander/port/pin
static display_cell_t idx_to_cell(int i)
{
    display_cell_t c = { .present = true };
    c.exp  = (uint8_t)(i / 16);
    int w  = i % 16;
    c.port = (uint8_t)(w / 8);
    c.pin  = (uint8_t)(w % 8);
    return c;
}

static void display_fault_locked(esp_err_t err)
{
    s_healthy = false;
    ESP_LOGE(TAG, "display I2C write failed: %s - forcing safe state", esp_err_to_name(err));
    if (s_error_cb) s_error_cb(err);
}

// Full-state write to both expanders. This is slower than differential writes,
// but safer for Nixie cathodes: every render re-sends a complete known state.
// If any I2C transaction fails, main is notified and HV can be shut down.
static esp_err_t flush_locked(uint8_t buf[2][2])
{
    esp_err_t err = ESP_OK;
    for (int e = 0; e < 2; e++) {
        esp_err_t r = tca9535_write_both(s_dev[e], buf[e][0], buf[e][1]);
        if (r == ESP_OK) {
            s_shadow[e][0] = buf[e][0];
            s_shadow[e][1] = buf[e][1];
        } else {
            err = r;
        }
    }
    if (err != ESP_OK) display_fault_locked(err);
    else s_healthy = true;
    return err;
}

static esp_err_t render_locked(int h10, int h1, int m10, int m1)
{
    uint8_t buf[2][2] = {{0, 0}, {0, 0}};
    int td[TUBE_COUNT] = { h10, h1, m10, m1 };
    for (int t = 0; t < TUBE_COUNT; t++) {
        int d = td[t];
        if (d != DISPLAY_BLANK && d >= 0 && d < 10) {
            display_cell_t c = s_lut[t][d];
            if (c.present && c.exp < 2 && c.port < 2 && c.pin < 8) {
                buf[c.exp][c.port] |= (1u << c.pin);
            } else if (c.present) {
                ESP_LOGE(TAG, "invalid LUT cell t=%d d=%d exp=%u port=%u pin=%u",
                         t, d, c.exp, c.port, c.pin);
                return ESP_ERR_INVALID_STATE;
            }
        }
    }
    return flush_locked(buf);
}

esp_err_t display_init(tca9535_handle_t dev_u500, tca9535_handle_t dev_u501)
{
    s_dev[0] = dev_u500;
    s_dev[1] = dev_u501;
    s_mtx = xSemaphoreCreateMutex();
    if (!s_mtx) return ESP_ERR_NO_MEM;
    load_default_lut();
    memset(s_shadow, 0xFF, sizeof(s_shadow)); // force first write
    s_healthy = false;
    return display_blank_all();
}

void display_set_error_callback(display_error_cb_t cb)
{
    s_error_cb = cb;
}

bool display_is_healthy(void)
{
    return s_healthy;
}

esp_err_t display_set_time(int hh, int mm)
{
    int h10 = (hh >= 10) ? hh / 10 : DISPLAY_BLANK; // leading-zero blanking
    int h1  = hh % 10;
    int m10 = mm / 10;
    int m1  = mm % 10;
    return display_set_raw(h10, h1, m10, m1);
}

esp_err_t display_set_raw(int h10, int h1, int m10, int m1)
{
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    esp_err_t r = render_locked(h10, h1, m10, m1);
    xSemaphoreGive(s_mtx);
    return r;
}

esp_err_t display_blank_all(void)
{
    return display_set_raw(DISPLAY_BLANK, DISPLAY_BLANK, DISPLAY_BLANK, DISPLAY_BLANK);
}

esp_err_t display_calib(int bit_index)
{
    if (bit_index < 0 || bit_index > 28) return ESP_ERR_INVALID_ARG;
    display_cell_t c = idx_to_cell(bit_index);
    uint8_t buf[2][2] = {{0, 0}, {0, 0}};
    buf[c.exp][c.port] |= (1u << c.pin);
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    esp_err_t r = flush_locked(buf);
    xSemaphoreGive(s_mtx);
    ESP_LOGI(TAG, "calib bit %d -> exp%d P%d.%d", bit_index, c.exp, c.port, c.pin);
    return r;
}

void display_describe_bit(int bit_index, char *buf, size_t len)
{
    if (bit_index < 0 || bit_index > 28) { snprintf(buf, len, "invalid"); return; }
    display_cell_t c = idx_to_cell(bit_index);
    const char *tube_name[TUBE_COUNT] = { "H10", "H1", "M10", "M1" };
    int ft = -1, fd = -1;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    for (int t = 0; t < TUBE_COUNT && ft < 0; t++)
        for (int d = 0; d < 10; d++)
            if (s_lut[t][d].present && s_lut[t][d].exp == c.exp &&
                s_lut[t][d].port == c.port && s_lut[t][d].pin == c.pin) {
                ft = t; fd = d; break;
            }
    xSemaphoreGive(s_mtx);
    if (ft >= 0)
        snprintf(buf, len, "exp%d P%d.%d -> expect %s digit %d",
                 c.exp, c.port, c.pin, tube_name[ft], fd);
    else
        snprintf(buf, len, "exp%d P%d.%d -> (not in LUT / unused)", c.exp, c.port, c.pin);
}

esp_err_t display_lut_set(tube_t tube, int digit, uint8_t exp, uint8_t port, uint8_t pin, bool present)
{
    if ((int)tube < 0 || tube >= TUBE_COUNT || digit < 0 || digit > 9 || exp > 1 || port > 1 || pin > 7)
        return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    s_lut[tube][digit] = (display_cell_t){ exp, port, pin, present };
    xSemaphoreGive(s_mtx);
    return ESP_OK;
}

esp_err_t display_lut_save_nvs(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    err = nvs_set_blob(h, NVS_KEY, s_lut, sizeof(s_lut));
    xSemaphoreGive(s_mtx);
    if (err == ESP_OK) err = nvs_set_u32(h, NVS_VKEY, LUT_VER);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

esp_err_t display_lut_load_nvs(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (err != ESP_OK) return err;
    uint32_t ver = 0;
    if (nvs_get_u32(h, NVS_VKEY, &ver) != ESP_OK || ver != LUT_VER) {
        nvs_close(h);
        return ESP_ERR_NVS_NOT_FOUND;   // no/incompatible stored LUT -> keep defaults
    }
    display_cell_t tmp[TUBE_COUNT][10];
    size_t sz = sizeof(tmp);
    err = nvs_get_blob(h, NVS_KEY, tmp, &sz);
    nvs_close(h);
    if (err != ESP_OK) return err;
    if (sz != sizeof(tmp)) return ESP_ERR_INVALID_SIZE;

    for (int t = 0; t < TUBE_COUNT; t++) {
        for (int d = 0; d < 10; d++) {
            display_cell_t c = tmp[t][d];
            if (c.present && (c.exp > 1 || c.port > 1 || c.pin > 7)) {
                ESP_LOGE(TAG, "invalid LUT in NVS, keeping compiled default");
                return ESP_ERR_INVALID_STATE;
            }
        }
    }

    xSemaphoreTake(s_mtx, portMAX_DELAY);
    memcpy(s_lut, tmp, sizeof(s_lut));
    xSemaphoreGive(s_mtx);
    ESP_LOGI(TAG, "validated LUT loaded from NVS");
    return ESP_OK;
}

esp_err_t display_lut_get(tube_t tube, int digit, display_cell_t *out)
{
    if ((int)tube < 0 || tube >= TUBE_COUNT || digit < 0 || digit > 9 || !out) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    *out = s_lut[tube][digit];
    xSemaphoreGive(s_mtx);
    return ESP_OK;
}
