#pragma once
#include "driver/i2c_master.h"
#include "esp_err.h"

// Thin driver for TCA9535 16-bit I2C I/O expander.
// All pins are used as push-pull outputs. Bit = 1 -> pin HIGH -> NPN on -> cathode lit.
typedef i2c_master_dev_handle_t tca9535_handle_t;

// Add the device on an existing I2C master bus.
esp_err_t tca9535_create(i2c_master_bus_handle_t bus, uint8_t addr,
                         uint32_t scl_hz, tca9535_handle_t *out);

// Configure both ports as outputs, polarity normal, outputs driven LOW (all off).
esp_err_t tca9535_config_all_outputs(tca9535_handle_t h);

// Write a single output port (port 0 or 1).
esp_err_t tca9535_write_port(tca9535_handle_t h, uint8_t port, uint8_t value);

// Write both output ports in one transaction.
esp_err_t tca9535_write_both(tca9535_handle_t h, uint8_t p0, uint8_t p1);
