#include <stdbool.h>
#include <time.h>

#include "bsp/esp-bsp.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "imu_service.h"
#include "watch_buttons.h"
#include "watch_launcher.h"
#include "watch_nvs.h"
#include "watch_rtc.h"

#define POLL_MS 20
#define LOG_EVERY_POLLS 10
#define BOOT_LONG_PRESS_MS 700

static const char *TAG = "watch_board_basic";

void app_main(void)
{
    watch_launcher_boot_once();
    ESP_ERROR_CHECK(watch_nvs_init());
    ESP_ERROR_CHECK(bsp_i2c_init());

    ESP_ERROR_CHECK(watch_boot_button_init());
    watch_boot_debouncer_t boot;
    watch_boot_debouncer_init(&boot, 0, BOOT_LONG_PRESS_MS);

    esp_err_t err = watch_pwr_key_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "PWR key unavailable: %s", esp_err_to_name(err));
    }

    err = watch_rtc_init(false);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "RTC unavailable: %s", esp_err_to_name(err));
    }

    err = imu_service_init();
    if (err == ESP_OK) {
        err = imu_service_calibrate();
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "IMU unavailable: %s", esp_err_to_name(err));
    }

    for (unsigned polls = 0;; polls++) {
        const watch_boot_event_t ev = watch_boot_debouncer_poll(&boot);
        if (ev.short_press) {
            ESP_LOGI(TAG, "BOOT short press");
        }
        if (ev.long_press) {
            ESP_LOGI(TAG, "BOOT long press");
        }

        if (polls % LOG_EVERY_POLLS == 0) {
            imu_service_accel_t accel = {0};
            if (imu_service_is_available() && imu_service_read(&accel) == ESP_OK) {
                ESP_LOGI(TAG, "accel x=%.2f y=%.2f", accel.x, accel.y);
            }

            bool pwr_pressed = false;
            if (watch_pwr_key_is_available() &&
                watch_pwr_key_take_short_press(&pwr_pressed) == ESP_OK && pwr_pressed) {
                ESP_LOGI(TAG, "PWR short press");
            }
        }

        if (polls % (LOG_EVERY_POLLS * 25) == 0) {
            const time_t now = time(NULL);
            struct tm tm;
            localtime_r(&now, &tm);
            ESP_LOGI(TAG, "time %02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
        }

        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
}
