#pragma once

#include <string_view>
#include <array>
#include <mutex>

#include "camera_hub/ProvisioningPolicy.hpp"
#include "camera_hub/IWifiStation.hpp"
#include "camera_hub/IWifiScanner.hpp"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

namespace connectivity {

class WifiService final : public camera_hub::IWifiStation, public camera_hub::IWifiScanner {
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
    [[nodiscard]] std::uint32_t stationAddress() const noexcept override;
    [[nodiscard]] camera_hub::WifiError startScan() noexcept override;
    [[nodiscard]] bool scanComplete() const noexcept override;
    void cancelScan() noexcept override;
    [[nodiscard]] camera_hub::WifiError collectScanResults(camera_hub::WifiScanResult* results,
                                                            std::size_t capacity,
                                                            std::size_t& count) noexcept override;
    [[nodiscard]] esp_err_t stopAccessPoint() noexcept;
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
    mutable std::mutex scanMutex_;
    bool scanActive_{false};
    bool apActive_{false};
    bool hostedInitialized_{false};
    bool initialized_{false};
};

}