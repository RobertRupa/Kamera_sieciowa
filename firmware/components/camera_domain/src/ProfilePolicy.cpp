#include "camera_hub/ProfilePolicy.hpp"

namespace camera_hub {

ProfilePolicy::ProfilePolicy(const VideoProfile* supported, std::size_t count) noexcept
    : supported_(supported), count_(supported != nullptr && count <= 32 ? count : 0) {}

ProfileResult ProfilePolicy::check(const VideoProfile& requested,
                                  const VideoProfile& current,
                                  bool usbStreaming) const noexcept {
    if (requested.width == 0 || requested.height == 0 || requested.fps == 0 ||
        requested.fps > 60 || requested.jpegQuality == 0 || requested.jpegQuality > 100) {
        return ProfileResult::InvalidArgument;
    }
    if (count_ == 0) {
        return ProfileResult::Unverified;
    }
    bool supported = false;
    for (std::size_t index = 0; index < count_; ++index) {
        const auto& profile = supported_[index];
        supported |= requested.width == profile.width && requested.height == profile.height &&
                     requested.fps == profile.fps;
    }
    if (!supported) {
        return ProfileResult::Unsupported;
    }
    if (usbStreaming && !(requested == current)) {
        return ProfileResult::Busy;
    }
    return ProfileResult::Accepted;
}

ProfileResult ProfilePolicy::negotiateUsb(std::uint16_t width, std::uint16_t height,
                                         std::uint8_t fps,
                                         const VideoProfile& current) const noexcept {
    const VideoProfile requested{width, height, fps, current.jpegQuality};
    return check(requested, current, true);
}

}