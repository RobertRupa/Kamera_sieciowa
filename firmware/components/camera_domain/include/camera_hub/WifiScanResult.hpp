#pragma once

#include <array>
#include <cstdint>

namespace camera_hub {

enum class WifiSecurity : std::uint8_t {
    Open,
    Wep,
    WpaPsk,
    Wpa2Psk,
    WpaWpa2Psk,
    Enterprise,
    Wpa3Psk,
    Wpa2Wpa3Psk,
    Owe,
    Unknown
};

struct WifiScanResult {
    std::array<char, 33> ssid{};
    std::int8_t rssi{0};
    WifiSecurity security{WifiSecurity::Unknown};
};

} // namespace camera_hub