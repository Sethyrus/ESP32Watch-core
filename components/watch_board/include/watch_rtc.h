#pragma once

#include <stdbool.h>
#include <time.h>

#include "esp_err.h"

// PCF85063 real-time clock (I2C 0x51) on the BSP bus. The RTC keeps local time (no
// time zone); init copies it into the system clock, so time()/localtime_r() work
// afterwards without further I2C traffic.

// If the RTC lost its time (oscillator-stop flag), or set_from_build is true, the
// build time (__DATE__ __TIME__ of watch_rtc.c, compiled in the app's build/) is
// written first. Can be called again; the I2C device is only added once.
esp_err_t watch_rtc_init(bool set_from_build);

// Sets hours and minutes (seconds to 0, date kept) in the RTC and the system clock.
esp_err_t watch_rtc_set_time(int hours, int minutes);

// Sets date and time (local time, tm_year from 1900, tm_mon 0..11; the weekday is
// recomputed) in the RTC and the system clock. Years 2000..2099.
esp_err_t watch_rtc_set_datetime(const struct tm *tm);

// True when the last watch_rtc_init() found the RTC had lost the time (so it now holds
// the build time and the user should set it).
bool watch_rtc_time_was_lost(void);
