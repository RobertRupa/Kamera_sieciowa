#pragma once

#include <array>
#include <cstdint>

#include "app_services/NetworkProvisioner.hpp"
#include "camera_hub/ProvisioningPolicy.hpp"
#include "esp_http_server.h"

namespace web_api {

class ProvisioningApi final {
public:
    explicit ProvisioningApi(app_services::NetworkProvisioner& provisioner) noexcept;
    ~ProvisioningApi();
    ProvisioningApi(const ProvisioningApi&) = delete;
    ProvisioningApi& operator=(const ProvisioningApi&) = delete;
    [[nodiscard]] esp_err_t start(const camera_hub::ProvisioningPrerequisites& prerequisites,
                                  std::uint32_t apAddress) noexcept;
    void stop() noexcept;

private:
    static esp_err_t session(httpd_req_t* request);
    static esp_err_t credentials(httpd_req_t* request);
    static esp_err_t operation(httpd_req_t* request);
    static esp_err_t error(httpd_req_t* request, const char* status, const char* code);
    bool eligible(httpd_req_t* request, std::uint32_t& peer) const noexcept;
    bool authorized(httpd_req_t* request) const noexcept;
    app_services::NetworkProvisioner& provisioner_;
    httpd_handle_t server_{nullptr};
    std::uint32_t apAddress_{0};
    std::uint32_t sessionPeer_{0};
    std::int64_t expiresAt_{0};
    std::int64_t nextSessionAt_{0};
    std::array<char, 65> token_{};
    bool credentialsEnabled_{false};
};

}