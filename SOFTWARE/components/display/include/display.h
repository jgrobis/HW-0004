#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "tca9535.h"

#define DISPLAY_BLANK   0xFF   // pass as a digit value to blank that tube

typedef enum {
    TUBE_H10 = 0,  // hours tens   (only cathodes 1,2 wired)
    TUBE_H1,       // hours units  (0-9)
    TUBE_M10,      // minutes tens (0-5 used; 6 wired/unused)
    TUBE_M1,       // minutes units(0-9)
    TUBE_COUNT
} tube_t;

// One cathode mapping entry: which expander/port/pin lights (tube,digit).
typedef struct {
    uint8_t exp;     // 0 = U500, 1 = U501
    uint8_t port;    // 0 or 1
    uint8_t pin;     // 0..7
    bool    present; // false = this digit has no wired cathode on this tube
} display_cell_t;

// dev0 = U500 (REG1s/REG2s), dev1 = U501 (REG3s/REG4s)
esp_err_t display_init(tca9535_handle_t dev_u500, tca9535_handle_t dev_u501);

// High-level: show HH:MM with leading-zero blanking on hours tens.
esp_err_t display_set_time(int hh, int mm);

// Raw override: each arg is a digit 0..9 or DISPLAY_BLANK. Order: H10,H1,M10,M1.
esp_err_t display_set_raw(int h10, int h1, int m10, int m1);

// Blank all four tubes (separator stays lit while HV is on).
esp_err_t display_blank_all(void);

// Hardware/display health. If any I2C write fails, display becomes unhealthy
// and the optional callback is called (main uses it to force HV off).
typedef void (*display_error_cb_t)(esp_err_t err);
void display_set_error_callback(display_error_cb_t cb);
bool display_is_healthy(void);

// Calibration: light exactly one cathode by REG bit index 0..28
// (0..7=REG1s, 8..15=REG2s, 16..23=REG3s, 24..28=REG4s).
esp_err_t display_calib(int bit_index);

// Human-readable description of a calibration bit vs the current LUT.
void display_describe_bit(int bit_index, char *buf, size_t len);

// Runtime LUT edit (e.g. from web/calibration) + NVS persistence.
esp_err_t display_lut_set(tube_t tube, int digit, uint8_t exp, uint8_t port, uint8_t pin, bool present);
esp_err_t display_lut_save_nvs(void);
esp_err_t display_lut_load_nvs(void);   // returns ESP_ERR_NVS_NOT_FOUND if none stored
esp_err_t display_lut_get(tube_t tube, int digit, display_cell_t *out);
