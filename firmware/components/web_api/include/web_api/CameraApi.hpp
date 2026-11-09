#pragma once

#include <array>
#include <cstdint>

#include "camera_stream/CameraHardwareProbe.hpp"
#include "camera_stream/CameraNetworkStream.hpp"
#include "esp_http_server.h"

namespace web_api {

class CameraApi final {
public:
    CameraApi(camera_stream::CameraHardwareProbe& camera,
              camera_hub::CameraProbeResult probe, camera_stream::CameraNetworkStream& streams) noexcept;
    [[nodiscard]] esp_err_t handle(httpd_req_t* request, std::uint32_t peer);

private:
    [[nodiscard]] bool authorized(httpd_req_t* request, std::uint32_t peer) const noexcept;
    [[nodiscard]] esp_err_t session(httpd_req_t* request, std::uint32_t peer);
    [[nodiscard]] esp_err_t inventory(httpd_req_t* request);
    [[nodiscard]] esp_err_t configuration(httpd_req_t* request);
    [[nodiscard]] esp_err_t udp(httpd_req_t* request, std::uint32_t peer);
    [[nodiscard]] esp_err_t sdp(httpd_req_t* request, std::uint32_t peer);
    camera_stream::CameraHardwareProbe& camera_;
    camera_hub::CameraProbeResult probe_;
    camera_stream::CameraNetworkStream& streams_;
    std::array<char, 65> token_{};
    std::uint32_t peer_{0};
    std::int64_t expiresAt_{0};
    std::int64_t nextLoginAt_{0};
};

} // namespace web_api