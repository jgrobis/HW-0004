#pragma once
#include <stdbool.h>
#include "esp_err.h"

// Supported safe interfaces for the HV boost converter EN pin.
// The module EN pin has its own pull-up to about 5 V, so EN must NOT be tied
// directly to an ESP32 GPIO. Both modes below only pull EN down to GND.
typedef enum {
    // Temporary/prototype wiring:
    //   EN module -> collector MMBTA42
    //   emitter   -> DGND
    //   ESP GPIO  -> base MMBTA42, currently possibly without external resistor
    // To protect GPIO, OFF uses INPUT + internal pull-up, ON uses OUTPUT LOW.
    HV_DRIVER_MMBTA_DIRECT_BASE = 0,

    // Recommended MOSFET open-drain wiring:
    //   EN module -> drain 2N7002/BSS138
    //   source    -> DGND
    //   ESP GPIO  -> gate through ~100R..1k
    // OFF uses GPIO open-drain HIGH + internal pull-up, ON drives GPIO LOW.
    HV_DRIVER_OPEN_DRAIN_PULLUP = 1,
} hv_driver_mode_t;

// Controls the boost-converter EN pin (170 VDC). Public API remains normal:
// hv_on() = HV ON, hv_off() = HV OFF, regardless of selected driver mode.
// hv_on() has a short cooldown after OFF to protect against rapid web/API spam.
esp_err_t hv_init(int gpio_en);
esp_err_t hv_on(void);
esp_err_t hv_off(void);
bool      hv_is_on(void);

hv_driver_mode_t hv_get_driver_mode(void);
const char *hv_driver_mode_name(void);
esp_err_t hv_set_driver_mode(hv_driver_mode_t mode, bool save_to_nvs);
