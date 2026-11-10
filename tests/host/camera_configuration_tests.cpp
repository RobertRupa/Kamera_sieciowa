#include "camera_hub/CameraConfiguration.hpp"

#include <cassert>
#include <cstdint>

using namespace camera_hub;

int main() {
    CameraConfiguration config{};
    config.modeCount = 5;
    const auto modes = std::array<SensorMode, 5>{{
        {0, {}, 800, 1280, 50},
        {1, {}, 800, 640, 50},
        {2, {}, 800, 800, 50},
        {3, {}, 1920, 1080, 30},
        {4, {}, 1280, 960, 45},
    }};
    for (std::size_t i = 0; i < modes.size(); ++i) config.modes[i] = modes[i];

    CameraSettings settings{};
    settings.modeId = 2;
    settings.previewFps = 50;
    assert(validCameraSettings(config, settings));
    settings.previewFps = 20;
    assert(!validCameraSettings(config, settings)); // no manual 20 FPS cap
    settings.modeId = 4;
    settings.previewFps = 45;
    assert(validCameraSettings(config, settings));
    settings.modeId = 3;
    settings.previewFps = 30;
    assert(validCameraSettings(config, settings));
    settings.modeId = 0;
    settings.previewFps = 50;
    assert(validCameraSettings(config, settings));
    settings.modeId = 19;
    assert(!validCameraSettings(config, settings));

    settings.modeId = 2;
    settings.previewFps = 50;
    settings.codec = VideoCodec::H264;
    assert(validCameraSettings(config, settings));
    settings.modeId = 3;
    settings.previewFps = 30;
    assert(!validCameraSettings(config, settings)); // 1080 is not multiple of 16
    return 0;
}
