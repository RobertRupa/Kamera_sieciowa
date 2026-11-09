#include "camera_stream/CameraHardwareProbe.hpp"

#include <array>
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <new>
#include <mutex>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>

#include "camera_hub/FrameBroker.hpp"
#include "esp_cam_sensor_types.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_video_device.h"
#include "esp_video_init.h"
#include "esp_video_ioctl.h"
#include "driver/jpeg_encode.h"
#include "esp_h264_enc_single_hw.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdkconfig.h"

namespace camera_stream {
namespace {

constexpr char kTag[] = "camera_probe";
constexpr std::size_t kBufferCount = 2;
constexpr std::size_t kMaxBufferBytes = camera_hub::kMaxCaptureFrameBytes;
constexpr std::size_t kMaxJpegBytes = 1024 * 1024;
constexpr std::size_t kFrameSlotCount = camera_hub::FrameBroker::slotCount;
constexpr std::size_t kFrameStorageBytes = kFrameSlotCount * kMaxJpegBytes;
constexpr std::size_t kBufferAlignment = 64;
constexpr std::uint16_t kOv5647ProductId = 0x5647;
constexpr std::uint32_t kVideoFlags = ESP_VIDEO_INIT_FLAGS_MIPI_CSI | ESP_VIDEO_INIT_FLAGS_ISP;
constexpr std::uint32_t kPreviewMaxFps = 20;

struct CaptureBuffer {
    void* address{nullptr};
    std::size_t length{0};
};

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

struct CameraHardwareProbe::State {
    camera_hub::FrameBroker* broker{nullptr};
    std::uint8_t* frameStorage{nullptr};
    std::array<CaptureBuffer, kBufferCount> captureBuffers{};
    std::size_t captureBufferSize{0};
    int cameraFd{-1};
    jpeg_encoder_handle_t jpegEncoder{nullptr};
    jpeg_encode_cfg_t jpegConfig{};
    esp_h264_enc_handle_t h264Encoder{nullptr};
    std::uint8_t* jpegOutput{nullptr};
    std::size_t jpegOutputSize{0};
    SemaphoreHandle_t taskStopped{nullptr};
    TaskHandle_t captureTask{nullptr};
    std::atomic<bool> running{false};
    std::atomic<bool> frameReceived{false};
    std::atomic<bool> encoderErrorLogged{false};
    std::atomic<bool> publishErrorLogged{false};
    std::mutex subscriberMutex;
    std::size_t httpSubscribers{0};
    std::atomic<bool> networkEnabled{false};
    std::atomic<bool> keyFrameRequested{false};
    mutable std::mutex configurationMutex;
    camera_hub::CameraConfiguration configuration{};
    camera_hub::CameraSettings requestedSettings{};
    bool videoInitialized{false};
    bool streamStarted{false};
    std::uint16_t width{0};
    std::uint16_t height{0};
    std::uint8_t fps{0};
};

CameraHardwareProbe::~CameraHardwareProbe() { stopPreview(); }

void CameraHardwareProbe::releaseResources() noexcept {
    if (state_ == nullptr) return;
    auto& state = *state_;

    if (state.cameraFd >= 0) {
        if (state.streamStarted) {
            int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            ioctl(state.cameraFd, VIDIOC_STREAMOFF, &type);
            state.streamStarted = false;
        }
        v4l2_requestbuffers request{};
        request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        request.memory = V4L2_MEMORY_MMAP;
        request.count = 0;
        ioctl(state.cameraFd, VIDIOC_REQBUFS, &request);
        for (const auto& buffer : state.captureBuffers) {
            if (buffer.address != nullptr) munmap(buffer.address, buffer.length);
        }
        close(state.cameraFd);
        state.cameraFd = -1;
    }
    if (state.jpegOutput != nullptr) {
        free(state.jpegOutput);
        state.jpegOutput = nullptr;
    }
    if (state.h264Encoder != nullptr) {
        esp_h264_enc_close(state.h264Encoder);
        esp_h264_enc_del(state.h264Encoder);
        state.h264Encoder = nullptr;
    }
    if (state.jpegEncoder != nullptr) {
        jpeg_del_encoder_engine(state.jpegEncoder);
        state.jpegEncoder = nullptr;
    }
    if (state.videoInitialized) {
        esp_video_deinit_with_flags(kVideoFlags);
        state.videoInitialized = false;
    }
    if (state.broker != nullptr) {
        delete state.broker;
        state.broker = nullptr;
    }
    if (state.frameStorage != nullptr) {
        heap_caps_free(state.frameStorage);
        state.frameStorage = nullptr;
    }
    if (state.taskStopped != nullptr) {
        vSemaphoreDelete(state.taskStopped);
        state.taskStopped = nullptr;
    }
    delete state_;
    state_ = nullptr;
}

esp_err_t CameraHardwareProbe::startPreview(const camera_hub::CameraProbeResult& probe) noexcept {
    if (state_ != nullptr) return ESP_ERR_INVALID_STATE;
    if (probe.state != camera_hub::CameraProbeState::FrameCaptured ||
        probe.sensor != camera_hub::CameraSensorModel::Ov5647) {
        return ESP_ERR_NOT_SUPPORTED;
    }
#if !CONFIG_APP_CAMERA_PROBE_ON_BOOT
    return ESP_ERR_NOT_SUPPORTED;
#else
    state_ = new (std::nothrow) State{};
    if (state_ == nullptr) return ESP_ERR_NO_MEM;
    auto& state = *state_;

    state.frameStorage = static_cast<std::uint8_t*>(heap_caps_aligned_alloc(
        kBufferAlignment, kFrameStorageBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (state.frameStorage == nullptr) {
        releaseResources();
        return ESP_ERR_NO_MEM;
    }
    state.broker = new (std::nothrow) camera_hub::FrameBroker(
        state.frameStorage, kFrameStorageBytes, kMaxJpegBytes);
    if (state.broker == nullptr || !state.broker->valid()) {
        releaseResources();
        return ESP_ERR_NO_MEM;
    }

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
    auto result = esp_video_init_with_flags(&videoConfig, kVideoFlags);
    if (result != ESP_OK) {
        releaseResources();
        return result;
    }
    state.videoInitialized = true;

    state.cameraFd = open(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, O_RDONLY);
    if (state.cameraFd < 0) {
        releaseResources();
        return ESP_FAIL;
    }

    esp_cam_sensor_format_t sensorFormat{};
    if (ioctl(state.cameraFd, VIDIOC_G_SENSOR_FMT, &sensorFormat) != 0) {
        releaseResources();
        return ESP_FAIL;
    }
    for (std::size_t index = 0; index < state.configuration.modes.size(); ++index) {
        v4l2_sensor_format_enum entry{};
        entry.index = static_cast<std::uint32_t>(index);
        if (ioctl(state.cameraFd, VIDIOC_ENUM_SENSOR_FMT, &entry) != 0) break;
        auto& mode = state.configuration.modes[state.configuration.modeCount++];
        mode.id = entry.index;
        mode.width = entry.format.width;
        mode.height = entry.format.height;
        mode.sensorFps = entry.format.fps;
        std::snprintf(mode.name.data(), mode.name.size(), "%s",
                      entry.format.name == nullptr ? "" : entry.format.name);
        if (entry.format.name != nullptr && sensorFormat.name != nullptr &&
            std::strcmp(entry.format.name, sensorFormat.name) == 0)
            state.configuration.settings.modeId = mode.id;
    }

    for (std::uint32_t index = 0; index < 16; ++index) {
        v4l2_fmtdesc description{};
        description.index = index;
        description.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(state.cameraFd, VIDIOC_ENUM_FMT, &description) != 0) break;
        ESP_LOGI(kTag, "capture format[%lu]=0x%08lx",
                 static_cast<unsigned long>(index),
                 static_cast<unsigned long>(description.pixelformat));
    }

    v4l2_format format{};
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(state.cameraFd, VIDIOC_G_FMT, &format) != 0 ||
        format.fmt.pix.width != probe.width || format.fmt.pix.height != probe.height) {
        ESP_LOGW(kTag, "capture source mode differs from probe: %ux%u",
                 static_cast<unsigned int>(format.fmt.pix.width),
                 static_cast<unsigned int>(format.fmt.pix.height));
        releaseResources();
        return ESP_ERR_INVALID_STATE;
    }
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
    if (ioctl(state.cameraFd, VIDIOC_S_FMT, &format) != 0) {
        ESP_LOGW(kTag, "RGB565 capture format request failed: errno=%d", errno);
        releaseResources();
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (format.fmt.pix.pixelformat != V4L2_PIX_FMT_RGB565) {
        ESP_LOGW(kTag, "RGB565 not selected; driver returned format=0x%08lx",
                 static_cast<unsigned long>(format.fmt.pix.pixelformat));
        releaseResources();
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (
        format.fmt.pix.width == 0 || format.fmt.pix.height == 0 ||
        format.fmt.pix.width > 1920 || format.fmt.pix.height > 1920) {
        ESP_LOGW(kTag, "invalid RGB565 capture buffer: %ux%u size=%lu",
                 static_cast<unsigned int>(format.fmt.pix.width),
                 static_cast<unsigned int>(format.fmt.pix.height),
                 static_cast<unsigned long>(format.fmt.pix.sizeimage));
        releaseResources();
        return ESP_ERR_NOT_SUPPORTED;
    }
    state.width = static_cast<std::uint16_t>(format.fmt.pix.width);
    state.height = static_cast<std::uint16_t>(format.fmt.pix.height);

    v4l2_streamparm parameters{};
    parameters.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(state.cameraFd, VIDIOC_G_PARM, &parameters) == 0 &&
        parameters.parm.capture.timeperframe.numerator != 0) {
        const auto frameRate = parameters.parm.capture.timeperframe.denominator /
                               parameters.parm.capture.timeperframe.numerator;
        const auto previewFrameRate = frameRate < kPreviewMaxFps ? frameRate : kPreviewMaxFps;
        state.fps = static_cast<std::uint8_t>(previewFrameRate);
    }

    v4l2_requestbuffers request{};
    request.count = kBufferCount;
    request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    request.memory = V4L2_MEMORY_MMAP;
    if (ioctl(state.cameraFd, VIDIOC_REQBUFS, &request) != 0) {
        releaseResources();
        return ESP_FAIL;
    }
    for (std::size_t index = 0; index < state.captureBuffers.size(); ++index) {
        v4l2_buffer buffer{};
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = index;
        if (ioctl(state.cameraFd, VIDIOC_QUERYBUF, &buffer) != 0 ||
            buffer.length == 0 || buffer.length > kMaxBufferBytes ||
            (index != 0 && buffer.length != state.captureBufferSize)) {
            releaseResources();
            return ESP_FAIL;
        }
        if (index == 0) state.captureBufferSize = buffer.length;
        auto& mapped = state.captureBuffers[index];
        mapped.length = buffer.length;
        mapped.address = mmap(nullptr, buffer.length, PROT_READ | PROT_WRITE,
                              MAP_SHARED, state.cameraFd, buffer.m.offset);
        if (mapped.address == MAP_FAILED) {
            mapped.address = nullptr;
            releaseResources();
            return ESP_ERR_NO_MEM;
        }
        if (ioctl(state.cameraFd, VIDIOC_QBUF, &buffer) != 0) {
            releaseResources();
            return ESP_FAIL;
        }
    }

    jpeg_encode_engine_cfg_t engineConfig{};
    engineConfig.timeout_ms = 1000;
    result = jpeg_new_encoder_engine(&engineConfig, &state.jpegEncoder);
    if (result != ESP_OK) {
        releaseResources();
        return result;
    }
    state.jpegConfig.height = state.height;
    state.jpegConfig.width = state.width;
    state.jpegConfig.src_type = JPEG_ENCODE_IN_FORMAT_RGB565;
    state.jpegConfig.sub_sample = JPEG_DOWN_SAMPLING_YUV422;
    state.jpegConfig.image_quality = 80;
    const std::size_t requestedOutputSize = kMaxJpegBytes;
    jpeg_encode_memory_alloc_cfg_t outputConfig{};
    outputConfig.buffer_direction = JPEG_ENC_ALLOC_OUTPUT_BUFFER;
    state.jpegOutput = static_cast<std::uint8_t*>(jpeg_alloc_encoder_mem(
        requestedOutputSize, &outputConfig, &state.jpegOutputSize));
    if (state.jpegOutput == nullptr || state.jpegOutputSize > kMaxJpegBytes) {
        releaseResources();
        return ESP_ERR_NO_MEM;
    }

    timeval dequeueTimeout{};
    dequeueTimeout.tv_sec = 2;
    if (ioctl(state.cameraFd, VIDIOC_S_DQBUF_TIMEOUT, &dequeueTimeout) != 0) {
        releaseResources();
        return ESP_FAIL;
    }
    state.configuration.settings.previewFps = state.fps == 0 ? 20 : state.fps;
    state.fps = state.configuration.settings.previewFps;
    const auto captureType = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(state.cameraFd, VIDIOC_STREAMON, &captureType) != 0) {
        releaseResources();
        return ESP_FAIL;
    }
    state.streamStarted = true;
    state.taskStopped = xSemaphoreCreateBinary();
    if (state.taskStopped == nullptr) {
        releaseResources();
        return ESP_ERR_NO_MEM;
    }
    state.running.store(true);
    if (xTaskCreate(&CameraHardwareProbe::captureTaskEntry, "camera_preview", 8192,
                    this, 5, &state.captureTask) != pdPASS) {
        state.running.store(false);
        releaseResources();
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(kTag, "persistent capture started: %ux%u source_fps=%u preview_fps=%u pixel=RGB565",
             static_cast<unsigned int>(state.width), static_cast<unsigned int>(state.height),
             static_cast<unsigned int>(sensorFormat.fps), static_cast<unsigned int>(state.fps));
    return ESP_OK;
#endif
}

void CameraHardwareProbe::captureTaskEntry(void* context) {
    auto* service = static_cast<CameraHardwareProbe*>(context);
    service->captureTask();
    vTaskDelete(nullptr);
}

void CameraHardwareProbe::captureTask() noexcept {
    auto& state = *state_;
    std::int64_t lastEncodeUs = 0;
    std::int64_t configurationDeadline = 0;
    camera_hub::CameraSettings rollbackSettings{};
    while (state.running.load()) {
        camera_hub::CameraSettings requested{};
        camera_hub::CameraSettings previous{};
        bool apply = false;
        {
            const std::lock_guard<std::mutex> lock(state.configurationMutex);
            if (state.configuration.state == camera_hub::ConfigurationState::Queued) {
                previous = state.configuration.settings;
                requested = state.requestedSettings;
                state.configuration.state = camera_hub::ConfigurationState::Applying;
                apply = true;
            }
        }
        if (apply) {
            rollbackSettings = previous;
            auto result = requested.modeId == previous.modeId && requested.codec == previous.codec
                ? ESP_OK : restartCapture(requested.modeId, requested.codec);
            if (result == ESP_OK) result = configureEncoder(requested);
            const bool restored = result == ESP_OK || restoreConfiguration(previous);
            {
                const std::lock_guard<std::mutex> lock(state.configurationMutex);
                if (result == ESP_OK) {
                    state.configuration.settings = requested;
                    state.jpegConfig.image_quality = requested.jpegQuality;
                    state.fps = requested.previewFps;
                    configurationDeadline = esp_timer_get_time() + 4000000;
                } else {
                    state.configuration.state = camera_hub::ConfigurationState::Failed;
                }
            }
            if (!restored) break;
        }
        v4l2_buffer buffer{};
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        if (ioctl(state.cameraFd, VIDIOC_DQBUF, &buffer) != 0) {
            if (configurationDeadline != 0 && esp_timer_get_time() >= configurationDeadline) {
                const bool restored = restoreConfiguration(rollbackSettings);
                {
                    const std::lock_guard<std::mutex> lock(state.configurationMutex);
                    state.configuration.settings = rollbackSettings;
                    state.configuration.state = camera_hub::ConfigurationState::Failed;
                    state.jpegConfig.image_quality = rollbackSettings.jpegQuality;
                    state.fps = rollbackSettings.previewFps;
                }
                configurationDeadline = 0;
                if (!restored) break;
                continue;
            }
            if (errno == ETIMEDOUT || errno == EAGAIN) continue;
            ESP_LOGW(kTag, "capture dequeue failed: errno=%d", errno);
            break;
        }
        if (buffer.index >= state.captureBuffers.size()) break;

        if ((buffer.flags & V4L2_BUF_FLAG_DONE) != 0 && buffer.bytesused != 0) {
            state.frameReceived.store(true);
            bool validating = false;
            camera_hub::VideoCodec codec{};
            {
                const std::lock_guard<std::mutex> lock(state.configurationMutex);
                validating = state.configuration.state == camera_hub::ConfigurationState::Applying;
                codec = state.configuration.settings.codec;
            }
            bool hasHttpSubscribers = false;
            {
                const std::lock_guard<std::mutex> lock(state.subscriberMutex);
                hasHttpSubscribers = state.httpSubscribers != 0;
            }
            const auto now = esp_timer_get_time();
            if ((validating || (hasHttpSubscribers && codec == camera_hub::VideoCodec::Jpeg) || state.networkEnabled.load()) &&
                now - lastEncodeUs >= 1000000 / state.fps) {
                std::uint32_t jpegSize = 0;
                const auto* source = static_cast<const std::uint8_t*>(
                    state.captureBuffers[buffer.index].address);
                esp_err_t encodeResult = ESP_FAIL;
                if (codec == camera_hub::VideoCodec::Jpeg) {
                    encodeResult = jpeg_encoder_process(
                        state.jpegEncoder, &state.jpegConfig, source,
                        static_cast<std::uint32_t>(state.captureBufferSize),
                        state.jpegOutput, static_cast<std::uint32_t>(state.jpegOutputSize), &jpegSize);
                } else if (state.h264Encoder != nullptr) {
                    if (state.keyFrameRequested.exchange(false)) {
                        esp_h264_enc_param_hw_handle_t parameters = nullptr;
                        if (esp_h264_enc_hw_get_param_hd(state.h264Encoder, &parameters) == ESP_H264_ERR_OK)
                            static_cast<void>(esp_h264_enc_force_idr(&parameters->base));
                    }
                    esp_h264_enc_in_frame_t input{};
                    input.raw_data.buffer = const_cast<std::uint8_t*>(source);
                    input.raw_data.len = state.captureBufferSize;
                    esp_h264_enc_out_frame_t output{};
                    output.raw_data.buffer = state.jpegOutput;
                    output.raw_data.len = state.jpegOutputSize;
                    const auto result = esp_h264_enc_process(state.h264Encoder, &input, &output);
                    if (result == ESP_H264_ERR_OK && output.length != 0 && output.length <= state.jpegOutputSize) {
                        jpegSize = output.length;
                        encodeResult = ESP_OK;
                    }
                }
                if (encodeResult == ESP_OK) {
                    const auto published = state.broker->publish(state.jpegOutput, jpegSize,
                                                                 static_cast<std::uint64_t>(now), codec);
                    if (validating && published == camera_hub::PublishResult::Published) {
                        const std::lock_guard<std::mutex> lock(state.configurationMutex);
                        state.configuration.state = camera_hub::ConfigurationState::Applied;
                        configurationDeadline = 0;
                    }
                    if (published != camera_hub::PublishResult::Published &&
                        published != camera_hub::PublishResult::PoolExhausted &&
                        !state.publishErrorLogged.exchange(true)) {
                        ESP_LOGW(kTag, "encoded frame rejected: result=%u bytes=%lu",
                                 static_cast<unsigned int>(published), static_cast<unsigned long>(jpegSize));
                    }
                } else if (!state.encoderErrorLogged.exchange(true)) {
                    ESP_LOGW(kTag, "hardware encode failed: %s input=%lu output=%lu",
                             esp_err_to_name(encodeResult),
                             static_cast<unsigned long>(state.captureBufferSize),
                             static_cast<unsigned long>(state.jpegOutputSize));
                }
                lastEncodeUs = now;
                if (validating && encodeResult != ESP_OK) {
                    const bool restored = restoreConfiguration(rollbackSettings);
                    configurationDeadline = 0;
                    if (!restored) break;
                    continue;
                }
            }
        }
        if (ioctl(state.cameraFd, VIDIOC_QBUF, &buffer) != 0) {
            ESP_LOGW(kTag, "capture requeue failed: errno=%d", errno);
            break;
        }
    }
    if (state.streamStarted) {
        const auto captureType = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(state.cameraFd, VIDIOC_STREAMOFF, &captureType);
        state.streamStarted = false;
    }
    state.running.store(false);
    if (state.taskStopped != nullptr) xSemaphoreGive(state.taskStopped);
}

void CameraHardwareProbe::stopPreview() noexcept {
    if (state_ == nullptr) return;
    auto& state = *state_;
    state.running.store(false);
    if (state.captureTask != nullptr && state.taskStopped != nullptr) {
        if (xSemaphoreTake(state.taskStopped, pdMS_TO_TICKS(5000)) != pdPASS) {
            ESP_LOGE(kTag, "camera capture task did not stop; retaining owned resources");
            return;
        }
        state.captureTask = nullptr;
    }
    releaseResources();
}

CameraCaptureStatus CameraHardwareProbe::captureStatus() const noexcept {
    CameraCaptureStatus status{};
    if (state_ == nullptr) return status;
    const std::lock_guard<std::mutex> lock(state_->configurationMutex);
    status.running = state_->running.load();
    status.frameReceived = state_->frameReceived.load();
    status.width = state_->width;
    status.height = state_->height;
    status.fps = state_->fps;
    status.codec = state_->configuration.settings.codec;
    return status;
}

camera_hub::CameraConfiguration CameraHardwareProbe::configuration() const noexcept {
    if (state_ == nullptr) return {};
    const std::lock_guard<std::mutex> lock(state_->configurationMutex);
    return state_->configuration;
}

esp_err_t CameraHardwareProbe::requestConfiguration(const camera_hub::CameraSettings& settings) noexcept {
    if (state_ == nullptr || !state_->running.load()) return ESP_ERR_INVALID_STATE;
    const std::lock_guard<std::mutex> lock(state_->configurationMutex);
    auto& configuration = state_->configuration;
    if (!camera_hub::validCameraSettings(configuration, settings)) return ESP_ERR_INVALID_ARG;
    if (configuration.state == camera_hub::ConfigurationState::Queued ||
        configuration.state == camera_hub::ConfigurationState::Applying) return ESP_ERR_INVALID_STATE;
    state_->requestedSettings = settings;
    configuration.state = camera_hub::ConfigurationState::Queued;
    return ESP_OK;
}

esp_err_t CameraHardwareProbe::restartCapture(std::uint32_t modeId, camera_hub::VideoCodec codec) noexcept {
    auto& state = *state_;
    const auto type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    state.frameReceived.store(false);
    if (state.streamStarted) {
        if (ioctl(state.cameraFd, VIDIOC_STREAMOFF, &type) != 0) return ESP_FAIL;
        state.streamStarted = false;
    }
    for (auto& buffer : state.captureBuffers) {
        if (buffer.address != nullptr) munmap(buffer.address, buffer.length);
        buffer = {};
    }
    v4l2_requestbuffers request{};
    request.type = type;
    request.memory = V4L2_MEMORY_MMAP;
    if (ioctl(state.cameraFd, VIDIOC_REQBUFS, &request) != 0) return ESP_FAIL;
    v4l2_sensor_format_enum mode{};
    mode.index = modeId;
    if (ioctl(state.cameraFd, VIDIOC_ENUM_SENSOR_FMT, &mode) != 0 ||
        ioctl(state.cameraFd, VIDIOC_S_SENSOR_FMT, &mode.format) != 0) return ESP_FAIL;
    v4l2_format format{};
    format.type = type;
    if (ioctl(state.cameraFd, VIDIOC_G_FMT, &format) != 0) return ESP_FAIL;
    const auto pixelFormat = codec == camera_hub::VideoCodec::H264 ? V4L2_PIX_FMT_YUV420 : V4L2_PIX_FMT_RGB565;
    format.fmt.pix.pixelformat = pixelFormat;
    if (ioctl(state.cameraFd, VIDIOC_S_FMT, &format) != 0 ||
        format.fmt.pix.pixelformat != pixelFormat) return ESP_FAIL;
    request.count = kBufferCount;
    if (ioctl(state.cameraFd, VIDIOC_REQBUFS, &request) != 0) return ESP_FAIL;
    state.captureBufferSize = 0;
    for (std::size_t index = 0; index < state.captureBuffers.size(); ++index) {
        v4l2_buffer buffer{};
        buffer.type = type;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = index;
        if (ioctl(state.cameraFd, VIDIOC_QUERYBUF, &buffer) != 0 || buffer.length == 0 ||
            buffer.length > kMaxBufferBytes ||
            (index != 0 && buffer.length != state.captureBufferSize)) return ESP_FAIL;
        state.captureBufferSize = buffer.length;
        auto& mapped = state.captureBuffers[index];
        mapped.length = buffer.length;
        mapped.address = mmap(nullptr, buffer.length, PROT_READ | PROT_WRITE,
                              MAP_SHARED, state.cameraFd, buffer.m.offset);
        if (mapped.address == MAP_FAILED) {
            mapped.address = nullptr;
            return ESP_ERR_NO_MEM;
        }
        if (ioctl(state.cameraFd, VIDIOC_QBUF, &buffer) != 0) return ESP_FAIL;
    }
    if (ioctl(state.cameraFd, VIDIOC_STREAMON, &type) != 0) return ESP_FAIL;
    state.streamStarted = true;
    {
        const std::lock_guard<std::mutex> lock(state.configurationMutex);
        state.width = format.fmt.pix.width;
        state.height = format.fmt.pix.height;
        state.jpegConfig.width = state.width;
        state.jpegConfig.height = state.height;
    }
    return ESP_OK;
}

void CameraHardwareProbe::subscribeHttp() noexcept {
    if (state_ == nullptr || state_->broker == nullptr) return;
    const std::lock_guard<std::mutex> lock(state_->subscriberMutex);
    if (state_->httpSubscribers++ == 0) {
        state_->broker->setEnabled(camera_hub::Output::Http, true);
        ESP_LOGI(kTag, "HTTP MJPEG subscriber connected");
    }
}

void CameraHardwareProbe::unsubscribeHttp() noexcept {
    if (state_ == nullptr || state_->broker == nullptr) return;
    const std::lock_guard<std::mutex> lock(state_->subscriberMutex);
    if (state_->httpSubscribers != 0 && --state_->httpSubscribers == 0) {
        state_->broker->setEnabled(camera_hub::Output::Http, false);
        ESP_LOGI(kTag, "HTTP MJPEG subscriber disconnected");
    }
}

camera_hub::FrameLease CameraHardwareProbe::tryTakeHttpFrame() noexcept {
    return state_ == nullptr || state_->broker == nullptr
        ? camera_hub::FrameLease{} : state_->broker->tryTake(camera_hub::Output::Http);
}

void CameraHardwareProbe::setNetworkEnabled(bool enabled) noexcept {
    if (state_ == nullptr || state_->broker == nullptr) return;
    state_->broker->setEnabled(camera_hub::Output::Rtsp, enabled);
    state_->networkEnabled.store(enabled);
}

void CameraHardwareProbe::requestKeyFrame() noexcept {
    if (state_ != nullptr) state_->keyFrameRequested.store(true);
}

camera_hub::FrameLease CameraHardwareProbe::tryTakeNetworkFrame() noexcept {
    if (state_ == nullptr || state_->broker == nullptr) return {};
    return configuration().settings.codec == camera_hub::VideoCodec::H264
        ? state_->broker->tryTake(camera_hub::Output::Rtsp)
        : state_->broker->tryTakeLatest(camera_hub::Output::Rtsp);
}

esp_err_t CameraHardwareProbe::configureEncoder(const camera_hub::CameraSettings& settings) noexcept {
    auto& state = *state_;
    if (state.h264Encoder != nullptr) {
        esp_h264_enc_close(state.h264Encoder);
        esp_h264_enc_del(state.h264Encoder);
        state.h264Encoder = nullptr;
    }
    if (settings.codec == camera_hub::VideoCodec::Jpeg) return ESP_OK;
    esp_h264_enc_cfg_hw_t config{};
    config.pic_type = ESP_H264_RAW_FMT_O_UYY_E_VYY;
    config.res.width = state.width;
    config.res.height = state.height;
    config.fps = settings.previewFps;
    config.gop = settings.previewFps;
    config.rc.bitrate = settings.h264Bitrate;
    config.rc.qp_min = 20;
    config.rc.qp_max = 40;
    if (esp_h264_enc_hw_new(&config, &state.h264Encoder) != ESP_H264_ERR_OK ||
        esp_h264_enc_open(state.h264Encoder) != ESP_H264_ERR_OK) {
        if (state.h264Encoder != nullptr) {
            esp_h264_enc_del(state.h264Encoder);
            state.h264Encoder = nullptr;
        }
        return ESP_FAIL;
    }
    return ESP_OK;
}

bool CameraHardwareProbe::restoreConfiguration(const camera_hub::CameraSettings& settings) noexcept {
    const bool restored = restartCapture(settings.modeId, settings.codec) == ESP_OK && configureEncoder(settings) == ESP_OK;
    const std::lock_guard<std::mutex> lock(state_->configurationMutex);
    state_->configuration.settings = settings;
    state_->configuration.state = camera_hub::ConfigurationState::Failed;
    state_->jpegConfig.image_quality = settings.jpegQuality;
    state_->fps = settings.previewFps;
    return restored;
}

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