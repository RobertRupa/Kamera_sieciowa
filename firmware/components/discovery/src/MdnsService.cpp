#include "discovery/MdnsService.hpp"

#include <array>
#include <cstring>

#include "camera_hub/ProvisioningPolicy.hpp"
#include "mdns.h"

namespace discovery {

MdnsService::~MdnsService() { stop(); }

esp_err_t MdnsService::start(std::string_view hostname, bool ipReady,
                            std::uint16_t activeHttpPort) noexcept {
    if (!ipReady || started_) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!camera_hub::ProvisioningPolicy::validHostname(hostname)) {
        return ESP_ERR_INVALID_ARG;
    }
    std::array<char, 64> name{};
    std::memcpy(name.data(), hostname.data(), hostname.size());
    auto result = mdns_init();
    if (result != ESP_OK) {
        return result;
    }
    started_ = true;
    result = mdns_hostname_set(name.data());
    if (result == ESP_OK && activeHttpPort != 0) {
        result = mdns_service_add("camera_hub", "_http", "_tcp", activeHttpPort, nullptr, 0);
    }
    if (result != ESP_OK) {
        stop();
    }
    return result;
}

void MdnsService::stop() noexcept {
    if (started_) {
        mdns_free();
        started_ = false;
    }
}

}