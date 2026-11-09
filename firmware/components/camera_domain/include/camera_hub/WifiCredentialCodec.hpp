#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "camera_hub/WifiCredentials.hpp"

namespace camera_hub {

using CredentialRecord = std::array<std::uint8_t, 98>;

class WifiCredentialCodec final {
public:
    [[nodiscard]] static CredentialError encode(const WifiCredentials& credentials,
                                                CredentialRecord& record) noexcept;
    [[nodiscard]] static CredentialError decode(const CredentialRecord& record,
                                                WifiCredentials& credentials) noexcept;
};

}