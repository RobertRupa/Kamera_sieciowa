#pragma once

#include <cstdint>

namespace camera_hub {

enum class CameraProbeState : std::uint8_t {
    NotRun,
    NoSupportedSensor,
    SensorIdentified,
    FrameCaptured,
    CaptureFailed,
    Error,
};

enum class CameraSensorModel : std::uint8_t {
    Unknown,
    Ov5647,
};

struct CameraProbeResult {
    CameraProbeState state{CameraProbeState::NotRun};
    CameraSensorModel sensor{CameraSensorModel::Unknown};
    std::uint16_t width{0};
    std::uint16_t height{0};
    std::uint32_t frameBytes{0};
};

[[nodiscard]] const char* cameraProbeStateKey(CameraProbeState state) noexcept;
[[nodiscard]] const char* cameraSensorName(CameraSensorModel sensor) noexcept;

} // namespace camera_hub