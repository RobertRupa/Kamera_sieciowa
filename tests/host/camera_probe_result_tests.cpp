#include <cassert>
#include <cstring>

#include "camera_hub/CameraProbeResult.hpp"

int main() {
    using namespace camera_hub;

    assert(std::strcmp(cameraProbeStateKey(CameraProbeState::NotRun), "not_run") == 0);
    assert(std::strcmp(cameraProbeStateKey(CameraProbeState::NoSupportedSensor), "no_supported_sensor") == 0);
    assert(std::strcmp(cameraProbeStateKey(CameraProbeState::SensorIdentified), "sensor_identified") == 0);
    assert(std::strcmp(cameraProbeStateKey(CameraProbeState::FrameCaptured), "frame_captured") == 0);
    assert(std::strcmp(cameraProbeStateKey(CameraProbeState::CaptureFailed), "capture_failed") == 0);
    assert(std::strcmp(cameraProbeStateKey(CameraProbeState::Error), "error") == 0);
    assert(std::strcmp(cameraSensorName(CameraSensorModel::Ov5647), "OV5647") == 0);
    assert(cameraSensorName(CameraSensorModel::Unknown) == nullptr);
}