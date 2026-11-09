#pragma once

#include <array>
#include <cstdint>
#include "esp_err.h"

namespace connectivity {

struct CompanionInfo {
    std::uint32_t chipId{0};
    std::array<char, 32> target{};
};

class HostedProbe final {
public:
    [[nodiscard]] esp_err_t run(bool pinoutChecked, CompanionInfo& info) const noexcept;
};

}