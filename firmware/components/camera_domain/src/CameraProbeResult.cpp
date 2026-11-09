#include "camera_hub/CameraProbeResult.hpp"

namespace camera_hub {

const char* cameraProbeStateKey(CameraProbeState state) noexcept {
    switch (state) {
    case CameraProbeState::NotRun: return "not_run";
    case CameraProbeState::NoSupportedSensor: return "no_supported_sensor";
    case CameraProbeState::SensorIdentified: return "sensor_identified";
    case CameraProbeState::FrameCaptured: return "frame_captured";
    case CameraProbeState::CaptureFailed: return "capture_failed";
    case CameraProbeState::Error: return "error";
    }
    return "error";
}

const char* cameraSensorName(CameraSensorModel sensor) noexcept {
    switch (sensor) {
    case CameraSensorModel::Ov5647: return "OV5647";
    case CameraSensorModel::Unknown: break;
    }
    return nullptr;
}

} // namespace camera_hub