#pragma once

#include <stdbool.h>

#include "esp_err.h"

// Raw button primitives. Debounce and press-duration logic stay in each app.

// BOOT button on GPIO0, active low with internal pull-up.
esp_err_t watch_boot_button_init(void);
bool watch_boot_button_is_pressed(void);

// PWR button short press, reported by the AXP2101 PMU (PWRON is not a direct GPIO).
// Requires the BSP I2C bus to be initialized (e.g. after bsp_display_start/new).
esp_err_t watch_pwr_key_init(void);
bool watch_pwr_key_is_available(void);

// Reads and clears the AXP2101 short-press IRQ flag.
// On a clear failure, *pressed is still true and the write error is returned.
esp_err_t watch_pwr_key_take_short_press(bool *pressed);
