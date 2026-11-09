#pragma once

#include <cstddef>

#include "camera_hub/IWifiStation.hpp"
#include "camera_hub/WifiScanResult.hpp"

namespace camera_hub {

inline constexpr std::size_t kMaxWifiScanResults = 20;

class IWifiScanner {
public:
    virtual ~IWifiScanner() = default;
    [[nodiscard]] virtual WifiError startScan() noexcept = 0;
    [[nodiscard]] virtual bool scanComplete() const noexcept = 0;
    virtual void cancelScan() noexcept = 0;
    [[nodiscard]] virtual WifiError collectScanResults(WifiScanResult* results,
                                                       std::size_t capacity,
                                                       std::size_t& count) noexcept = 0;
};

} // namespace camera_hub