idf_component_register(
    SRCS "imu_service.c" "watch_buttons.c" "watch_launcher.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_i2c esp_driver_gpio
    PRIV_REQUIRES app_update esp_partition
)
