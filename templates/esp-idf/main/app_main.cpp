#include "esp_log.h"
#include "sdkconfig.h"

// This skeleton is intentionally not a complete appliance.
// Add adapters, service contracts and a verified encrypted-NVS setup first.

#if !CONFIG_APP_DEV_MODE && CONFIG_APP_DEV_DEFAULT_CREDENTIALS
#error "admin/admin is development-only; disable default credentials for release."
#endif

extern "C" void app_main(void) {
    ESP_LOGI("app", "Minimal ESP-IDF project skeleton - implement AGENTS.md checklist.");
}
