#include "camera_stream/CameraHardwareProbe.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>

#include "esp_cam_sensor_types.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_video_device.h"
#include "esp_video_init.h"
#include "esp_video_ioctl.h"
#include "sdkconfig.h"

namespace camera_stream {
namespace {

constexpr char kTag[] = "camera_probe";
constexpr std::size_t kBufferCount = 2;
constexpr std::size_t kMaxBufferBytes = 2 * 1024 * 1024;
constexpr std::uint16_t kOv5647ProductId = 0x5647;
constexpr std::uint32_t kVideoFlags = ESP_VIDEO_INIT_FLAGS_MIPI_CSI | ESP_VIDEO_INIT_FLAGS_ISP;

struct MappedBuffer {
    void* address{nullptr};
    std::size_t length{0};
};

esp_err_t captureOneFrame(int fd, camera_hub::CameraProbeResult& result) noexcept {
    const auto type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    struct v4l2_format format{};
    struct v4l2_requestbuffers request{};
    struct v4l2_buffer frame{};
    struct timeval timeout{};
    std::array<MappedBuffer, kBufferCount> buffers{};
    bool buffersRequested = false;
    bool streaming = false;
    bool frameReceived = false;
    esp_err_t resultCode = ESP_FAIL;

    format.type = type;
    if (ioctl(fd, VIDIOC_G_FMT, &format) != 0 || format.fmt.pix.width == 0 ||
        format.fmt.pix.height == 0) {
        goto cleanup;
    }
    if (ioctl(fd, VIDIOC_S_FMT, &format) != 0) goto cleanup;

    request.count = kBufferCount;
    request.type = type;
    request.memory = V4L2_MEMORY_MMAP;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    if (ioctl(fd, VIDIOC_REQBUFS, &request) != 0) goto cleanup;
    buffersRequested = true;

    for (std::size_t index = 0; index < buffers.size(); ++index) {
        struct v4l2_buffer buffer{};
        buffer.type = type;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = index;
        if (ioctl(fd, VIDIOC_QUERYBUF, &buffer) != 0 || buffer.length == 0 ||
            buffer.length > kMaxBufferBytes) {
            goto cleanup;
        }
        buffers[index].length = buffer.length;
        buffers[index].address = mmap(nullptr, buffer.length, PROT_READ | PROT_WRITE,
                                      MAP_SHARED, fd, buffer.m.offset);
        if (buffers[index].address == MAP_FAILED) {
            buffers[index].address = nullptr;
            goto cleanup;
        }
        if (ioctl(fd, VIDIOC_QBUF, &buffer) != 0) goto cleanup;
    }

    if (ioctl(fd, VIDIOC_S_DQBUF_TIMEOUT, &timeout) != 0 ||
        ioctl(fd, VIDIOC_STREAMON, &type) != 0) {
        goto cleanup;
    }
    streaming = true;

    frame.type = type;
    frame.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_DQBUF, &frame) != 0 || frame.index >= buffers.size() ||
        (frame.flags & V4L2_BUF_FLAG_DONE) == 0 || frame.bytesused == 0 ||
        frame.bytesused > buffers[frame.index].length) {
        goto cleanup;
    }
    frameReceived = true;
    result.width = static_cast<std::uint16_t>(format.fmt.pix.width);
    result.height = static_cast<std::uint16_t>(format.fmt.pix.height);
    result.frameBytes = frame.bytesused;
    resultCode = ESP_OK;

cleanup:
    if (streaming && ioctl(fd, VIDIOC_STREAMOFF, &type) != 0) resultCode = ESP_FAIL;
    for (const auto& buffer : buffers) {
        if (buffer.address != nullptr) munmap(buffer.address, buffer.length);
    }
    if (buffersRequested) {
        request.count = 0;
        if (ioctl(fd, VIDIOC_REQBUFS, &request) != 0) resultCode = ESP_FAIL;
    }
    if (!frameReceived && resultCode == ESP_OK) resultCode = ESP_FAIL;
    return resultCode;
}

} // namespace

camera_hub::CameraProbeResult CameraHardwareProbe::run() const noexcept {
    camera_hub::CameraProbeResult result{};
#if CONFIG_APP_CAMERA_PROBE_ON_BOOT
    esp_video_init_csi_config_t csiConfig{};
    csiConfig.sccb_config.init_sccb = true;
    csiConfig.sccb_config.i2c_config.port = I2C_NUM_0;
    csiConfig.sccb_config.i2c_config.scl_pin = GPIO_NUM_8;
    csiConfig.sccb_config.i2c_config.sda_pin = GPIO_NUM_7;
    csiConfig.sccb_config.freq = 400000;
    csiConfig.reset_pin = GPIO_NUM_NC;
    csiConfig.pwdn_pin = GPIO_NUM_NC;
    csiConfig.dont_init_ldo = false;
    esp_video_init_config_t videoConfig{};
    videoConfig.csi = &csiConfig;
    const auto initResult = esp_video_init_with_flags(&videoConfig, kVideoFlags);
    if (initResult != ESP_OK) {
        result.state = camera_hub::CameraProbeState::Error;
        ESP_LOGW(kTag, "video initialization failed: %s", esp_err_to_name(initResult));
        return result;
    }

    const int fd = open(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, O_RDONLY);
    if (fd < 0) {
        result.state = errno == ENOENT ? camera_hub::CameraProbeState::NoSupportedSensor
                                       : camera_hub::CameraProbeState::Error;
    } else {
        struct v4l2_capability capability{};
        esp_cam_sensor_id_t chipId{};
        struct v4l2_ext_control control{};
        struct v4l2_ext_controls controls{};
        control.id = ESP_CAM_SENSOR_IOC_G_CHIP_ID;
        control.p_u8 = reinterpret_cast<std::uint8_t*>(&chipId);
        control.size = sizeof(chipId);
        controls.ctrl_class = V4L2_CTRL_CLASS_ESP_CAM_IOCTL;
        controls.count = 1;
        controls.controls = &control;

        if (ioctl(fd, VIDIOC_QUERYCAP, &capability) != 0 ||
            ioctl(fd, VIDIOC_G_EXT_CTRLS, &controls) != 0) {
            result.state = camera_hub::CameraProbeState::Error;
        } else if (chipId.pid != kOv5647ProductId) {
            result.state = camera_hub::CameraProbeState::NoSupportedSensor;
        } else {
            result.sensor = camera_hub::CameraSensorModel::Ov5647;
            result.state = camera_hub::CameraProbeState::SensorIdentified;
            if (captureOneFrame(fd, result) == ESP_OK) {
                result.state = camera_hub::CameraProbeState::FrameCaptured;
            } else {
                result.state = camera_hub::CameraProbeState::CaptureFailed;
            }
        }
        close(fd);
    }

    const auto deinitResult = esp_video_deinit_with_flags(kVideoFlags);
    if (deinitResult != ESP_OK) {
        ESP_LOGW(kTag, "video deinitialization failed: %s", esp_err_to_name(deinitResult));
        if (result.state == camera_hub::CameraProbeState::FrameCaptured)
            result.state = camera_hub::CameraProbeState::Error;
    }
#else
    result.state = camera_hub::CameraProbeState::NotRun;
#endif
    ESP_LOGI(kTag, "probe state=%s sensor=%s frame=%ux%u bytes=%lu",
             camera_hub::cameraProbeStateKey(result.state),
             camera_hub::cameraSensorName(result.sensor) == nullptr ? "unknown" :
                 camera_hub::cameraSensorName(result.sensor),
             static_cast<unsigned int>(result.width), static_cast<unsigned int>(result.height),
             static_cast<unsigned long>(result.frameBytes));
    return result;
}

} // namespace camera_stream