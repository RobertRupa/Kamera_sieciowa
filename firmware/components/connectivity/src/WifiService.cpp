#include "connectivity/WifiService.hpp"

#include <cstring>
#include <algorithm>

#include "esp_wifi.h"
#include "esp_hosted.h"
#include "esp_log.h"
#include "camera_hub/WifiCredentialCodec.hpp"
#include "mbedtls/platform_util.h"

namespace connectivity {
namespace {
constexpr char kTag[] = "wifi_service";
}

WifiService::~WifiService() { stop(); }

void WifiService::onWifiEvent(void* context, esp_event_base_t, int32_t event, void*) {
    auto& service = *static_cast<WifiService*>(context);
    if (event == WIFI_EVENT_AP_START) {
        xEventGroupSetBits(service.events_, BIT0);
    } else if (event == WIFI_EVENT_AP_STOP) {
        xEventGroupClearBits(service.events_, BIT0);
    } else if (event == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(service.events_, BIT1);
    }
}

void WifiService::onIpEvent(void* context, esp_event_base_t, int32_t event, void* data) {
    auto& service = *static_cast<WifiService*>(context);
    if (event == IP_EVENT_STA_GOT_IP && data != nullptr &&
        static_cast<ip_event_got_ip_t*>(data)->esp_netif == service.sta_) {
        xEventGroupSetBits(service.events_, BIT1);
    } else if (event == IP_EVENT_STA_LOST_IP) {
        xEventGroupClearBits(service.events_, BIT1);
    }
}

esp_err_t WifiService::startProvisioning(const camera_hub::ProvisioningPrerequisites& prerequisites,
                                        std::string_view ssid) noexcept {
    if (!camera_hub::ProvisioningPolicy::canStartAccessPoint(prerequisites) || initialized_) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!camera_hub::ProvisioningPolicy::validSsid(ssid)) {
        return ESP_ERR_INVALID_ARG;
    }
    auto result = esp_netif_init();
    if (result != ESP_OK) {
        return result;
    }
    result = esp_event_loop_create_default();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
        return result;
    }
    ap_ = esp_netif_create_default_wifi_ap();
    sta_ = esp_netif_create_default_wifi_sta();
    events_ = xEventGroupCreate();
    if (ap_ == nullptr || sta_ == nullptr || events_ == nullptr) {
        stop();
        return ESP_ERR_NO_MEM;
    }
    auto hostedResult = esp_hosted_init();
    if (hostedResult != 0) {
        ESP_LOGE(kTag, "ESP-Hosted initialization failed: %d", hostedResult);
        stop();
        return ESP_FAIL;
    }
    hostedInitialized_ = true;
    hostedResult = esp_hosted_connect_to_slave();
    if (hostedResult != 0) {
        ESP_LOGE(kTag, "ESP-Hosted connection failed: %d", hostedResult);
        stop();
        return ESP_FAIL;
    }
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    config.nvs_enable = 0;
    result = esp_wifi_init(&config);
    if (result != ESP_OK) {
        stop();
        return result;
    }
    initialized_ = true;
    result = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                 &WifiService::onWifiEvent, this, &handler_);
    if (result == ESP_OK) {
        result = esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID,
                                                     &WifiService::onIpEvent, this, &ipHandler_);
    }
    if (result == ESP_OK) {
        result = esp_wifi_set_mode(WIFI_MODE_APSTA);
    }
    wifi_config_t apConfig{};
    std::memcpy(apConfig.ap.ssid, ssid.data(), ssid.size());
    apConfig.ap.ssid_len = ssid.size();
    apConfig.ap.authmode = WIFI_AUTH_OPEN;
    apConfig.ap.channel = 1;
    apConfig.ap.max_connection = 2;
    if (result == ESP_OK) {
        result = esp_wifi_set_config(WIFI_IF_AP, &apConfig);
    }
    if (result == ESP_OK) {
        result = esp_wifi_start();
    }
    if (result != ESP_OK) {
        stop();
    }
    return result;
}

bool WifiService::waitForAp(TickType_t timeout) const noexcept {
    return events_ != nullptr && (xEventGroupWaitBits(events_, BIT0, pdFALSE, pdTRUE, timeout) & BIT0) != 0;
}

camera_hub::WifiError WifiService::connectStation(const camera_hub::WifiCredentials& credentials) noexcept {
    using namespace camera_hub;
    if (!initialized_) return WifiError::NotReady;
    CredentialRecord record{};
    const auto validated = WifiCredentialCodec::encode(credentials, record);
    mbedtls_platform_zeroize(record.data(), record.size());
    if (validated != CredentialError::None) return WifiError::InvalidArgument;
    xEventGroupClearBits(events_, BIT1);
    auto result = esp_wifi_disconnect();
    if (result != ESP_OK && result != ESP_ERR_WIFI_NOT_CONNECT) return WifiError::DriverFailure;
    wifi_config_t config{};
    std::memcpy(config.sta.ssid, credentials.ssid.data(), 32);
    std::memcpy(config.sta.password, credentials.passphrase.data(), credentials.passphrase.size());
    config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    requestedSsid_ = credentials.ssid;
    result = esp_wifi_set_config(WIFI_IF_STA, &config);
    mbedtls_platform_zeroize(&config, sizeof(config));
    if (result == ESP_OK) result = esp_wifi_connect();
    return result == ESP_OK ? WifiError::None : WifiError::DriverFailure;
}

bool WifiService::stationHasIp() const noexcept {
    if (!initialized_ || events_ == nullptr || (xEventGroupGetBits(events_) & BIT1) == 0) return false;
    wifi_ap_record_t actual{};
    if (esp_wifi_sta_get_ap_info(&actual) != ESP_OK) return false;
    const auto length = std::find(requestedSsid_.begin(), requestedSsid_.end(), '\0') - requestedSsid_.begin();
    return length != 0 && std::memcmp(actual.ssid, requestedSsid_.data(), length) == 0 &&
           (length == 32 || actual.ssid[length] == 0);
}

std::uint32_t WifiService::apAddress() const noexcept {
    esp_netif_ip_info_t info{};
    return ap_ != nullptr && esp_netif_get_ip_info(ap_, &info) == ESP_OK ? info.ip.addr : 0;
}

void WifiService::stop() noexcept {
    if (initialized_) {
        esp_wifi_stop();
    }
    if (handler_ != nullptr) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, handler_);
        handler_ = nullptr;
    }
    if (ipHandler_ != nullptr) {
        esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, ipHandler_);
        ipHandler_ = nullptr;
    }
    if (initialized_) {
        esp_wifi_deinit();
        initialized_ = false;
    }
    if (ap_ != nullptr) {
        esp_netif_destroy_default_wifi(ap_);
        ap_ = nullptr;
    }
    if (sta_ != nullptr) {
        esp_netif_destroy_default_wifi(sta_);
        sta_ = nullptr;
    }
    if (events_ != nullptr) {
        vEventGroupDelete(events_);
        events_ = nullptr;
    }
    if (hostedInitialized_) {
        const auto result = esp_hosted_deinit();
        if (result != 0) ESP_LOGW(kTag, "ESP-Hosted deinitialization failed: %d", result);
        hostedInitialized_ = false;
    }
    requestedSsid_.fill(0);
}

}