#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

// Button primitives: raw BOOT level, a BOOT debouncer and the PWR short press.

// BOOT button on GPIO0, active low with internal pull-up.
esp_err_t watch_boot_button_init(void);
bool watch_boot_button_is_pressed(void);

// Debounced BOOT events. Keep one tracker per app, poll it from a single task every
// few ms (<= 20 ms) and act on the flags of each poll.
typedef struct {
    bool held;        // debounced level
    bool down;        // pressed on this poll
    bool short_press; // released on this poll before long_press_ms (any release if long_press_ms is 0)
    bool long_press;  // held for long_press_ms; fires once, while still held
} watch_boot_event_t;

typedef struct {
    uint32_t debounce_ms;
    uint32_t long_press_ms;
    // Internal state.
    bool raw;
    bool stable;
    bool long_fired;
    int64_t changed_us;
    int64_t down_us;
} watch_boot_debouncer_t;

// debounce_ms 0 means 30 ms. long_press_ms 0 disables long presses. Starts from the
// current level, so a button already held at startup does not report a press.
void watch_boot_debouncer_init(watch_boot_debouncer_t *d, uint32_t debounce_ms, uint32_t long_press_ms);
watch_boot_event_t watch_boot_debouncer_poll(watch_boot_debouncer_t *d);

// PWR button short press, reported by the AXP2101 PMU (PWRON is not a direct GPIO).
// Uses the BSP I2C bus (bsp_i2c_get_handle() initializes it on first use).
esp_err_t watch_pwr_key_init(void);
bool watch_pwr_key_is_available(void);

// Reads and clears the AXP2101 short-press IRQ flag.
// On a clear failure, *pressed is still true and the write error is returned;
// the clear is retried on later calls without reporting the same press again.
esp_err_t watch_pwr_key_take_short_press(bool *pressed);
