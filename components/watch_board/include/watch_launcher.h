#pragma once

#include <stdbool.h>

#include "esp_err.h"

// Multi-app mode (ESP32Watch-Launcher): the launcher is the `factory` app and each
// app lives in an OTA slot. Switching apps is a reboot, so every app starts clean.

// Call first thing in app_main. When running from an OTA slot, points the next boot
// back at `factory`, so any reset (Salir, crash, PWR power-off) returns to the launcher.
// No-op when the app runs standalone from `factory` or there is no `factory` partition.
esp_err_t watch_launcher_boot_once(void);

// True when running from an OTA slot with a launcher in `factory` (show "Salir").
bool watch_launcher_is_available(void);

// Reboots into the launcher. Does not return.
void watch_launcher_exit(void);
