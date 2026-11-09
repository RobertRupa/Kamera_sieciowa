#pragma once

#include "camera_hub/WifiCredentials.hpp"

namespace camera_hub {

class ICredentialStore {
public:
    virtual ~ICredentialStore() = default;
    [[nodiscard]] virtual bool ready() const noexcept = 0;
    [[nodiscard]] virtual CredentialError save(const WifiCredentials& credentials) noexcept = 0;
    [[nodiscard]] virtual CredentialError load(WifiCredentials& credentials) noexcept = 0;
    [[nodiscard]] virtual CredentialError clear() noexcept = 0;
};

}