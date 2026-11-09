#include "board_support/BoardConfig.hpp"
#include "board_capabilities/CapabilityRegistry.hpp"
#include "camera_stream/CameraHardwareProbe.hpp"
#include "persistence/NvsCredentialStore.hpp"
#include "connectivity/WifiService.hpp"
#include "connectivity/HostedProbe.hpp"
#include "discovery/MdnsService.hpp"
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
    const board_capabilities::CapabilityRegistry capabilities(board.profile(), built);
    const board_capabilities::RuntimeState runtime{};
    ESP_LOGI("camera_hub", "Camera probe complete; persistent preview starts after the setup AP is ready");
    for (std::size_t index = 0; index < board_capabilities::kFeatureCount; ++index) {
        const auto feature = static_cast<board_capabilities::Feature>(index);
        ESP_LOGI("camera_hub", "%s=%s", capabilities.featureKey(feature),
                 capabilities.stateKey(capabilities.get(feature, runtime)));
    }
    ESP_LOGW("camera_hub", "Wi-Fi credential provisioning requires verified board and companion storage configuration");
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
    const auto wifiResult = wifi.startProvisioning(prerequisites, "CAMERA-HUB-SETUP");
    if (wifiResult != ESP_OK) {
        ESP_LOGW("camera_hub", "Provisioning start refused: %s", esp_err_to_name(wifiResult));
        return;
    }
    if (!wifi.waitForAp(pdMS_TO_TICKS(5000)) || wifi.apAddress() == 0) {
        ESP_LOGW("camera_hub", "Provisioning AP did not become ready");
        return;
    }
    if (cameraProbeResult.state == camera_hub::CameraProbeState::FrameCaptured) {
        const auto cameraResult = cameraProbe.startPreview(cameraProbeResult);
        if (cameraResult != ESP_OK)
            ESP_LOGW("camera_hub", "Persistent camera preview did not start: %s",
                     esp_err_to_name(cameraResult));
    }
    if (api.start(prerequisites, wifi.apAddress()) != ESP_OK) {
        ESP_LOGW("camera_hub", "Provisioning AP or HTTP start failed");
        return;
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
    const auto discoveryResult = mdns.start("camera-hub", wifi.apAddress() != 0, 80);
    if (discoveryResult != ESP_OK) ESP_LOGW("camera_hub", "mDNS start failed: %s", esp_err_to_name(discoveryResult));
    for (;;) {
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