#pragma once

#include <string_view>

namespace camera_hub {

struct ProvisioningPrerequisites {
    bool boardWiringVerified{false};
    bool credentialStoreReady{false};
    bool companionStorageAudited{false};
    bool localOpenApAuthorized{false};
};

class ProvisioningPolicy final {
public:
    [[nodiscard]] static bool canStartAccessPoint(const ProvisioningPrerequisites& state) noexcept;
    [[nodiscard]] static bool canAcceptCredentials(const ProvisioningPrerequisites& state) noexcept;
    [[nodiscard]] static bool validSsid(std::string_view ssid) noexcept;
    [[nodiscard]] static bool validPassphrase(std::string_view password) noexcept;
    [[nodiscard]] static bool validHostname(std::string_view hostname) noexcept;
};

}