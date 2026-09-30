#include "watch_battery.h"

#include <string.h>

#include "watch_pmu_priv.h"

esp_err_t watch_battery_init(void)
{
    uint8_t v = 0;
    esp_err_t err = watch_pmu_read(AXP2101_ADC_CTRL, &v);
    if (err == ESP_OK && !(v & 0x01)) {
        err = watch_pmu_write(AXP2101_ADC_CTRL, v | 0x01);
    }
    if (err == ESP_OK) {
        err = watch_pmu_read(AXP2101_BAT_DET_CTRL, &v);
    }
    if (err == ESP_OK && !(v & 0x01)) {
        err = watch_pmu_write(AXP2101_BAT_DET_CTRL, v | 0x01);
    }
    return err;
}

esp_err_t watch_battery_read(watch_battery_t *out)
{
    memset(out, 0, sizeof(*out));
    out->percent = -1;
    uint8_t s1 = 0;
    uint8_t s2 = 0;
    esp_err_t err = watch_pmu_read(AXP2101_STATUS1, &s1);
    if (err == ESP_OK) {
        err = watch_pmu_read(AXP2101_STATUS2, &s2);
    }
    if (err != ESP_OK) {
        return err;
    }
    out->present = s1 & 0x08;
    out->usb = (s1 & 0x20) && !(s2 & 0x08);
    switch (s2 >> 5) {
    case 1: out->state = WATCH_BATTERY_CHARGING; break;
    case 2: out->state = WATCH_BATTERY_DISCHARGING; break;
    default: out->state = WATCH_BATTERY_IDLE; break;
    }
    if (!out->present) {
        return ESP_OK;
    }
    uint8_t h = 0;
    uint8_t l = 0;
    uint8_t pct = 0;
    err = watch_pmu_read(AXP2101_VBAT_H, &h);
    if (err == ESP_OK) {
        err = watch_pmu_read(AXP2101_VBAT_L, &l);
    }
    if (err == ESP_OK) {
        err = watch_pmu_read(AXP2101_BAT_PERCENT, &pct);
    }
    if (err != ESP_OK) {
        return err;
    }
    out->millivolts = ((h & 0x1f) << 8) | l;
    out->percent = pct > 100 ? 100 : pct;
    return ESP_OK;
}
