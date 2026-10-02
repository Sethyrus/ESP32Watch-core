#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "lvgl.h"

// AMOLED + touch + LVGL, replacing bsp_display_start(). The BSP registers this QSPI
// panel as an RGB display (heap overrun, flush not waiting for DMA; core
// docs/GOTCHAS.md); this registers it as an SPI display. Lock LVGL with
// bsp_display_lock()/bsp_display_unlock() as usual.

typedef struct {
    int brightness;   // 0..100
    int buffer_lines; // LVGL draw buffer height; two buffers, internal DMA RAM
    bool touch;       // add the FT3168 as an LVGL input device
} watch_display_config_t;

#define WATCH_DISPLAY_CONFIG_DEFAULT() {.brightness = 80, .buffer_lines = 50, .touch = true}

// NULL config = defaults. Returns NULL on failure.
lv_display_t *watch_display_start(const watch_display_config_t *config);

// Remembered, and restored by watch_display_wake().
esp_err_t watch_display_set_brightness(int percent);
int watch_display_get_brightness(void);

// Panel off and in sleep mode (display off + sleep in), or back on (sleep out, 120 ms,
// display on, brightness). Sleep also turns the touch off (INT interrupt and LVGL input
// device) and wake turns it back on 100 ms after the panel. Stop LVGL and hold its lock
// first (watch_power_sleep() does all of it).
esp_err_t watch_display_sleep(void);
esp_err_t watch_display_wake(void);
bool watch_display_is_asleep(void);

esp_lcd_panel_io_handle_t watch_display_get_io(void);
