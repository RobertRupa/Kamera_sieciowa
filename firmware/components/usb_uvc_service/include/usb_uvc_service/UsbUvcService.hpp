#pragma once

#include <atomic>
#include <cstdint>
#include "camera_stream/CameraHardwareProbe.hpp"
#include "camera_hub/FrameLease.hpp"
#include "usb_device_uvc.h"

namespace usb_uvc_service {

// Single USB UVC MJPEG device using the existing JPEG encoder and FrameBroker.
// Camera CSI and ISP remain exclusively owned by CameraHardwareProbe.
class UsbUvcService final {
public:
    explicit UsbUvcService(camera_stream::CameraHardwareProbe& camera) noexcept;
    ~UsbUvcService();
    UsbUvcService(const UsbUvcService&) = delete;
    UsbUvcService& operator=(const UsbUvcService&) = delete;
    [[nodiscard]] esp_err_t start() noexcept;
    [[nodiscard]] bool ready() const noexcept { return initialized_; }
private:
    static esp_err_t onStart(uvc_format_t format, int width, int height,
                            int rate, void* context);
    static uvc_fb_t* onGetFrame(void* context);
    static void onReturnFrame(uvc_fb_t* frame, void* context);
    static void onStop(void* context);

    camera_stream::CameraHardwareProbe& camera_;
    camera_hub::FrameLease inFlight_{};
    uvc_fb_t frame_{};
    std::uint8_t* transferBuffer_{nullptr};
    std::atomic<bool> streaming_{false};
    bool initialized_{false};
    int requestedWidth_{0};
    int requestedHeight_{0};
    int requestedFps_{0};
    // Updated exclusively by the UVC frame worker in onGetFrame().
    std::int64_t metricsStartedUs_{0};
    std::uint32_t framesFed_{0};
    std::uint64_t bytesFed_{0};
};

} // namespace usb_uvc_service
