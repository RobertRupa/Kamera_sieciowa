#include "app_services/NetworkScanService.hpp"

namespace app_services {

NetworkScanService::NetworkScanService(camera_hub::IWifiScanner& scanner) noexcept
    : scanner_(scanner) {}

NetworkScanStartResult NetworkScanService::start(std::uint32_t& operationId) noexcept {
    operationId = 0;
    const std::lock_guard<std::mutex> lock(mutex_);
    if (snapshot_.state == NetworkScanState::Scanning) return NetworkScanStartResult::Busy;

    const auto started = scanner_.startScan();
    if (started == camera_hub::WifiError::NotReady) return NetworkScanStartResult::NotReady;
    if (started != camera_hub::WifiError::None) return NetworkScanStartResult::DriverFailure;

    ++snapshot_.operationId;
    if (snapshot_.operationId == 0) ++snapshot_.operationId;
    snapshot_.state = NetworkScanState::Scanning;
    snapshot_.wifiError = camera_hub::WifiError::None;
    snapshot_.resultCount = 0;
    snapshot_.results.fill({});
    operationId = snapshot_.operationId;
    return NetworkScanStartResult::Accepted;
}

void NetworkScanService::processPending(bool timedOut) noexcept {
    std::uint32_t operationId = 0;
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        if (snapshot_.state != NetworkScanState::Scanning) return;
        operationId = snapshot_.operationId;
    }
    if (!scanner_.scanComplete()) {
        if (!timedOut) return;
        scanner_.cancelScan();
    }

    const std::lock_guard<std::mutex> lock(mutex_);
    if (snapshot_.state != NetworkScanState::Scanning || snapshot_.operationId != operationId) return;
    std::size_t resultCount = 0;
    auto scanError = scanner_.scanComplete()
        ? scanner_.collectScanResults(snapshot_.results.data(), snapshot_.results.size(), resultCount)
        : camera_hub::WifiError::DriverFailure;
    if (resultCount > snapshot_.results.size()) {
        resultCount = 0;
        scanError = camera_hub::WifiError::DriverFailure;
    }

    snapshot_.wifiError = scanError;
    if (scanError == camera_hub::WifiError::None) {
        snapshot_.resultCount = resultCount;
        snapshot_.state = NetworkScanState::Complete;
    } else {
        snapshot_.resultCount = 0;
        snapshot_.results.fill({});
        snapshot_.state = NetworkScanState::Failed;
    }
}

bool NetworkScanService::isScanning() const noexcept {
    const std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_.state == NetworkScanState::Scanning;
}

std::uint32_t NetworkScanService::operationId() const noexcept {
    const std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_.operationId;
}

NetworkScanSnapshot NetworkScanService::snapshot() const noexcept {
    const std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_;
}

} // namespace app_services