#include "esp_hosted.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "c6_update/UpdateService.hpp"

extern const std::uint8_t imageStart[] asm("_binary_c6_image_bin_start");
extern const std::uint8_t imageEnd[] asm("_binary_c6_image_bin_end");

extern "C" void app_main() {
    ESP_LOGI("c6_migration", "Authorized C6 OTA migration via P4; credentials and eFuses unchanged");
    const auto events = esp_event_loop_create_default();
    if (events != ESP_OK && events != ESP_ERR_INVALID_STATE) return;
    const auto initialized = esp_hosted_init();
    ESP_LOGI("c6_migration", "init=%d", initialized);
    if (initialized != 0) return;
    const auto connected = esp_hosted_connect_to_slave();
    ESP_LOGI("c6_migration", "connect=%d", connected);
    if (connected == 0) {
        wifi_mode_t mode{};
        const auto result = esp_wifi_get_mode(&mode);
        ESP_LOGI("c6_migration", "GET_MODE result=%s mode=%d", esp_err_to_name(result), static_cast<int>(mode));
        if (result == ESP_OK || result == ESP_ERR_WIFI_NOT_INIT) {
            const c6_update::UpdateService updater;
            const auto updated = updater.apply(imageStart, imageEnd - imageStart);
            ESP_LOGI("c6_migration", "OTA result=%s peer_validated=%d activated=%d",
                     esp_err_to_name(updated.error), updated.imageValidatedByPeer, updated.activationRequested);
        } else {
            ESP_LOGE("c6_migration", "OTA refused: legacy RPC is not responding");
        }
    }
    esp_hosted_deinit();
}