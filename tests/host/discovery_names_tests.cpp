#include "camera_hub/DiscoveryNames.hpp"
#include "camera_hub/ProvisioningPolicy.hpp"

#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace {
void require(bool truth, const char* message) {
    if (!truth) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::abort();
    }
}
}

int main() {
    using camera_hub::allowedDeviceHttpOrigin;
    require(camera_hub::kCameraMdnsHostname == "camera_hub", "canonical hostname");
    require(camera_hub::kCameraMdnsFallbackHostname == "camera-hub", "compatible alias");
    // The deliberate mDNS-only underscore exception must not relax DHCP/DNS
    // hostname validation used elsewhere in the application.
    require(!camera_hub::ProvisioningPolicy::validHostname("camera_hub"),
            "nonstandard hostname rejected by generic validator");
    require(camera_hub::ProvisioningPolicy::validHostname("camera-hub"),
            "conventional alias valid for DHCP/DNS");

    require(allowedDeviceHttpOrigin("http://camera_hub.local", "192.168.4.1"),
            "canonical mDNS address allowed for AP");
    require(allowedDeviceHttpOrigin("http://camera_hub.local", "192.168.69.40"),
            "canonical mDNS address allowed for STA");
    require(allowedDeviceHttpOrigin("http://camera-hub.local", "192.168.69.40"),
            "legacy mDNS alias allowed");
    require(allowedDeviceHttpOrigin("http://192.168.69.40", "192.168.69.40"),
            "matching local IPv4 origin allowed");
    for (auto origin : {"http://other.local", "http://camera_hub.local.evil.example",
                        "http://camera_hub.local:8080", "https://camera_hub.local",
                        "http://192.168.69.41", "http://camera_hub.local/",
                        "http://CAMERA_HUB.local", "", "null"}) {
        require(!allowedDeviceHttpOrigin(origin, "192.168.69.40"),
                "untrusted/ambiguous origin rejected");
    }
    std::puts("PASS: camera_hub.local mDNS naming and strict local Origin policy");
}
