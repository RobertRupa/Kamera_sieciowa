#pragma once

#include <cstdint>

namespace camera_hub {

struct VideoProfile {
    std::uint16_t width{0};
    std::uint16_t height{0};
    std::uint8_t fps{0};
    std::uint8_t jpegQuality{0};

    [[nodiscard]] bool operator==(const VideoProfile& other) const noexcept {
        return width == other.width && height == other.height &&
               fps == other.fps && jpegQuality == other.jpegQuality;
    }
};

enum class ProfileResult : std::uint8_t { Accepted, InvalidArgument, Unverified, Unsupported, Busy };

}