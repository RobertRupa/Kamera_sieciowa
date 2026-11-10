#pragma once

#include <cstddef>
#include "camera_hub/CameraProbeResult.hpp"
#include "camera_hub/CameraConfiguration.hpp"
#include "camera_hub/FrameLease.hpp"
#include "esp_err.h"

namespace camera_stream {

struct CameraCaptureStatus {
    bool running{false};
    bool frameReceived{false};
    std::uint16_t width{0};
    std::uint16_t height{0};
    std::uint8_t fps{0};
    camera_hub::VideoCodec codec{camera_hub::VideoCodec::Jpeg};
};

class CameraHardwareProbe final {
public:
    CameraHardwareProbe() noexcept = default;
    ~CameraHardwareProbe();
    CameraHardwareProbe(const CameraHardwareProbe&) = delete;
    CameraHardwareProbe& operator=(const CameraHardwareProbe&) = delete;

    [[nodiscard]] camera_hub::CameraProbeResult run() const noexcept;
    [[nodiscard]] esp_err_t startPreview(const camera_hub::CameraProbeResult& probe) noexcept;
    void stopPreview() noexcept;
    [[nodiscard]] CameraCaptureStatus captureStatus() const noexcept;
    [[nodiscard]] camera_hub::CameraConfiguration configuration() const noexcept;
    [[nodiscard]] esp_err_t requestConfiguration(const camera_hub::CameraSettings& settings) noexcept;
    // USB subscribers receive the same immutable hardware-encoded JPEG frames.
    [[nodiscard]] esp_err_t startUsbStream(int width, int height, int fps) noexcept;
    void stopUsbStream() noexcept;
    // Called after the host has stopped or disconnected (not during re-COMMIT).
    // The capture task restores the user's pre-USB sensor configuration.
    void requestUsbProfileRestore() noexcept;
    [[nodiscard]] bool usbStreaming() const noexcept;
    [[nodiscard]] camera_hub::FrameLease tryTakeUsbFrame() noexcept;
    void subscribeHttp() noexcept;
    void unsubscribeHttp() noexcept;
    [[nodiscard]] camera_hub::FrameLease tryTakeHttpFrame() noexcept;
    void setNetworkEnabled(bool enabled) noexcept;
    void requestKeyFrame() noexcept;
    [[nodiscard]] camera_hub::FrameLease tryTakeNetworkFrame() noexcept;

private:
    static void captureTaskEntry(void* context);
    void captureTask() noexcept;
    void releaseResources() noexcept;
    [[nodiscard]] esp_err_t restartCapture(std::uint32_t modeId, camera_hub::VideoCodec codec) noexcept;
    [[nodiscard]] esp_err_t configureEncoder(const camera_hub::CameraSettings& settings) noexcept;
    [[nodiscard]] bool restoreConfiguration(const camera_hub::CameraSettings& settings) noexcept;

    struct State;
    State* state_{nullptr};
};

} // namespace camera_stream