#include "watch_power.h"

#include <stdbool.h>

#include "bsp/esp-bsp.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "watch_buttons.h"
#include "watch_display.h"
#include "watch_pmu_priv.h"

#define BOOT_GPIO GPIO_NUM_0
#define PWR_POLL_US 200000
#define USB_AWAKE_POLL_MS 50
#define USB_CHECK_US 1000000
#define QMI8658_ADDR 0x6B
#define QMI8658_CTRL7 0x08 // bit 0 accel, bit 1 gyro enable
#define SPEAKER_AMP_GPIO GPIO_NUM_46

static const char *TAG = "watch_power";

static bool usb_present(void)
{
    uint8_t s1 = 0;
    uint8_t s2 = 0;
    if (watch_pmu_read(AXP2101_STATUS1, &s1) != ESP_OK || watch_pmu_read(AXP2101_STATUS2, &s2) != ESP_OK) {
        return false; // assume battery: light sleep is the safe default for power
    }
    return (s1 & 0x20) && !(s2 & 0x08);
}

static bool take_pwr(void)
{
    bool pressed = false;
    return watch_pwr_key_take_short_press(&pressed) == ESP_OK && pressed;
}

// One light sleep until BOOT or the PWR poll timer.
static bool light_sleep_once(void)
{
    gpio_wakeup_enable(BOOT_GPIO, GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();
    esp_sleep_enable_timer_wakeup(PWR_POLL_US);
    esp_light_sleep_start();
    const bool boot = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO || watch_boot_button_is_pressed();
    gpio_wakeup_disable(BOOT_GPIO);
    gpio_set_intr_type(BOOT_GPIO, GPIO_INTR_DISABLE);
    return boot;
}

// Panel off until BOOT, PWR or the deadline. The chip stays awake (50 ms polls) with
// USB power or while stay_awake() returns true; otherwise it light-sleeps.
static watch_wake_t screen_off_until(uint32_t timeout_ms, bool (*stay_awake)(void))
{
    const int64_t start = esp_timer_get_time();
    const int64_t deadline = timeout_ms != 0 ? start + (int64_t)timeout_ms * 1000 : INT64_MAX;
    take_pwr(); // drop a press that happened before sleeping

    lvgl_port_lock(0);
    lvgl_port_stop();
    watch_display_sleep();

    bool usb = usb_present();
    int64_t next_usb_check = start + USB_CHECK_US;
    watch_wake_t reason;
    for (;;) {
        bool boot;
        if (usb || (stay_awake != NULL && stay_awake())) {
            vTaskDelay(pdMS_TO_TICKS(USB_AWAKE_POLL_MS));
            boot = watch_boot_button_is_pressed();
        } else {
            boot = light_sleep_once();
        }
        const int64_t now = esp_timer_get_time();
        if (boot) {
            reason = WATCH_WAKE_BOOT;
            break;
        }
        if (take_pwr()) {
            reason = WATCH_WAKE_PWR;
            break;
        }
        if (now >= deadline) {
            reason = WATCH_WAKE_TIMEOUT;
            break;
        }
        if (now >= next_usb_check) {
            next_usb_check = now + USB_CHECK_US;
            usb = usb_present();
        }
    }

    watch_display_wake();
    lvgl_port_resume();
    lv_obj_invalidate(lv_screen_active());
    lv_display_trigger_activity(NULL);
    lvgl_port_unlock();

    if (reason == WATCH_WAKE_BOOT) {
        while (watch_boot_button_is_pressed()) {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
    return reason;
}

watch_wake_t watch_power_sleep(uint32_t timeout_ms)
{
    return screen_off_until(timeout_ms, NULL);
}

static bool always(void)
{
    return true;
}

watch_wake_t watch_power_screen_off(uint32_t timeout_ms, bool (*stay_awake)(void))
{
    return screen_off_until(timeout_ms, stay_awake != NULL ? stay_awake : always);
}

void watch_power_quiet_peripherals(void)
{
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (bus != NULL) {
        const i2c_device_config_t cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = QMI8658_ADDR,
            .scl_speed_hz = 400000,
        };
        i2c_master_dev_handle_t imu = NULL;
        if (i2c_master_bus_add_device(bus, &cfg, &imu) == ESP_OK) {
            const uint8_t off[2] = {QMI8658_CTRL7, 0x00};
            if (i2c_master_transmit(imu, off, sizeof(off), 20) != ESP_OK) {
                ESP_LOGW(TAG, "IMU not answering; left as is");
            }
            i2c_master_bus_rm_device(imu);
        }
    }
    gpio_reset_pin(SPEAKER_AMP_GPIO);
    gpio_set_direction(SPEAKER_AMP_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(SPEAKER_AMP_GPIO, 0);
}

void watch_power_off(void)
{
    uint8_t v = 0;
    ESP_LOGI(TAG, "Power off");
    if (watch_pmu_read(AXP2101_COMMON_CONFIG, &v) == ESP_OK) {
        watch_pmu_write(AXP2101_COMMON_CONFIG, v | 0x01);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
}
