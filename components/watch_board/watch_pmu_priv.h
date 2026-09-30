#pragma once

#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

// Shared AXP2101 (I2C 0x34) access for watch_buttons, watch_battery and watch_power.
// Not public: apps use those headers.

#define AXP2101_STATUS1 0x00  // bit 5 VBUS good, bit 3 battery present
#define AXP2101_STATUS2 0x01  // bits 7:5 charge direction, bit 3 set without VBUS
#define AXP2101_COMMON_CONFIG 0x10 // bit 0: power off
#define AXP2101_ADC_CTRL 0x30 // bit 0: battery voltage measurement
#define AXP2101_VBAT_H 0x34   // battery voltage in mV, 5 + 8 bits
#define AXP2101_VBAT_L 0x35
#define AXP2101_INTEN2 0x41
#define AXP2101_INTSTS2 0x49
#define AXP2101_BAT_DET_CTRL 0x68 // bit 0: battery detection
#define AXP2101_BAT_PERCENT 0xA4  // fuel gauge, 0..100

// Adds the device on the BSP bus the first time; later calls return the same handle.
esp_err_t watch_pmu_get(i2c_master_dev_handle_t *dev);
esp_err_t watch_pmu_read(uint8_t reg, uint8_t *val);
esp_err_t watch_pmu_write(uint8_t reg, uint8_t val);
