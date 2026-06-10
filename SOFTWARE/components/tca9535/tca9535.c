#include "tca9535.h"
#include "esp_log.h"

#define TCA_CMD_OUTPUT0    0x02
#define TCA_CMD_POLARITY0  0x04
#define TCA_CMD_CONFIG0    0x06
#define TCA_TIMEOUT_MS     100

static const char *TAG = "tca9535";

esp_err_t tca9535_create(i2c_master_bus_handle_t bus, uint8_t addr,
                         uint32_t scl_hz, tca9535_handle_t *out)
{
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = addr,
        .scl_speed_hz    = scl_hz,
    };
    esp_err_t err = i2c_master_bus_add_device(bus, &cfg, out);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "add device 0x%02X failed: %s", addr, esp_err_to_name(err));
    }
    return err;
}

esp_err_t tca9535_config_all_outputs(tca9535_handle_t h)
{
    esp_err_t err;
    uint8_t cfg[3]  = {TCA_CMD_CONFIG0,   0x00, 0x00}; // 0 = output on every pin
    uint8_t pol[3]  = {TCA_CMD_POLARITY0, 0x00, 0x00}; // no inversion
    uint8_t out[3]  = {TCA_CMD_OUTPUT0,   0x00, 0x00}; // all low (cathodes off)

    if ((err = i2c_master_transmit(h, out, 3, TCA_TIMEOUT_MS)) != ESP_OK) return err;
    if ((err = i2c_master_transmit(h, pol, 3, TCA_TIMEOUT_MS)) != ESP_OK) return err;
    return i2c_master_transmit(h, cfg, 3, TCA_TIMEOUT_MS);
}

esp_err_t tca9535_write_port(tca9535_handle_t h, uint8_t port, uint8_t value)
{
    uint8_t buf[2] = {(uint8_t)(TCA_CMD_OUTPUT0 + (port & 1)), value};
    return i2c_master_transmit(h, buf, 2, TCA_TIMEOUT_MS);
}

esp_err_t tca9535_write_both(tca9535_handle_t h, uint8_t p0, uint8_t p1)
{
    uint8_t buf[3] = {TCA_CMD_OUTPUT0, p0, p1};
    return i2c_master_transmit(h, buf, 3, TCA_TIMEOUT_MS);
}
