#include "board_support/BoardConfig.hpp"
#include "board_capabilities/CapabilityRegistry.hpp"
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
    const board_capabilities::CapabilityRegistry capabilities(board.profile(), {});
    const board_capabilities::RuntimeState runtime{};
    ESP_LOGW("camera_hub", "Diagnostic foundation only: camera, UVC and IP streaming unavailable");
    for (std::size_t index = 0; index < board_capabilities::kFeatureCount; ++index) {
        const auto feature = static_cast<board_capabilities::Feature>(index);
        ESP_LOGI("camera_hub", "%s=%s", capabilities.featureKey(feature),
                 capabilities.stateKey(capabilities.get(feature, runtime)));
    }
    ESP_LOGW("camera_hub", "Wi-Fi credential provisioning requires verified board and companion storage configuration");
    connectivity::WifiService wifi;
    app_services::NetworkProvisioner provisioner(credentialStore, wifi);
    web_api::ProvisioningApi api(provisioner);
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
    if (!wifi.waitForAp(pdMS_TO_TICKS(5000)) || wifi.apAddress() == 0 ||
        api.start(prerequisites, wifi.apAddress()) != ESP_OK) {
        ESP_LOGW("camera_hub", "Provisioning AP or HTTP start failed");
        return;
    }
    std::int64_t deadline = 0;
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
        if (provisioner.snapshot().state == app_services::ProvisioningState::Queued) {
            provisioner.processPending();
            deadline = esp_timer_get_time() + 30000000;
        }
        provisioner.updateConnection(deadline != 0 && esp_timer_get_time() >= deadline);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}