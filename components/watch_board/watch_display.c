#include "watch_display.h"

#include "bsp/display.h"
#include "bsp/esp-bsp.h"
#include "bsp/touch.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LCD_CMD_SLPIN 0x10
#define LCD_CMD_SLPOUT 0x11
#define LCD_CMD_DISPOFF 0x28
#define LCD_CMD_DISPON 0x29
#define LCD_SLPOUT_DELAY_MS 120

static const char *TAG = "watch_display";

static esp_lcd_panel_io_handle_t s_io;
static esp_lcd_panel_handle_t s_panel;
static lv_display_t *s_disp;
static int s_brightness = 80;
static bool s_asleep;

// The panel needs windows starting on even and ending on odd coordinates.
static void rounder_cb(lv_event_t *e)
{
    lv_area_t *a = (lv_area_t *)lv_event_get_param(e);
    a->x1 &= ~1;
    a->y1 &= ~1;
    a->x2 |= 1;
    a->y2 |= 1;
}

// QSPI command encoding used by the SH8601 driver (opcode 0x02, command in bits 15:8).
static esp_err_t panel_cmd(uint8_t cmd)
{
    return esp_lcd_panel_io_tx_param(s_io, (0x02 << 24) | (cmd << 8), NULL, 0);
}

lv_display_t *watch_display_start(const watch_display_config_t *config)
{
    const watch_display_config_t defaults = WATCH_DISPLAY_CONFIG_DEFAULT();
    const watch_display_config_t *cfg = config != NULL ? config : &defaults;
    if (s_disp != NULL) {
        return s_disp;
    }

    const lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    esp_err_t err = lvgl_port_init(&port_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "lvgl_port_init: %s", esp_err_to_name(err));
        return NULL;
    }
    const int lines = cfg->buffer_lines > 0 ? cfg->buffer_lines : 50;
    const bsp_display_config_t bsp_cfg = {
        .max_transfer_sz = BSP_LCD_H_RES * lines * BSP_LCD_BITS_PER_PIXEL / 8,
    };
    err = bsp_display_new(&bsp_cfg, &s_panel, &s_io);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "bsp_display_new: %s", esp_err_to_name(err));
        return NULL;
    }
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = s_io,
        .panel_handle = s_panel,
        .buffer_size = BSP_LCD_H_RES * lines,
        .double_buffer = true,
        .hres = BSP_LCD_H_RES,
        .vres = BSP_LCD_V_RES,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .buff_dma = true,
            .sw_rotate = true,
            .swap_bytes = true,
        },
    };
    s_disp = lvgl_port_add_disp(&disp_cfg);
    if (s_disp == NULL) {
        ESP_LOGE(TAG, "lvgl_port_add_disp failed");
        return NULL;
    }
    lv_display_add_event_cb(s_disp, rounder_cb, LV_EVENT_INVALIDATE_AREA, NULL);

    if (cfg->touch) {
        esp_lcd_touch_handle_t tp = NULL;
        err = bsp_touch_new(NULL, &tp);
        if (err == ESP_OK) {
            const lvgl_port_touch_cfg_t touch_cfg = {.disp = s_disp, .handle = tp};
            if (lvgl_port_add_touch(&touch_cfg) == NULL) {
                ESP_LOGW(TAG, "Touch input not registered");
            }
        } else {
            ESP_LOGW(TAG, "Touch unavailable: %s", esp_err_to_name(err));
        }
    }
    watch_display_set_brightness(cfg->brightness);
    return s_disp;
}

esp_err_t watch_display_set_brightness(int percent)
{
    s_brightness = percent < 0 ? 0 : (percent > 100 ? 100 : percent);
    return s_asleep ? ESP_OK : bsp_display_brightness_set(s_brightness);
}

int watch_display_get_brightness(void)
{
    return s_brightness;
}

esp_err_t watch_display_sleep(void)
{
    if (s_io == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = panel_cmd(LCD_CMD_DISPOFF);
    if (err == ESP_OK) {
        err = panel_cmd(LCD_CMD_SLPIN);
    }
    s_asleep = err == ESP_OK;
    return err;
}

esp_err_t watch_display_wake(void)
{
    if (s_io == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = panel_cmd(LCD_CMD_SLPOUT);
    vTaskDelay(pdMS_TO_TICKS(LCD_SLPOUT_DELAY_MS));
    if (err == ESP_OK) {
        err = panel_cmd(LCD_CMD_DISPON);
    }
    s_asleep = false;
    bsp_display_brightness_set(s_brightness);
    return err;
}

bool watch_display_is_asleep(void)
{
    return s_asleep;
}

esp_lcd_panel_io_handle_t watch_display_get_io(void)
{
    return s_io;
}
