#include "esp_err.h"
#include "esp_log.h"
#include "sdkconfig.h"

#if CONFIG_ESP_WIFI_NVS_ENABLED
#error "C6 Wi-Fi driver credential persistence must remain disabled."
#endif
#if CONFIG_SECURE_BOOT || CONFIG_SECURE_FLASH_ENC_ENABLED
#error "C6 image must not enable irreversible security provisioning."
#endif

extern "C" esp_err_t __wrap_nvs_flash_erase() {
    ESP_LOGE("camera_hub_radio", "Automatic NVS erase refused");
    return ESP_ERR_NOT_ALLOWED;
}

extern "C" esp_err_t __wrap_nvs_flash_erase_partition(const char*) {
    ESP_LOGE("camera_hub_radio", "Automatic NVS partition erase refused");
    return ESP_ERR_NOT_ALLOWED;
}

extern "C" void app_main() {
    ESP_LOGI("camera_hub_radio", "ESP-Hosted 3.1.0 C6 radio; Wi-Fi NVS disabled");
}