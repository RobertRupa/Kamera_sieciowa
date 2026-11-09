#pragma once

#include <cstddef>

#include "camera_hub/VideoProfile.hpp"

namespace camera_hub {

class ProfilePolicy final {
public:
    ProfilePolicy(const VideoProfile* supported, std::size_t count) noexcept;
    [[nodiscard]] ProfileResult check(const VideoProfile& requested,
                                      const VideoProfile& current,
                                      bool usbStreaming) const noexcept;
    [[nodiscard]] ProfileResult negotiateUsb(std::uint16_t width, std::uint16_t height,
                                             std::uint8_t fps,
                                             const VideoProfile& current) const noexcept;

private:
    const VideoProfile* supported_;
    std::size_t count_;
};

}