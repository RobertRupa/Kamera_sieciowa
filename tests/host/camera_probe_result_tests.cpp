#include <cassert>
#include <cstring>

#include "camera_hub/CameraProbeResult.hpp"
#include "camera_hub/CameraConfiguration.hpp"
#include "camera_hub/JpegHeaderView.hpp"

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

    CameraConfiguration configuration{};
    configuration.modeCount = 1;
    configuration.modes[0].id = 3;
    configuration.modes[0].width = 800;
    configuration.modes[0].height = 800;
    configuration.modes[0].sensorFps = 50;
    CameraSettings settings{3, 20, 80};
    assert(validCameraSettings(configuration, settings));
    settings.modeId = 4;
    assert(!validCameraSettings(configuration, settings));
    settings.modeId = 3;
    settings.previewFps = 21;
    assert(!validCameraSettings(configuration, settings));
    settings.previewFps = 0;
    assert(!validCameraSettings(configuration, settings));
    settings.previewFps = 20;
    settings.jpegQuality = 101;
    assert(!validCameraSettings(configuration, settings));
    settings.jpegQuality = 0;
    assert(!validCameraSettings(configuration, settings));
    settings.jpegQuality = 80;
    configuration.modes[0].width = 1920;
    configuration.modes[0].height = 1080;
    assert(validCameraSettings(configuration, settings));
    for (const auto dimensions : {std::array<std::uint16_t, 2>{800, 640}, {800, 800},
                                  {800, 1280}, {1920, 1080}, {1280, 960}}) {
        configuration.modes[0].width = dimensions[0];
        configuration.modes[0].height = dimensions[1];
        assert(validCameraSettings(configuration, settings));
    }
    configuration.modes[0].width = 1920;
    configuration.modes[0].height = 1920;
    assert(!validCameraSettings(configuration, settings));
    configuration.modes[0].width = 800;
    configuration.modes[0].height = 800;
    configuration.modes[0].sensorFps = 10;
    assert(!validCameraSettings(configuration, settings));
    configuration.modes[0].sensorFps = 50;
    settings.codec = VideoCodec::H264;
    assert(validCameraSettings(configuration, settings));
    settings.h264Bitrate = 63999;
    assert(!validCameraSettings(configuration, settings));
    settings.h264Bitrate = 10000001;
    assert(!validCameraSettings(configuration, settings));
    settings.h264Bitrate = 2000000;
    configuration.modes[0].width = 1920;
    configuration.modes[0].height = 1080;
    assert(!validCameraSettings(configuration, settings));
    configuration.modes[0].width = 800;
    configuration.modes[0].height = 800;
    configuration.modes[0].width = 64;
    assert(!validCameraSettings(configuration, settings));
    settings.codec = VideoCodec::Jpeg;
    assert(validCameraSettings(configuration, settings));
    settings.codec = static_cast<VideoCodec>(99);
    assert(!validCameraSettings(configuration, settings));

    const std::uint8_t jpeg[]{0xff, 0xd8, 0xff, 0xfe, 0, 4, 'A', 'B',
        0xff, 0xdb, 0, 3, 0, 0xff, 0xda, 0, 2, 0x12, 0xff, 0xd9};
    JpegHeaderView header{};
    assert(prepareJpegHeader(jpeg, sizeof(jpeg), header));
    assert(header.length == 11 && header.scanOffset == 17);
    assert(header.bytes[2] == 0xff && header.bytes[3] == 0xdb);
    for (std::size_t size = 0; size < 20; ++size)
        assert(!prepareJpegHeader(jpeg, size, header));
    const std::uint8_t invalid[]{0xff, 0xd8, 0xff, 0xfe, 0xff, 0xff};
    assert(!prepareJpegHeader(invalid, sizeof(invalid), header));
}