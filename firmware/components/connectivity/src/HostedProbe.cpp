#include "connectivity/HostedProbe.hpp"
#include "esp_hosted.h"
#include "esp_log.h"

namespace connectivity {

esp_err_t HostedProbe::run(bool pinoutChecked, CompanionInfo& info) const noexcept {
    info = {};
    if (!pinoutChecked) return ESP_ERR_INVALID_STATE;
    esp_log_level_set("eh_init_evt", ESP_LOG_INFO);
    const auto initialized = esp_hosted_init();
    ESP_LOGI("camera_hub", "C6_PROBE init=%d", initialized);
    if (initialized != 0) return ESP_FAIL;
    const auto connection = esp_hosted_connect_to_slave();
    ESP_LOGI("camera_hub", "C6_PROBE connect=%d", connection);
    const auto identity = connection == 0
        ? esp_hosted_get_cp_info(&info.chipId, info.target.data(), info.target.size()) : ESP_ERR_INVALID_STATE;
    ESP_LOGI("camera_hub", "C6_PROBE identity=%d", identity);
    const bool identified = connection == 0 && identity == 0;
    const auto stopped = esp_hosted_deinit();
    return identified && stopped == 0 ? ESP_OK : ESP_FAIL;
}

}