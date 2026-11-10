#pragma once

#include <string_view>

namespace camera_hub {

// mDNS hostname (without ".local"). Underscore is deliberate to preserve
// the requested device URL, although conventional DNS hostnames use hyphens.
inline constexpr std::string_view kCameraMdnsHostname = "camera_hub";
inline constexpr std::string_view kCameraMdnsFallbackHostname = "camera-hub";

// Do not allow arbitrary .local origins, wildcards or additional ports.
// ProvisioningApi also verifies the local destination socket IP before
// checking Origin; this only permits known same-device browser origins.
inline bool allowedDeviceHttpOrigin(std::string_view origin,
                                    std::string_view localIpv4) noexcept {
    constexpr std::string_view prefix = "http://";
    if (origin == "http://camera_hub.local" ||
        origin == "http://camera-hub.local") return true;
    return origin.size() == prefix.size() + localIpv4.size() &&
        origin.substr(0, prefix.size()) == prefix &&
        origin.substr(prefix.size()) == localIpv4;
}

} // namespace camera_hub
