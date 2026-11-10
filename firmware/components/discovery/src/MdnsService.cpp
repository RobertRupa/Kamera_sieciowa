#include "discovery/MdnsService.hpp"

#include <array>
#include <cstring>

#include "camera_hub/ProvisioningPolicy.hpp"
#include "camera_hub/DiscoveryNames.hpp"
#include "mdns.h"
#include "esp_log.h"

namespace discovery {
namespace {
constexpr char kTag[] = "mdns_service";
}

MdnsService::~MdnsService() { stop(); }

esp_err_t MdnsService::start(std::string_view hostname, bool ipReady,
                            std::uint16_t activeHttpPort) noexcept {
    if (!ipReady || started_) {
        return ESP_ERR_INVALID_STATE;
    }
    // Only camera_hub uses a non-LDH mDNS label. Do not relax general
    // DHCP/DNS hostname validation, which intentionally rejects underscores.
    if (hostname != camera_hub::kCameraMdnsHostname &&
        !camera_hub::ProvisioningPolicy::validHostname(hostname)) {
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
    if (result == ESP_OK) result = mdns_instance_name_set("Camera Hub");
    if (result == ESP_OK && activeHttpPort != 0) {
        result = mdns_service_add("Camera Hub", "_http", "_tcp", activeHttpPort, nullptr, 0);
    }
    if (result != ESP_OK) {
        ESP_LOGW(kTag, "mDNS setup failed for %s.local: %s",
                 name.data(), esp_err_to_name(result));
        stop();
    } else {
        ESP_LOGI(kTag, "mDNS responder initialized as %s.local, HTTP _http._tcp port %u",
                 name.data(), static_cast<unsigned int>(activeHttpPort));
    }
    return result;
}

esp_err_t MdnsService::updateFallbackAddress(std::uint32_t ipv4) noexcept {
    if (!started_) return ESP_ERR_INVALID_STATE;
    if (ipv4 == 0) return ESP_ERR_INVALID_ARG;
    if (fallbackRegistered_ && fallbackIpv4_ == ipv4) return ESP_OK;
    mdns_ip_addr_t address{};
    address.addr.type = ESP_IPADDR_TYPE_V4;
    address.addr.u_addr.ip4.addr = ipv4;
    address.next = nullptr;
    const auto name = camera_hub::kCameraMdnsFallbackHostname;
    std::array<char, 64> alias{};
    std::memcpy(alias.data(), name.data(), name.size());
    const auto result = fallbackRegistered_
        ? mdns_delegate_hostname_set_address(alias.data(), &address)
        : mdns_delegate_hostname_add(alias.data(), &address);
    if (result == ESP_OK) {
        fallbackRegistered_ = true;
        fallbackIpv4_ = ipv4;
        ESP_LOGI(kTag, "mDNS compatibility alias %s.local updated", alias.data());
    } else {
        ESP_LOGW(kTag, "mDNS compatibility alias update failed: %s", esp_err_to_name(result));
    }
    return result;
}

void MdnsService::stop() noexcept {
    if (started_) {
        mdns_free();
        started_ = false;
        fallbackRegistered_ = false;
        fallbackIpv4_ = 0;
    }
}

}