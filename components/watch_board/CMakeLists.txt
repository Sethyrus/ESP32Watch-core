idf_component_register(
    SRCS "imu_service.c" "watch_buttons.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_i2c esp_driver_gpio
)
