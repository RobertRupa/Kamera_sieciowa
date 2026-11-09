#include "web_api/ProvisioningApi.hpp"

#include <charconv>
#include <cstdio>
#include <cstring>
#include <string_view>

#include "cJSON.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "lwip/sockets.h"
extern "C" {
#include "mbedtls/constant_time.h"
}
#include "mbedtls/platform_util.h"
#include "sdkconfig.h"

#if CONFIG_CJSON_NESTING_LIMIT > 4
#error "Provisioning JSON parser nesting limit must be at most four."
#endif

namespace web_api {

ProvisioningApi::ProvisioningApi(app_services::NetworkProvisioner& provisioner) noexcept
    : provisioner_(provisioner) {}
ProvisioningApi::~ProvisioningApi() { stop(); }

esp_err_t ProvisioningApi::start(const camera_hub::ProvisioningPrerequisites& prerequisites,
                                std::uint32_t apAddress) noexcept {
    if (server_ != nullptr || apAddress == 0 || !camera_hub::ProvisioningPolicy::canStartAccessPoint(prerequisites)) {
        return ESP_ERR_INVALID_STATE;
    }
    credentialsEnabled_ = camera_hub::ProvisioningPolicy::canAcceptCredentials(prerequisites);
    apAddress_ = apAddress;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 3;
    config.max_open_sockets = 2;
    config.recv_wait_timeout = 2;
    config.send_wait_timeout = 2;
    config.uri_match_fn = httpd_uri_match_wildcard;
    auto result = httpd_start(&server_, &config);
    if (result != ESP_OK) return result;
    const httpd_uri_t routes[]{
        {"/api/v1/provisioning/session", HTTP_POST, &ProvisioningApi::session, this},
        {"/api/v1/network/credentials", HTTP_PUT, &ProvisioningApi::credentials, this},
        {"/api/v1/network/operations/*", HTTP_GET, &ProvisioningApi::operation, this},
    };
    for (const auto& route : routes) {
        result = httpd_register_uri_handler(server_, &route);
        if (result != ESP_OK) { stop(); return result; }
    }
    return ESP_OK;
}

void ProvisioningApi::stop() noexcept {
    if (server_ != nullptr) { httpd_stop(server_); server_ = nullptr; }
    mbedtls_platform_zeroize(token_.data(), token_.size());
    sessionPeer_ = 0;
    expiresAt_ = 0;
    nextSessionAt_ = 0;
}

esp_err_t ProvisioningApi::error(httpd_req_t* request, const char* status, const char* code) {
    std::array<char, 192> body{};
    std::snprintf(body.data(), body.size(), "{\"error\":{\"code\":\"%s\",\"message\":\"%s\",\"retryable\":false}}", code, code);
    httpd_resp_set_status(request, status);
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, body.data(), HTTPD_RESP_USE_STRLEN);
}

bool ProvisioningApi::eligible(httpd_req_t* request, std::uint32_t& peer) const noexcept {
    const int socket = httpd_req_to_sockfd(request);
    sockaddr_in local{}, remote{};
    socklen_t length = sizeof(local);
    if (getsockname(socket, reinterpret_cast<sockaddr*>(&local), &length) != 0 ||
        local.sin_family != AF_INET || local.sin_addr.s_addr != apAddress_) return false;
    length = sizeof(remote);
    if (getpeername(socket, reinterpret_cast<sockaddr*>(&remote), &length) != 0 || remote.sin_family != AF_INET) return false;
    peer = remote.sin_addr.s_addr;
    if (httpd_req_get_hdr_value_len(request, "Origin") != 0) {
        std::array<char, 80> origin{}, expected{};
        std::array<char, 16> address{};
        inet_ntop(AF_INET, &local.sin_addr, address.data(), address.size());
        std::snprintf(expected.data(), expected.size(), "http://%s", address.data());
        if (httpd_req_get_hdr_value_str(request, "Origin", origin.data(), origin.size()) != ESP_OK ||
            std::strcmp(origin.data(), expected.data()) != 0) return false;
    }
    return true;
}

bool ProvisioningApi::authorized(httpd_req_t* request) const noexcept {
    std::uint32_t peer = 0;
    if (!eligible(request, peer) || peer != sessionPeer_ || esp_timer_get_time() >= expiresAt_) return false;
    std::array<char, 72> header{};
    if (httpd_req_get_hdr_value_len(request, "Authorization") != 71 ||
        httpd_req_get_hdr_value_str(request, "Authorization", header.data(), header.size()) != ESP_OK) return false;
    const bool valid = std::memcmp(header.data(), "Bearer ", 7) == 0 &&
        mbedtls_ct_memcmp(header.data() + 7, token_.data(), 64) == 0;
    mbedtls_platform_zeroize(header.data(), header.size());
    return valid;
}

esp_err_t ProvisioningApi::session(httpd_req_t* request) {
    auto& api = *static_cast<ProvisioningApi*>(request->user_ctx);
    if (!api.credentialsEnabled_) return error(request, "503 Service Unavailable", "credential_storage_unavailable");
    std::uint32_t peer = 0;
    if (!api.eligible(request, peer)) return error(request, "403 Forbidden", "provisioning_context_required");
    if (request->content_len != 0) return error(request, "400 Bad Request", "unexpected_body");
    const auto now = esp_timer_get_time();
    if (now < api.nextSessionAt_) return error(request, "429 Too Many Requests", "rate_limited");
    if (now < api.expiresAt_ && api.sessionPeer_ != peer) return error(request, "409 Conflict", "session_busy");
    std::array<std::uint8_t, 32> random{};
    esp_fill_random(random.data(), random.size());
    constexpr char hex[] = "0123456789abcdef";
    for (std::size_t index = 0; index < random.size(); ++index) {
        api.token_[index * 2] = hex[random[index] >> 4];
        api.token_[index * 2 + 1] = hex[random[index] & 15];
    }
    api.token_[64] = 0;
    mbedtls_platform_zeroize(random.data(), random.size());
    api.sessionPeer_ = peer;
    api.expiresAt_ = now + 180000000;
    api.nextSessionAt_ = now + 5000000;
    std::array<char, 160> body{};
    std::snprintf(body.data(), body.size(), "{\"data\":{\"token\":\"%s\",\"expires_in_seconds\":180}}", api.token_.data());
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    const auto result = httpd_resp_send(request, body.data(), HTTPD_RESP_USE_STRLEN);
    mbedtls_platform_zeroize(body.data(), body.size());
    return result;
}

esp_err_t ProvisioningApi::credentials(httpd_req_t* request) {
    auto& api = *static_cast<ProvisioningApi*>(request->user_ctx);
    if (!api.authorized(request)) return error(request, "401 Unauthorized", "unauthorized");
    if (request->content_len == 0 || request->content_len > 384) return error(request, "400 Bad Request", "invalid_body");
    std::array<char, 40> contentType{};
    if (httpd_req_get_hdr_value_str(request, "Content-Type", contentType.data(), contentType.size()) != ESP_OK ||
        std::strcmp(contentType.data(), "application/json") != 0) return error(request, "415 Unsupported Media Type", "json_required");
    std::array<char, 385> body{};
    std::size_t received = 0;
    const auto deadline = esp_timer_get_time() + 3000000;
    while (received < request->content_len) {
        if (esp_timer_get_time() >= deadline) {
            mbedtls_platform_zeroize(body.data(), body.size());
            return error(request, "408 Request Timeout", "timeout");
        }
        const auto count = httpd_req_recv(request, body.data() + received, request->content_len - received);
        if (count <= 0) {
            mbedtls_platform_zeroize(body.data(), body.size());
            return error(request, "408 Request Timeout", "timeout");
        }
        received += count;
    }
    if (std::memchr(body.data(), 0, received) != nullptr || std::strstr(body.data(), "\\u0000") != nullptr) {
        mbedtls_platform_zeroize(body.data(), body.size());
        return error(request, "400 Bad Request", "invalid_credentials");
    }
    cJSON* json = cJSON_ParseWithLengthOpts(body.data(), received + 1, nullptr, true);
    mbedtls_platform_zeroize(body.data(), body.size());
    auto* ssid = cJSON_GetObjectItemCaseSensitive(json, "ssid");
    auto* password = cJSON_GetObjectItemCaseSensitive(json, "password");
    camera_hub::WifiCredentials credentials;
    const bool valid = cJSON_IsObject(json) && cJSON_GetArraySize(json) == 2 &&
        cJSON_IsString(ssid) && cJSON_IsString(password) &&
        camera_hub::ProvisioningPolicy::validSsid(ssid->valuestring) &&
        camera_hub::ProvisioningPolicy::validPassphrase(password->valuestring);
    if (valid) {
        std::memcpy(credentials.ssid.data(), ssid->valuestring, std::strlen(ssid->valuestring));
        std::memcpy(credentials.passphrase.data(), password->valuestring, std::strlen(password->valuestring));
    }
    if (cJSON_IsString(password)) mbedtls_platform_zeroize(password->valuestring, std::strlen(password->valuestring));
    if (cJSON_IsString(ssid)) mbedtls_platform_zeroize(ssid->valuestring, std::strlen(ssid->valuestring));
    cJSON_Delete(json);
    if (!valid) return error(request, "400 Bad Request", "invalid_credentials");
    std::uint32_t operationId = 0;
    const auto submitted = api.provisioner_.submit(credentials, operationId);
    if (submitted != app_services::SubmitResult::Accepted) {
        return error(request, submitted == app_services::SubmitResult::Busy ? "409 Conflict" : "503 Service Unavailable", "provisioning_unavailable");
    }
    credentials.clear();
    std::array<char, 96> response{};
    std::snprintf(response.data(), response.size(), "{\"data\":{\"operation_id\":\"wifi-%u\"}}", static_cast<unsigned int>(operationId));
    httpd_resp_set_status(request, "202 Accepted");
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, response.data(), HTTPD_RESP_USE_STRLEN);
}

esp_err_t ProvisioningApi::operation(httpd_req_t* request) {
    auto& api = *static_cast<ProvisioningApi*>(request->user_ctx);
    if (!api.authorized(request)) return error(request, "401 Unauthorized", "unauthorized");
    constexpr std::string_view prefix = "/api/v1/network/operations/wifi-";
    const std::string_view uri(request->uri);
    if (uri.substr(0, prefix.size()) != prefix) return error(request, "404 Not Found", "operation_not_found");
    std::uint32_t id = 0;
    const auto parsed = std::from_chars(uri.data() + prefix.size(), uri.data() + uri.size(), id);
    const auto snapshot = api.provisioner_.snapshot();
    if (parsed.ec != std::errc{} || parsed.ptr != uri.data() + uri.size() || id == 0 || id != snapshot.operationId) {
        return error(request, "404 Not Found", "operation_not_found");
    }
    constexpr const char* states[]{"idle", "queued", "persisting", "connecting", "connected", "failed"};
    std::array<char, 128> body{};
    std::snprintf(body.data(), body.size(), "{\"data\":{\"operation_id\":\"wifi-%u\",\"state\":\"%s\"}}",
                  static_cast<unsigned int>(id), states[static_cast<std::size_t>(snapshot.state)]);
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, body.data(), HTTPD_RESP_USE_STRLEN);
}

}