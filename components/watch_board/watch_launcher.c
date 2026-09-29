#include "watch_launcher.h"

#include <stddef.h>

#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"

static const char *TAG = "watch_launcher";

static const esp_partition_t *find_factory(void)
{
    return esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
}

bool watch_launcher_is_available(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    return running != NULL && running->subtype != ESP_PARTITION_SUBTYPE_APP_FACTORY && find_factory() != NULL;
}

esp_err_t watch_launcher_boot_once(void)
{
    if (!watch_launcher_is_available()) {
        return ESP_OK;
    }
    const esp_partition_t *factory = find_factory();
    const esp_partition_t *boot = esp_ota_get_boot_partition();
    if (boot == factory) {
        return ESP_OK;
    }
    esp_err_t err = esp_ota_set_boot_partition(factory);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Cannot point next boot at the launcher: %s", esp_err_to_name(err));
    }
    return err;
}

void watch_launcher_exit(void)
{
    ESP_LOGI(TAG, "Returning to launcher");
    esp_restart();
}
