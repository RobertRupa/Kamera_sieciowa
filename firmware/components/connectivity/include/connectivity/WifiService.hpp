#pragma once

#include <string_view>
#include <array>

#include "camera_hub/ProvisioningPolicy.hpp"
#include "camera_hub/IWifiStation.hpp"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

namespace connectivity {

class WifiService final : public camera_hub::IWifiStation {
public:
    WifiService() noexcept = default;
    ~WifiService();
    WifiService(const WifiService&) = delete;
    WifiService& operator=(const WifiService&) = delete;
    [[nodiscard]] esp_err_t startProvisioning(const camera_hub::ProvisioningPrerequisites& prerequisites,
                                              std::string_view ssid) noexcept;
    [[nodiscard]] bool waitForAp(TickType_t timeout) const noexcept;
    [[nodiscard]] camera_hub::WifiError connectStation(const camera_hub::WifiCredentials& credentials) noexcept override;
    [[nodiscard]] bool stationHasIp() const noexcept override;
    [[nodiscard]] std::uint32_t apAddress() const noexcept;
    void stop() noexcept;

private:
    static void onWifiEvent(void* context, esp_event_base_t base, int32_t event, void* data);
    static void onIpEvent(void* context, esp_event_base_t base, int32_t event, void* data);
    esp_netif_t* ap_{nullptr};
    esp_netif_t* sta_{nullptr};
    EventGroupHandle_t events_{nullptr};
    esp_event_handler_instance_t handler_{nullptr};
    esp_event_handler_instance_t ipHandler_{nullptr};
    std::array<char, 33> requestedSsid_{};
    bool hostedInitialized_{false};
    bool initialized_{false};
};

}