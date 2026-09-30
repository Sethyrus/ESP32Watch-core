#pragma once

#include "esp_err.h"

// Default NVS partition. In launcher mode it is shared by every app, so each app
// uses its own namespace (table in core docs/ARCHITECTURE.md, "Persistencia").

// Initializes it. Safe to call more than once. If it is full or has an older format,
// it is erased and re-initialized, which drops the settings of every app (logged).
esp_err_t watch_nvs_init(void);
