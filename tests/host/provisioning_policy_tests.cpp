#include "camera_hub/ProvisioningPolicy.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

using namespace camera_hub;

void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::abort();
    }
}

int main() {
    for (unsigned int mask = 0; mask < 16; ++mask) {
        const ProvisioningPrerequisites state{
            (mask & 1) != 0, (mask & 2) != 0, (mask & 4) != 0, (mask & 8) != 0};
    require(ProvisioningPolicy::canStartAccessPoint(state) == ((mask & 9) == 9),
        "AP requires verified wiring and local authorization");
    require(ProvisioningPolicy::canAcceptCredentials(state) == (mask == 15),
        "credential provisioning requires store and companion audit");
    }
    require(ProvisioningPolicy::validSsid("hidden-network"), "valid SSID");
    require(ProvisioningPolicy::validSsid(std::string(32, 'a')), "32-byte SSID");
    require(!ProvisioningPolicy::validSsid(""), "empty SSID");
    require(!ProvisioningPolicy::validSsid(std::string(33, 'a')), "long SSID");
    require(!ProvisioningPolicy::validSsid(std::string_view("a\0b", 3)), "embedded null SSID");
    require(ProvisioningPolicy::validPassphrase(std::string(8, 'a')), "minimum password");
    require(ProvisioningPolicy::validPassphrase(std::string(63, 'a')), "maximum password");
    require(!ProvisioningPolicy::validPassphrase(std::string(7, 'a')), "short password");
    require(!ProvisioningPolicy::validPassphrase(std::string(64, 'a')), "long password");
    require(!ProvisioningPolicy::validPassphrase("abcdefgh\n"), "control byte password");
    require(ProvisioningPolicy::validHostname("camera-hub-123"), "valid mDNS name");
    require(ProvisioningPolicy::validHostname(std::string(63, 'a')), "maximum mDNS label");
    for (const auto hostname : {"", "Camera", "-camera", "camera-", "camera.local", "cam era"}) {
        require(!ProvisioningPolicy::validHostname(hostname), "invalid mDNS label");
    }
    std::puts("PASS: provisioning prerequisites, credential bounds and mDNS labels");
}