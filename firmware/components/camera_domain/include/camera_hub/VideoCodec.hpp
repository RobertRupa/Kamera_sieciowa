#pragma once

#include <cstdint>

namespace camera_hub {

enum class VideoCodec : std::uint8_t { Jpeg, H264 };

constexpr const char* videoCodecKey(VideoCodec codec) noexcept {
    return codec == VideoCodec::H264 ? "h264" : "jpeg";
}

} // namespace camera_hub