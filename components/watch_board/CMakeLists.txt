idf_component_register(
    SRCS "imu_service.c" "watch_buttons.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_driver_gpio
)
