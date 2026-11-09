#include "camera_hub/ProvisioningPolicy.hpp"

namespace camera_hub {

bool ProvisioningPolicy::canStartAccessPoint(const ProvisioningPrerequisites& state) noexcept {
    return state.boardWiringVerified && state.localOpenApAuthorized;
}

bool ProvisioningPolicy::canAcceptCredentials(const ProvisioningPrerequisites& state) noexcept {
    return canStartAccessPoint(state) && state.credentialStoreReady && state.companionStorageAudited;
}

bool ProvisioningPolicy::validSsid(std::string_view ssid) noexcept {
    return !ssid.empty() && ssid.size() <= 32 && ssid.find('\0') == std::string_view::npos;
}

bool ProvisioningPolicy::validPassphrase(std::string_view password) noexcept {
    if (password.size() < 8 || password.size() > 63) {
        return false;
    }
    for (const auto character : password) {
        if (static_cast<unsigned char>(character) < 32 || static_cast<unsigned char>(character) > 126) {
            return false;
        }
    }
    return true;
}

bool ProvisioningPolicy::validHostname(std::string_view hostname) noexcept {
    if (hostname.empty() || hostname.size() > 63 || hostname.front() == '-' || hostname.back() == '-') {
        return false;
    }
    for (const auto character : hostname) {
        if (!(character >= 'a' && character <= 'z') && !(character >= '0' && character <= '9') && character != '-') {
            return false;
        }
    }
    return true;
}

}