#include "watch_buttons.h"

#include <stddef.h>

#include "bsp/esp-bsp.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_timer.h"

#define WATCH_BOOT_GPIO GPIO_NUM_0
#define WATCH_BOOT_DEFAULT_DEBOUNCE_MS 30

#define AXP2101_ADDR 0x34
#define AXP2101_INTEN2 0x41
#define AXP2101_INTSTS2 0x49
#define AXP2101_PKEY_SHORT_IRQ_BIT (1 << 3)
#define AXP2101_POLL_TIMEOUT_MS 5
#define AXP2101_INIT_TIMEOUT_MS 50
#define AXP2101_CLEAR_TIMEOUT_MS 20

static const char *TAG = "watch_buttons";

static i2c_master_dev_handle_t s_axp2101_dev;
// Set when a short press was reported but its IRQ flag could not be cleared yet,
// so the same press is not reported twice.
static bool s_clear_pending;

static esp_err_t clear_short_press_flag(void)
{
    const uint8_t clear_data[2] = {AXP2101_INTSTS2, AXP2101_PKEY_SHORT_IRQ_BIT};
    esp_err_t err = i2c_master_transmit(s_axp2101_dev, clear_data, 2, AXP2101_CLEAR_TIMEOUT_MS);
    if (err != ESP_OK) {
        err = i2c_master_transmit(s_axp2101_dev, clear_data, 2, AXP2101_CLEAR_TIMEOUT_MS);
    }
    s_clear_pending = err != ESP_OK;
    return err;
}

esp_err_t watch_boot_button_init(void)
{
    const gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << WATCH_BOOT_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    return gpio_config(&io_conf);
}

bool watch_boot_button_is_pressed(void)
{
    return gpio_get_level(WATCH_BOOT_GPIO) == 0;
}

void watch_boot_debouncer_init(watch_boot_debouncer_t *d, uint32_t debounce_ms, uint32_t long_press_ms)
{
    const bool pressed = watch_boot_button_is_pressed();
    *d = (watch_boot_debouncer_t){
        .debounce_ms = debounce_ms != 0 ? debounce_ms : WATCH_BOOT_DEFAULT_DEBOUNCE_MS,
        .long_press_ms = long_press_ms,
        .raw = pressed,
        .stable = pressed,
        // A press already held at init never reports short or long.
        .long_fired = pressed,
        .changed_us = esp_timer_get_time(),
    };
}

watch_boot_event_t watch_boot_debouncer_poll(watch_boot_debouncer_t *d)
{
    watch_boot_event_t ev = {0};
    const int64_t now = esp_timer_get_time();
    const bool pressed = watch_boot_button_is_pressed();
    if (pressed != d->raw) {
        d->raw = pressed;
        d->changed_us = now;
    }
    if (d->raw != d->stable && now - d->changed_us >= (int64_t)d->debounce_ms * 1000) {
        d->stable = d->raw;
        if (d->stable) {
            ev.down = true;
            d->down_us = now;
            d->long_fired = false;
        } else if (!d->long_fired) {
            ev.short_press = true;
        }
    }
    if (d->stable && !d->long_fired && d->long_press_ms != 0 &&
        now - d->down_us >= (int64_t)d->long_press_ms * 1000) {
        d->long_fired = true;
        ev.long_press = true;
    }
    ev.held = d->stable;
    return ev;
}

esp_err_t watch_pwr_key_init(void)
{
    if (s_axp2101_dev != NULL) {
        return ESP_OK;
    }

    i2c_master_bus_handle_t i2c_bus = bsp_i2c_get_handle();
    if (i2c_bus == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    const i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AXP2101_ADDR,
        .scl_speed_hz = 400000,
    };
    i2c_master_dev_handle_t dev = NULL;
    esp_err_t err = i2c_master_bus_add_device(i2c_bus, &dev_cfg, &dev);
    if (err != ESP_OK) {
        return err;
    }

    // Enable AXP2101 short press interrupt (INTEN2 bit 3).
    uint8_t enable_data[2] = {AXP2101_INTEN2, 0x00};
    err = i2c_master_transmit_receive(dev, &enable_data[0], 1, &enable_data[1], 1, AXP2101_INIT_TIMEOUT_MS);
    if (err == ESP_OK) {
        enable_data[1] |= AXP2101_PKEY_SHORT_IRQ_BIT;
        err = i2c_master_transmit(dev, enable_data, 2, AXP2101_INIT_TIMEOUT_MS);
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "PWR short-press IRQ enable failed: %s", esp_err_to_name(err));
        i2c_master_bus_rm_device(dev);
        return err;
    }

    s_axp2101_dev = dev;
    // Drop any short press latched before the app started listening.
    if (clear_short_press_flag() != ESP_OK) {
        ESP_LOGW(TAG, "PWR short-press IRQ initial clear failed");
    }
    return ESP_OK;
}

bool watch_pwr_key_is_available(void)
{
    return s_axp2101_dev != NULL;
}

esp_err_t watch_pwr_key_take_short_press(bool *pressed)
{
    if (pressed == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *pressed = false;
    if (s_axp2101_dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t reg = AXP2101_INTSTS2;
    uint8_t status = 0;
    esp_err_t err = i2c_master_transmit_receive(s_axp2101_dev, &reg, 1, &status, 1, AXP2101_POLL_TIMEOUT_MS);
    if (err != ESP_OK || (status & AXP2101_PKEY_SHORT_IRQ_BIT) == 0) {
        if (err == ESP_OK) {
            s_clear_pending = false;
        }
        return err;
    }

    if (s_clear_pending) {
        // Still the press we already reported; only retry the clear.
        return clear_short_press_flag();
    }

    *pressed = true;
    return clear_short_press_flag();
}
