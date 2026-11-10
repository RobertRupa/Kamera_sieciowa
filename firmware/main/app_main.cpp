#include "board_support/BoardConfig.hpp"
#include "board_capabilities/CapabilityRegistry.hpp"
#include "camera_stream/CameraHardwareProbe.hpp"
#include "usb_uvc_service/UsbUvcService.hpp"
#include "persistence/NvsCredentialStore.hpp"
#include "connectivity/WifiService.hpp"
#include "connectivity/HostedProbe.hpp"
#include "discovery/MdnsService.hpp"
#include "camera_hub/DiscoveryNames.hpp"
#include "app_services/NetworkProvisioner.hpp"
#include "web_api/ProvisioningApi.hpp"
#include "esp_timer.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "sdkconfig.h"

#if !CONFIG_APP_DEV_MODE && CONFIG_APP_DEV_DEFAULT_CREDENTIALS
#error "Default credentials are forbidden outside explicit development mode."
#endif

#if CONFIG_ESP_HOSTED_AUTO_CALL_INIT_BEFORE_APP_MAIN
#error "ESP-Hosted auto-init must remain disabled until verified application startup."
#endif

namespace {
[[noreturn]] void keepUsbAliveWithoutNetwork(usb_uvc_service::UsbUvcService& usbUvc) noexcept {
    ESP_LOGW("camera_hub", "Network unavailable; maintaining camera and USB UVC service");
    for (;;) {
        usbUvc.pollConnection();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
} // namespace

extern "C" void app_main() {
#if CONFIG_APP_C6_TRANSPORT_PROBE
    ESP_LOGI("camera_hub", "C6_PROBE begin; no credentials or AP/STA configured");
    const connectivity::HostedProbe probe;
    connectivity::CompanionInfo companion{};
    const auto probeResult = probe.run(true, companion);
    ESP_LOGI("camera_hub", "C6_PROBE result=%s target=%s chip_id=%lu",
             esp_err_to_name(probeResult), companion.target.data(), static_cast<unsigned long>(companion.chipId));
#endif
    persistence::NvsCredentialStore credentialStore;
    const auto storeResult = credentialStore.initialize();
    if (storeResult == camera_hub::CredentialError::None) {
        ESP_LOGW("camera_hub", "Plaintext NVS credential store initialized; stored Wi-Fi passwords are readable from Flash");
    } else {
        ESP_LOGW("camera_hub", "Plaintext NVS credential store unavailable (status=%u, SDK=%s)",
                 static_cast<unsigned int>(storeResult), esp_err_to_name(credentialStore.lastPlatformError()));
    }
    const board_support::BoardConfig board;
#if CONFIG_APP_BOARD_WAVESHARE_P4_C6_SDIO
    ESP_LOGI("camera_hub",
             "board=Waveshare ESP32-P4 + ESP32-C6, target=esp32p4, "
             "radio=esp32c6 via existing ESP-Hosted 4-bit SDIO");
#endif
    camera_stream::CameraHardwareProbe cameraProbe;
    const auto cameraProbeResult = cameraProbe.run();
    ESP_LOGI("camera_hub", "camera probe=%s sensor=%s frame=%ux%u bytes=%lu",
             camera_hub::cameraProbeStateKey(cameraProbeResult.state),
             camera_hub::cameraSensorName(cameraProbeResult.sensor) == nullptr
                 ? "unknown" : camera_hub::cameraSensorName(cameraProbeResult.sensor),
             static_cast<unsigned int>(cameraProbeResult.width),
             static_cast<unsigned int>(cameraProbeResult.height),
             static_cast<unsigned long>(cameraProbeResult.frameBytes));
    board_capabilities::BuildFeatures built{};
    built.wifiAdapter = true;
    built.wifiWizard = true;
    built.mdns = true;
    built.webgui = true;
    built.credentialStorage = true;
#if CONFIG_APP_CAMERA_OV5647_DRIVER
    built.cameraCapture = true;
#endif
    // Start camera and USB before ESP-Hosted / AP / STA, so UVC works offline.
    if (cameraProbeResult.state == camera_hub::CameraProbeState::FrameCaptured) {
        const auto cameraResult = cameraProbe.startPreview(cameraProbeResult);
        if (cameraResult != ESP_OK)
            ESP_LOGW("camera_hub", "Persistent camera preview failed: %s",
                     esp_err_to_name(cameraResult));
    }
    usb_uvc_service::UsbUvcService usbUvc(cameraProbe);
#if CONFIG_APP_USB_UVC_ENABLE
    if (cameraProbe.captureStatus().running) {
        const auto usbResult = usbUvc.start();
        if (usbResult != ESP_OK)
            ESP_LOGW("camera_hub", "USB UVC init failed: %s", esp_err_to_name(usbResult));
    }
#endif
    const board_capabilities::CapabilityRegistry capabilities(board.profile(), built);
    const board_capabilities::RuntimeState runtime{};
    ESP_LOGI("camera_hub", "Camera capture and USB start independently of setup AP");
    for (std::size_t index = 0; index < board_capabilities::kFeatureCount; ++index) {
        const auto feature = static_cast<board_capabilities::Feature>(index);
        ESP_LOGI("camera_hub", "%s=%s", capabilities.featureKey(feature),
                 capabilities.stateKey(capabilities.get(feature, runtime)));
    }
    // The default dev profile authorizes the known P4<->C6 SDIO wiring and an
    // OPEN setup AP. The actual active sdkconfig may retain older "n" values.
    connectivity::WifiService wifi;
    app_services::NetworkProvisioner provisioner(credentialStore, wifi);
    app_services::NetworkScanService scanService(wifi);
    camera_stream::CameraNetworkStream networkStreams(cameraProbe);
    web_api::ProvisioningApi api(provisioner, scanService, board.profile(), built,
                                 cameraProbeResult, cameraProbe, networkStreams);
    discovery::MdnsService mdns;
    camera_hub::ProvisioningPrerequisites prerequisites{};
    prerequisites.credentialStoreReady = credentialStore.ready();
#if CONFIG_APP_WIFI_WIRING_VERIFIED
    prerequisites.boardWiringVerified = true;
#endif
#if CONFIG_APP_C6_STORAGE_AUDITED
    prerequisites.companionStorageAudited = true;
#endif
#if CONFIG_APP_DEV_OPEN_SETUP_AP
    prerequisites.localOpenApAuthorized = true;
#endif
    ESP_LOGI("camera_hub",
             "Wi-Fi gates: wiring_verified=%d open_ap_authorized=%d store_ready=%d companion_storage_audited=%d",
             static_cast<int>(prerequisites.boardWiringVerified),
             static_cast<int>(prerequisites.localOpenApAuthorized),
             static_cast<int>(prerequisites.credentialStoreReady),
             static_cast<int>(prerequisites.companionStorageAudited));
    if (!prerequisites.boardWiringVerified || !prerequisites.localOpenApAuthorized) {
        ESP_LOGE("camera_hub",
                 "Wi-Fi blocked by firmware configuration: enable APP_WIFI_WIRING_VERIFIED "
                 "and APP_DEV_OPEN_SETUP_AP in the active dev sdkconfig; no C6 handshake attempted");
    } else {
        ESP_LOGW("camera_hub",
                 "Development setup AP is OPEN; use only in a controlled lab, not production");
    }
    const auto wifiResult = wifi.startProvisioning(prerequisites, "CAMERA-HUB-SETUP");
    if (wifiResult != ESP_OK) {
        ESP_LOGW("camera_hub", "Provisioning start refused: %s", esp_err_to_name(wifiResult));
        keepUsbAliveWithoutNetwork(usbUvc);
    }
    if (!wifi.waitForAp(pdMS_TO_TICKS(5000)) || wifi.apAddress() == 0) {
        ESP_LOGW("camera_hub", "Provisioning AP did not become ready");
        keepUsbAliveWithoutNetwork(usbUvc);
    }
    if (api.start(prerequisites, wifi.apAddress()) != ESP_OK) {
        ESP_LOGW("camera_hub", "Provisioning AP or HTTP start failed");
        keepUsbAliveWithoutNetwork(usbUvc);
    }
    std::int64_t deadline = 0;
    std::int64_t scanDeadline = 0;
    std::int64_t apStopDeadline = 0;
    std::uint32_t scanOperationId = 0;
    bool apStopped = false;
    std::uint32_t streamAddress = 0;
    std::uint8_t streamAttempts = 0;
    std::int64_t nextStreamAttempt = 0;
    if (prerequisites.companionStorageAudited) {
        const auto restoreResult = provisioner.restoreSavedCredentials();
        if (restoreResult == app_services::RestoreResult::Started) {
            deadline = esp_timer_get_time() + 30000000;
            ESP_LOGI("camera_hub", "Stored Wi-Fi credentials loaded; waiting for station IP");
        } else if (restoreResult != app_services::RestoreResult::NoCredentials) {
            ESP_LOGW("camera_hub", "Stored Wi-Fi credentials could not be restored (status=%u)",
                     static_cast<unsigned int>(restoreResult));
        }
    } else {
        ESP_LOGW("camera_hub", "STA restore disabled until companion credential storage is audited");
    }
    // mDNS runs on the ESP32-P4 TCP/IP stack over the existing C6 SDIO
    // transport. The mDNS component listens to esp-netif IP up/down events.
    // Retry a failed initialization and update the legacy alias on AP->STA.
    std::int64_t nextMdnsRetryUs = 0;
    for (;;) {
        usbUvc.pollConnection();
        const auto mdnsAddress = provisioner.stationAddress() != 0
            ? provisioner.stationAddress() : wifi.apAddress();
        if (mdnsAddress == 0) {
            mdns.stop();
        } else if (!mdns.running() && esp_timer_get_time() >= nextMdnsRetryUs) {
            const auto result = mdns.start(camera_hub::kCameraMdnsHostname, true, 80);
            if (result != ESP_OK) {
                ESP_LOGW("camera_hub", "mDNS camera_hub.local start failed: %s",
                         esp_err_to_name(result));
                nextMdnsRetryUs = esp_timer_get_time() + 5000000;
            }
        }
        if (mdns.running()) {
            // No network reinitialization or changes to the working C6 SDIO path.
            static_cast<void>(mdns.updateFallbackAddress(mdnsAddress));
        }
        if (scanService.isScanning() && scanService.operationId() != scanOperationId) {
            scanOperationId = scanService.operationId();
            scanDeadline = esp_timer_get_time() + 20000000;
        }
        scanService.processPending(scanDeadline != 0 && esp_timer_get_time() >= scanDeadline);
        const bool scanActive = scanService.isScanning();
        if (!scanActive) {
            scanOperationId = 0;
            scanDeadline = 0;
        }
        if (provisioner.snapshot().state == app_services::ProvisioningState::Queued &&
            !scanActive) {
            provisioner.processPending();
            deadline = esp_timer_get_time() + 30000000;
        }
        provisioner.updateConnection(deadline != 0 && esp_timer_get_time() >= deadline);
        const auto address = provisioner.stationAddress() != 0
            ? provisioner.stationAddress() : (apStopped ? 0 : wifi.apAddress());
        if (address != streamAddress) {
            networkStreams.stop();
            streamAddress = address;
            streamAttempts = 0;
            nextStreamAttempt = 0;
        }
        if (address != 0 && cameraProbe.captureStatus().running && !networkStreams.status().ready &&
            streamAttempts < 3 && esp_timer_get_time() >= nextStreamAttempt) {
            ++streamAttempts;
            nextStreamAttempt = esp_timer_get_time() + 5000000;
            networkStreams.stop();
            const auto result = networkStreams.start(address);
            if (result != ESP_OK) ESP_LOGW("camera_hub", "RTP/RTSP start failed: %s", esp_err_to_name(result));
        }
        if (provisioner.snapshot().state == app_services::ProvisioningState::Connected && !apStopped) {
            if (apStopDeadline == 0) apStopDeadline = esp_timer_get_time() + 5000000;
            if (esp_timer_get_time() >= apStopDeadline) {
                const auto stopResult = wifi.stopAccessPoint();
                if (stopResult == ESP_OK) {
                    apStopped = true;
                    ESP_LOGI("camera_hub", "Setup AP stopped; WebGUI available on STA IP %lu",
                             static_cast<unsigned long>(provisioner.stationAddress()));
                } else {
                    ESP_LOGW("camera_hub", "Setup AP stop failed: %s", esp_err_to_name(stopResult));
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}