#include "usb_uvc_service/UsbUvcService.hpp"

#include <cstddef>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#if CONFIG_APP_USB_UVC_ENABLE
#include "tusb.h"
#endif
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace usb_uvc_service {
namespace {
constexpr char kTag[] = "usb_uvc";
constexpr std::size_t kTransferBufferBytes = 1024 * 1024; // FrameBroker JPEG slot size
} // namespace

UsbUvcService::UsbUvcService(camera_stream::CameraHardwareProbe& camera) noexcept
    : camera_(camera) {}

UsbUvcService::~UsbUvcService() {
    if (initialized_) {
        const auto result = uvc_device_deinit(); // joins the component's UVC worker
        if (result != ESP_OK) {
            ESP_LOGE(kTag, "UVC deinit failed: %s; retaining transfer buffer",
                     esp_err_to_name(result));
            return;
        }
    }
    onStop(this);
    inFlight_.reset();
    if (transferBuffer_ != nullptr) heap_caps_free(transferBuffer_);
}

esp_err_t UsbUvcService::start() noexcept {
#if !CONFIG_APP_USB_UVC_ENABLE
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (initialized_) return ESP_ERR_INVALID_STATE;
    if (!camera_.captureStatus().running) return ESP_ERR_INVALID_STATE;
    transferBuffer_ = static_cast<std::uint8_t*>(heap_caps_aligned_alloc(
        64, kTransferBufferBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (transferBuffer_ == nullptr) return ESP_ERR_NO_MEM;
    uvc_device_config_t config{};
    config.uvc_buffer = transferBuffer_;
    config.uvc_buffer_size = kTransferBufferBytes;
    config.start_cb = &UsbUvcService::onStart;
    config.fb_get_cb = &UsbUvcService::onGetFrame;
    config.fb_return_cb = &UsbUvcService::onReturnFrame;
    config.stop_cb = &UsbUvcService::onStop;
    config.cb_ctx = this;
    auto result = uvc_device_config(0, &config);
    if (result == ESP_OK) result = uvc_device_init();
    if (result != ESP_OK) {
        heap_caps_free(transferBuffer_);
        transferBuffer_ = nullptr;
        return result;
    }
    initialized_ = true;
    ESP_LOGI(kTag,
             "USB HS UVC MJPEG stack started (no host enumeration confirmed); awaiting UVC host COMMIT");
    ESP_LOGI(kTag,
             "If PC cannot detect camera: verify board USB OTG device role, direct data port and VBUS");
    return ESP_OK;
#endif
}

esp_err_t UsbUvcService::onStart(uvc_format_t format, int width, int height,
                                int rate, void* context) {
    auto& service = *static_cast<UsbUvcService*>(context);
    ESP_LOGI(kTag, "USB host requested UVC format=%d, size=%dx%d, fps=%d",
             static_cast<int>(format), width, height, rate);
    if (format != UVC_FORMAT_JPEG || width <= 0 || height <= 0 || rate <= 0) {
        ESP_LOGW(kTag, "Unsupported UVC request; MJPEG modes only");
        return ESP_ERR_NOT_SUPPORTED;
    }
    const auto result = service.camera_.startUsbStream(width, height, rate);
    if (result != ESP_OK)
        ESP_LOGE(kTag, "UVC host stream negotiation failed for %dx%d@%d: %s",
                 width, height, rate, esp_err_to_name(result));
    if (result == ESP_OK) {
        service.requestedWidth_ = width;
        service.requestedHeight_ = height;
        service.requestedFps_ = rate;
        service.stoppedAtUs_.store(0);
        const auto commitUs = esp_timer_get_time();
        service.lastCommitUs_.store(commitUs);
        service.lastFrameRequestUs_.store(commitUs);
        service.streaming_.store(true);
        ESP_LOGI(kTag, "UVC COMMIT %dx%d @ %d nominal sensor FPS", width, height, rate);
    }
    return result;
}

uvc_fb_t* UsbUvcService::onGetFrame(void* context) {
    auto& service = *static_cast<UsbUvcService*>(context);
    if (!service.streaming_.load()) {
        service.metricsStartedUs_ = 0;
        service.framesFed_ = 0;
        service.bytesFed_ = 0;
        return nullptr;
    }
    const auto nowUs = esp_timer_get_time();
    service.lastFrameRequestUs_.store(nowUs);
    if (service.metricsStartedUs_ == 0) service.metricsStartedUs_ = nowUs;
    if (nowUs - service.metricsStartedUs_ >= 5000000) {
        const auto elapsed = static_cast<std::uint64_t>(nowUs - service.metricsStartedUs_);
        const auto fpsX10 = elapsed
            ? static_cast<std::uint64_t>(service.framesFed_) * 10000000ULL / elapsed : 0;
        ESP_LOGI(kTag,
                 "UVC feeder 5s: frames=%lu (fps=%llu.%llu), avg_jpeg=%llu B "
                 "(driver acceptance; not host-delivered FPS)",
                 static_cast<unsigned long>(service.framesFed_),
                 static_cast<unsigned long long>(fpsX10 / 10),
                 static_cast<unsigned long long>(fpsX10 % 10),
                 static_cast<unsigned long long>(service.framesFed_
                     ? service.bytesFed_ / service.framesFed_ : 0));
        service.metricsStartedUs_ = nowUs;
        service.framesFed_ = 0;
        service.bytesFed_ = 0;
    }
    const auto status = service.camera_.captureStatus();
    const auto configuration = service.camera_.configuration();
    if (!status.running || status.codec != camera_hub::VideoCodec::Jpeg ||
        status.width != service.requestedWidth_ || status.height != service.requestedHeight_ ||
        status.fps != service.requestedFps_ ||
        configuration.state == camera_hub::ConfigurationState::Queued ||
        configuration.state == camera_hub::ConfigurationState::Applying) {
        vTaskDelay(pdMS_TO_TICKS(5));
        return nullptr;
    }
    service.inFlight_.reset();
    service.inFlight_ = service.camera_.tryTakeUsbFrame();
    if (!service.inFlight_ || service.inFlight_.codec() != camera_hub::VideoCodec::Jpeg) {
        vTaskDelay(pdMS_TO_TICKS(2));
        return nullptr;
    }
    const auto us = service.inFlight_.timestampUs();
    service.frame_.buf = const_cast<std::uint8_t*>(service.inFlight_.data());
    service.frame_.len = service.inFlight_.size();
    service.frame_.width = status.width;
    service.frame_.height = status.height;
    service.frame_.format = UVC_FORMAT_JPEG;
    ++service.framesFed_;
    service.bytesFed_ += service.frame_.len;
    service.frame_.timestamp.tv_sec = static_cast<time_t>(us / 1000000);
    service.frame_.timestamp.tv_usec = static_cast<suseconds_t>(us % 1000000);
    return &service.frame_;
}

void UsbUvcService::onReturnFrame(uvc_fb_t* frame, void* context) {
    auto& service = *static_cast<UsbUvcService*>(context);
    if (frame == &service.frame_) service.inFlight_.reset();
}

void UsbUvcService::onStop(void* context) {
    auto& service = *static_cast<UsbUvcService*>(context);
    const bool wasStreaming = service.streaming_.exchange(false);
    service.camera_.stopUsbStream();
    service.stoppedAtUs_.store(esp_timer_get_time());
    if (wasStreaming) ESP_LOGI(kTag, "UVC stream stopped; pending host disconnect/re-COMMIT");
    // onReturnFrame owns the lease until the UVC task finishes memcpy.
}

void UsbUvcService::pollConnection() noexcept {
#if CONFIG_APP_USB_UVC_ENABLE
    if (!initialized_) return;
    const auto now = esp_timer_get_time();
    // usb_device_uvc 1.3.1 logs unmount but does not invoke stop_cb there.
    // This path also detects when an application closes /dev/videoX.
    if (streaming_.load() && now - lastCommitUs_.load() >= 1000000 &&
        (!tud_mounted() || !tud_video_n_streaming(0, 0) ||
         now - lastFrameRequestUs_.load() >= 4000000)) {
        ESP_LOGI(kTag, "USB host streaming ended/stalled (mounted=%d, streaming=%d); releasing camera",
                 static_cast<int>(tud_mounted()), static_cast<int>(tud_video_n_streaming(0, 0)));
        onStop(this);
    }
    auto stoppedAt = stoppedAtUs_.load();
    if (!streaming_.load() && stoppedAt != 0 && now - stoppedAt >= 1000000) {
        // Stop callback also fires immediately before a new COMMIT; the grace
        // period avoids restoring the user's mode between USB renegotiations.
        if (stoppedAtUs_.compare_exchange_strong(
                stoppedAt, 0)) {
            camera_.requestUsbProfileRestore();
        }
    }
#endif
}

} // namespace usb_uvc_service
