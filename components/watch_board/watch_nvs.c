#include "watch_nvs.h"

#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "watch_nvs";

esp_err_t watch_nvs_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS %s: erasing it (settings of every app are lost)",
                 err == ESP_ERR_NVS_NO_FREE_PAGES ? "full" : "has an older format");
        err = nvs_flash_erase();
        if (err == ESP_OK) {
            err = nvs_flash_init();
        }
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS init failed: %s", esp_err_to_name(err));
    }
    return err;
}
