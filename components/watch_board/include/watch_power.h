#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

// Screen-off sleep, peripheral cleanup and power off.

typedef enum {
    WATCH_WAKE_BOOT = 0, // BOOT pressed (returns once it is released)
    WATCH_WAKE_PWR,      // PWR short press (the press is consumed)
    WATCH_WAKE_TIMEOUT,  // timeout_ms elapsed
} watch_wake_t;

// Turns the screen off and sleeps until BOOT, a PWR short press or timeout_ms
// (0 = no timeout); then turns the screen back on and redraws it. Touch does not wake.
// On battery the chip light-sleeps between 200 ms PWR polls (RAM, PSRAM and LVGL
// state are kept). With USB power it stays awake and only the panel sleeps, since
// USB-Serial-JTAG stops working in light sleep; plugging or unplugging USB while
// asleep switches mode without waking the screen.
// Needs watch_display_start(), watch_boot_button_init() and watch_pwr_key_init().
// Call it from a task that does not hold the LVGL lock.
watch_wake_t watch_power_sleep(uint32_t timeout_ms);

// Like watch_power_sleep(), but the chip does not light-sleep while stay_awake()
// returns true (NULL = always): the panel is off and LVGL paused, while other tasks
// (audio capture, SD writes) keep running. stay_awake() is called every 50 ms from
// the calling task; once it returns false the chip light-sleeps as watch_power_sleep()
// does, without turning the screen on. Same requirements as watch_power_sleep().
watch_wake_t watch_power_screen_off(uint32_t timeout_ms, bool (*stay_awake)(void));

// Puts in low power what an app may have left running across the reboot into this
// firmware (esp_restart() does not reset them): the QMI8658 IMU and the speaker amp.
void watch_power_quiet_peripherals(void);

// Cuts power through the AXP2101. Does not return while on battery; with USB
// connected the board may keep running.
void watch_power_off(void);
