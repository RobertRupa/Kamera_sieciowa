#pragma once

#include <array>
#include <cstdint>

#include "app_services/NetworkProvisioner.hpp"
#include "app_services/NetworkScanService.hpp"
#include "board_capabilities/CapabilityRegistry.hpp"
#include "camera_hub/CameraProbeResult.hpp"
#include "camera_hub/ProvisioningPolicy.hpp"
#include "esp_http_server.h"

namespace web_api {

class ProvisioningApi final {
public:
    ProvisioningApi(app_services::NetworkProvisioner& provisioner,
                    app_services::NetworkScanService& scanService,
                    board_capabilities::BoardProfile board,
                    board_capabilities::BuildFeatures built,
                    camera_hub::CameraProbeResult cameraProbe) noexcept;
    ~ProvisioningApi();
    ProvisioningApi(const ProvisioningApi&) = delete;
    ProvisioningApi& operator=(const ProvisioningApi&) = delete;
    [[nodiscard]] esp_err_t start(const camera_hub::ProvisioningPrerequisites& prerequisites,
                                  std::uint32_t apAddress) noexcept;
    void stop() noexcept;

private:
    static esp_err_t session(httpd_req_t* request);
    static esp_err_t status(httpd_req_t* request);
    static esp_err_t deviceStatus(httpd_req_t* request);
    static esp_err_t deviceCapabilities(httpd_req_t* request);
    static esp_err_t credentials(httpd_req_t* request);
    static esp_err_t operation(httpd_req_t* request);
    static esp_err_t scan(httpd_req_t* request);
    static esp_err_t scanOperation(httpd_req_t* request);
    static esp_err_t webAsset(httpd_req_t* request);
    static esp_err_t error(httpd_req_t* request, const char* status, const char* code);
    bool eligible(httpd_req_t* request, std::uint32_t& peer, bool allowStation = false,
                  bool* isApContext = nullptr) const noexcept;
    bool authorized(httpd_req_t* request) const noexcept;
    [[nodiscard]] board_capabilities::RuntimeState runtimeState(bool apContext) const noexcept;
    app_services::NetworkProvisioner& provisioner_;
    app_services::NetworkScanService& scanService_;
    board_capabilities::BoardProfile board_;
    board_capabilities::BuildFeatures built_;
    board_capabilities::CapabilityRegistry capabilities_;
    camera_hub::CameraProbeResult cameraProbe_;
    httpd_handle_t server_{nullptr};
    std::uint32_t apAddress_{0};
    std::uint32_t sessionPeer_{0};
    std::int64_t expiresAt_{0};
    std::int64_t nextSessionAt_{0};
    std::array<char, 65> token_{};
    bool credentialsEnabled_{false};
};

}