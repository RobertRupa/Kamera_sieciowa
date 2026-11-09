#pragma once

#include "camera_hub/WifiCredentials.hpp"

namespace camera_hub {

enum class WifiError : std::uint8_t { None, NotReady, InvalidArgument, DriverFailure };

class IWifiStation {
public:
    virtual ~IWifiStation() = default;
    [[nodiscard]] virtual WifiError connectStation(const WifiCredentials& credentials) noexcept = 0;
    [[nodiscard]] virtual bool stationHasIp() const noexcept = 0;
};

}