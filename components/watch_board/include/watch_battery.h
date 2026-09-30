#pragma once

#include <stdbool.h>

#include "esp_err.h"

// Battery and charger state from the AXP2101 (I2C, BSP bus).

typedef enum {
    WATCH_BATTERY_IDLE = 0,
    WATCH_BATTERY_CHARGING,
    WATCH_BATTERY_DISCHARGING,
} watch_battery_state_t;

typedef struct {
    bool present;              // a battery is connected
    bool usb;                  // VBUS present (USB power)
    watch_battery_state_t state;
    int millivolts;            // 0 without battery
    int percent;               // AXP2101 fuel gauge, 0..100; -1 without battery
} watch_battery_t;

// Enables battery detection and voltage measurement. Safe to call more than once.
esp_err_t watch_battery_init(void);

// A few I2C reads; fine once a second, not every frame. Needs watch_battery_init().
esp_err_t watch_battery_read(watch_battery_t *out);
