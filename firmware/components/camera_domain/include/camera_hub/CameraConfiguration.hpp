#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include "camera_hub/VideoCodec.hpp"

namespace camera_hub {

inline constexpr std::size_t kMaxCaptureFrameBytes = 5 * 1024 * 1024;

struct SensorMode {
    std::uint32_t id{0};
    std::array<char, 80> name{};
    std::uint16_t width{0};
    std::uint16_t height{0};
    std::uint16_t sensorFps{0};
};

struct CameraSettings {
    std::uint32_t modeId{0};
    std::uint8_t previewFps{20};
    std::uint8_t jpegQuality{80};
    VideoCodec codec{VideoCodec::Jpeg};
    std::uint32_t h264Bitrate{2000000};
};

enum class ConfigurationState : std::uint8_t { Idle, Queued, Applying, Applied, Failed };

struct CameraConfiguration {
    std::array<SensorMode, 32> modes{};
    std::size_t modeCount{0};
    CameraSettings settings{};
    ConfigurationState state{ConfigurationState::Idle};
};

constexpr bool captureModeFits(const SensorMode& mode) noexcept {
    return mode.width != 0 && mode.height != 0 && mode.sensorFps != 0 &&
           mode.width <= 1920 && mode.height <= 1920 &&
           static_cast<std::size_t>(mode.width) * mode.height * 2 <= kMaxCaptureFrameBytes;
}

inline bool validCameraSettings(const CameraConfiguration& configuration,
                                const CameraSettings& settings) noexcept {
    if (settings.previewFps == 0 || settings.previewFps > 20 ||
        settings.jpegQuality == 0 || settings.jpegQuality > 100 ||
        configuration.modeCount > configuration.modes.size()) return false;
    if ((settings.codec != VideoCodec::Jpeg && settings.codec != VideoCodec::H264) ||
        settings.h264Bitrate < 64000 || settings.h264Bitrate > 10000000) return false;
    for (std::size_t index = 0; index < configuration.modeCount; ++index) {
        const auto& mode = configuration.modes[index];
        if (mode.id == settings.modeId)
                return captureModeFits(mode) && settings.previewFps <= mode.sensorFps &&
                   (settings.codec != VideoCodec::H264 ||
                    (mode.width >= 80 && mode.height >= 80 && mode.width <= 1920 && mode.height <= 2032));
    }
    return false;
}

} // namespace camera_hub