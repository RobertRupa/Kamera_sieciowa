#pragma once

#include <cstddef>
#include <cstdint>
#include "esp_err.h"

namespace c6_update {

struct UpdateResult {
    esp_err_t error{ESP_OK};
    bool imageValidatedByPeer{false};
    bool activationRequested{false};
};

class UpdateService final {
public:
    [[nodiscard]] UpdateResult apply(const std::uint8_t* image, std::size_t length) const noexcept;
};

}