#pragma once

#include <array>
#include <cstdint>
#include <mutex>

#include "camera_hub/IWifiScanner.hpp"

namespace app_services {

enum class NetworkScanState : std::uint8_t { Idle, Scanning, Complete, Failed };
enum class NetworkScanStartResult : std::uint8_t { Accepted, NotReady, Busy, DriverFailure };

struct NetworkScanSnapshot {
    std::uint32_t operationId{0};
    NetworkScanState state{NetworkScanState::Idle};
    camera_hub::WifiError wifiError{camera_hub::WifiError::None};
    std::size_t resultCount{0};
    std::array<camera_hub::WifiScanResult, camera_hub::kMaxWifiScanResults> results{};
};

class NetworkScanService final {
public:
    explicit NetworkScanService(camera_hub::IWifiScanner& scanner) noexcept;
    [[nodiscard]] NetworkScanStartResult start(std::uint32_t& operationId) noexcept;
    void processPending(bool timedOut = false) noexcept;
    [[nodiscard]] bool isScanning() const noexcept;
    [[nodiscard]] std::uint32_t operationId() const noexcept;
    [[nodiscard]] NetworkScanSnapshot snapshot() const noexcept;

private:
    camera_hub::IWifiScanner& scanner_;
    mutable std::mutex mutex_;
    NetworkScanSnapshot snapshot_{};
};

} // namespace app_services