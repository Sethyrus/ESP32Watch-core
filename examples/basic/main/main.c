#include <stdbool.h>

#include "bsp/esp-bsp.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "imu_service.h"
#include "watch_buttons.h"

static const char *TAG = "watch_board_basic";

void app_main(void)
{
    ESP_ERROR_CHECK(bsp_i2c_init());

    ESP_ERROR_CHECK(watch_boot_button_init());

    esp_err_t err = watch_pwr_key_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "PWR key unavailable: %s", esp_err_to_name(err));
    }

    err = imu_service_init();
    if (err == ESP_OK) {
        err = imu_service_calibrate();
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "IMU unavailable: %s", esp_err_to_name(err));
    }

    while (true) {
        imu_service_accel_t accel = {0};
        if (imu_service_is_available() && imu_service_read(&accel) == ESP_OK) {
            ESP_LOGI(TAG, "accel x=%.2f y=%.2f", accel.x, accel.y);
        }

        if (watch_boot_button_is_pressed()) {
            ESP_LOGI(TAG, "BOOT pressed");
        }

        bool pwr_pressed = false;
        if (watch_pwr_key_is_available() &&
            watch_pwr_key_take_short_press(&pwr_pressed) == ESP_OK && pwr_pressed) {
            ESP_LOGI(TAG, "PWR short press");
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
