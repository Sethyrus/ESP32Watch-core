idf_component_register(
    SRCS "imu_service.c" "watch_battery.c" "watch_buttons.c" "watch_display.c" "watch_launcher.c"
         "watch_nvs.c" "watch_pmu.c" "watch_power.c" "watch_rtc.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_i2c esp_driver_gpio esp_lcd lvgl__lvgl
    PRIV_REQUIRES app_update esp_partition esp_timer log nvs_flash espressif__esp_lvgl_port espressif__esp_lcd_touch
)
