#include "watch_pmu_priv.h"

#include <stddef.h>

#include "bsp/esp-bsp.h"
#include "freertos/FreeRTOS.h"

#define AXP2101_ADDR 0x34
#define AXP2101_TIMEOUT_MS 20

static i2c_master_dev_handle_t s_dev;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

esp_err_t watch_pmu_get(i2c_master_dev_handle_t *dev)
{
    if (s_dev == NULL) {
        i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
        if (bus == NULL) {
            return ESP_ERR_INVALID_STATE;
        }
        const i2c_device_config_t cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = AXP2101_ADDR,
            .scl_speed_hz = 400000,
        };
        i2c_master_dev_handle_t added = NULL;
        esp_err_t err = i2c_master_bus_add_device(bus, &cfg, &added);
        if (err != ESP_OK) {
            return err;
        }
        portENTER_CRITICAL(&s_lock);
        if (s_dev == NULL) {
            s_dev = added;
            added = NULL;
        }
        portEXIT_CRITICAL(&s_lock);
        if (added != NULL) {
            i2c_master_bus_rm_device(added); // another task won the race
        }
    }
    *dev = s_dev;
    return ESP_OK;
}

esp_err_t watch_pmu_read(uint8_t reg, uint8_t *val)
{
    i2c_master_dev_handle_t dev;
    esp_err_t err = watch_pmu_get(&dev);
    if (err != ESP_OK) {
        return err;
    }
    return i2c_master_transmit_receive(dev, &reg, 1, val, 1, AXP2101_TIMEOUT_MS);
}

esp_err_t watch_pmu_write(uint8_t reg, uint8_t val)
{
    i2c_master_dev_handle_t dev;
    esp_err_t err = watch_pmu_get(&dev);
    if (err != ESP_OK) {
        return err;
    }
    const uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(dev, buf, sizeof(buf), AXP2101_TIMEOUT_MS);
}
