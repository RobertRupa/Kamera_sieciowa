#include "web_api/ProvisioningApi.hpp"

#include <charconv>
#include <cstdio>
#include <cstring>
#include <string_view>

#include "cJSON.h"
#include "wifi_web_assets.hpp"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_random.h"
#include "esp_log.h"
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

namespace {

constexpr char kTag[] = "provisioning_api";

bool socketIpv4Address(const sockaddr_storage& socketAddress, in_addr& address) noexcept {
    if (socketAddress.ss_family == AF_INET) {
        address = reinterpret_cast<const sockaddr_in*>(&socketAddress)->sin_addr;
        return true;
    }
    if (socketAddress.ss_family != AF_INET6) return false;

    const auto* bytes = reinterpret_cast<const std::uint8_t*>(
        &reinterpret_cast<const sockaddr_in6*>(&socketAddress)->sin6_addr);
    for (std::size_t index = 0; index < 10; ++index) {
        if (bytes[index] != 0) return false;
    }
    if (bytes[10] != 0xff || bytes[11] != 0xff) return false;
    std::memcpy(&address.s_addr, bytes + 12, sizeof(address.s_addr));
    return true;
}

const char* scanStateName(app_services::NetworkScanState state) noexcept {
    switch (state) {
    case app_services::NetworkScanState::Idle: return "idle";
    case app_services::NetworkScanState::Scanning: return "scanning";
    case app_services::NetworkScanState::Complete: return "complete";
    case app_services::NetworkScanState::Failed: return "failed";
    }
    return "failed";
}

const char* securityName(camera_hub::WifiSecurity security) noexcept {
    using camera_hub::WifiSecurity;
    switch (security) {
    case WifiSecurity::Open: return "open";
    case WifiSecurity::Wep: return "wep";
    case WifiSecurity::WpaPsk: return "wpa_psk";
    case WifiSecurity::Wpa2Psk: return "wpa2_psk";
    case WifiSecurity::WpaWpa2Psk: return "wpa_wpa2_psk";
    case WifiSecurity::Enterprise: return "enterprise";
    case WifiSecurity::Wpa3Psk: return "wpa3_psk";
    case WifiSecurity::Wpa2Wpa3Psk: return "wpa2_wpa3_psk";
    case WifiSecurity::Owe: return "owe";
    case WifiSecurity::Unknown: return "unknown";
    }
    return "unknown";
}

char* serializeScanSnapshot(const app_services::NetworkScanSnapshot& snapshot) {
    cJSON* root = cJSON_CreateObject();
    if (root == nullptr) return nullptr;
    cJSON* data = cJSON_AddObjectToObject(root, "data");
    cJSON* results = data == nullptr ? nullptr : cJSON_AddArrayToObject(data, "results");
    if (data == nullptr || results == nullptr) {
        cJSON_Delete(root);
        return nullptr;
    }

    std::array<char, 32> operationId{};
    std::snprintf(operationId.data(), operationId.size(), "scan-%u",
                  static_cast<unsigned int>(snapshot.operationId));
    if (cJSON_AddStringToObject(data, "operation_id", operationId.data()) == nullptr ||
        cJSON_AddStringToObject(data, "state", scanStateName(snapshot.state)) == nullptr) {
        cJSON_Delete(root);
        return nullptr;
    }
    if (snapshot.state == app_services::NetworkScanState::Failed &&
        cJSON_AddStringToObject(data, "error", "scan_failed") == nullptr) {
        cJSON_Delete(root);
        return nullptr;
    }
    for (std::size_t index = 0; index < snapshot.resultCount; ++index) {
        const auto& network = snapshot.results[index];
        cJSON* item = cJSON_CreateObject();
        if (item == nullptr || cJSON_AddStringToObject(item, "ssid", network.ssid.data()) == nullptr ||
            cJSON_AddNumberToObject(item, "rssi", network.rssi) == nullptr ||
            cJSON_AddStringToObject(item, "security", securityName(network.security)) == nullptr) {
            cJSON_Delete(item);
            cJSON_Delete(root);
            return nullptr;
        }
        cJSON_AddItemToArray(results, item);
    }
    char* body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return body;
}

char* serializeDeviceCapabilities(const board_capabilities::BoardProfile& board,
                                  const board_capabilities::CapabilityRegistry& registry,
                                  const board_capabilities::RuntimeState& runtime) {
    cJSON* root = cJSON_CreateObject();
    if (root == nullptr) return nullptr;
    cJSON* data = cJSON_AddObjectToObject(root, "data");
    cJSON* capabilities = data == nullptr ? nullptr : cJSON_AddObjectToObject(data, "capabilities");
    if (data == nullptr || capabilities == nullptr || board.boardId == nullptr || board.idfTarget == nullptr ||
        cJSON_AddStringToObject(data, "board_id", board.boardId) == nullptr ||
        cJSON_AddStringToObject(data, "target", board.idfTarget) == nullptr) {
        cJSON_Delete(root);
        return nullptr;
    }

    const auto states = registry.snapshot(runtime);
    for (std::size_t index = 0; index < states.size(); ++index) {
        const auto feature = static_cast<board_capabilities::Feature>(index);
        if (cJSON_AddStringToObject(capabilities, board_capabilities::CapabilityRegistry::featureKey(feature),
                                    board_capabilities::CapabilityRegistry::stateKey(states[index])) == nullptr) {
            cJSON_Delete(root);
            return nullptr;
        }
    }
    constexpr std::array<std::pair<const char*, const char*>, 4> outputs{{
        {"usb_uvc", "unverified"},
        {"http_mjpeg", "disabled"},
        {"rtsp", "disabled"},
        {"ethernet", "unverified"},
    }};
    for (const auto& output : outputs) {
        if (cJSON_AddStringToObject(capabilities, output.first, output.second) == nullptr) {
            cJSON_Delete(root);
            return nullptr;
        }
    }

    char* body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return body;
}

} // namespace

ProvisioningApi::ProvisioningApi(app_services::NetworkProvisioner& provisioner,
                                 app_services::NetworkScanService& scanService,
                                 board_capabilities::BoardProfile board,
                                                                 board_capabilities::BuildFeatures built,
                                                                 camera_hub::CameraProbeResult cameraProbe) noexcept
    : provisioner_(provisioner), scanService_(scanService), board_(board), built_(built),
            capabilities_(board_, built_), cameraProbe_(cameraProbe) {}
ProvisioningApi::~ProvisioningApi() { stop(); }

esp_err_t ProvisioningApi::start(const camera_hub::ProvisioningPrerequisites& prerequisites,
                                std::uint32_t apAddress) noexcept {
    if (server_ != nullptr || apAddress == 0 || !camera_hub::ProvisioningPolicy::canStartAccessPoint(prerequisites)) {
        return ESP_ERR_INVALID_STATE;
    }
    credentialsEnabled_ = camera_hub::ProvisioningPolicy::canAcceptCredentials(prerequisites);
    apAddress_ = apAddress;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 12;
    config.max_open_sockets = 4;
    config.recv_wait_timeout = 2;
    config.send_wait_timeout = 2;
    config.uri_match_fn = httpd_uri_match_wildcard;
    auto result = httpd_start(&server_, &config);
    if (result != ESP_OK) return result;
    const httpd_uri_t routes[]{
        {"/", HTTP_GET, &ProvisioningApi::webAsset, this},
        {"/assets/*", HTTP_GET, &ProvisioningApi::webAsset, this},
        {"/favicon.svg", HTTP_GET, &ProvisioningApi::webAsset, this},
        {"/icons.svg", HTTP_GET, &ProvisioningApi::webAsset, this},
        {"/api/v1/provisioning/status", HTTP_GET, &ProvisioningApi::status, this},
        {"/api/v1/device/status", HTTP_GET, &ProvisioningApi::deviceStatus, this},
        {"/api/v1/device/capabilities", HTTP_GET, &ProvisioningApi::deviceCapabilities, this},
        {"/api/v1/provisioning/session", HTTP_POST, &ProvisioningApi::session, this},
        {"/api/v1/network/credentials", HTTP_PUT, &ProvisioningApi::credentials, this},
        {"/api/v1/network/operations/*", HTTP_GET, &ProvisioningApi::operation, this},
        {"/api/v1/network/scan", HTTP_POST, &ProvisioningApi::scan, this},
        {"/api/v1/network/scan/*", HTTP_GET, &ProvisioningApi::scanOperation, this},
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

esp_err_t ProvisioningApi::webAsset(httpd_req_t* request) {
    auto& api = *static_cast<ProvisioningApi*>(request->user_ctx);
    std::uint32_t peer = 0;
    if (!api.eligible(request, peer, true)) return error(request, "403 Forbidden", "provisioning_context_required");

    const std::string_view requestedPath(request->uri);
    const std::string_view assetPath = requestedPath == "/" ? "/index.html" : requestedPath;
    for (const auto& asset : kWebAssets) {
        if (assetPath != asset.path) continue;
        httpd_resp_set_type(request, asset.contentType);
        httpd_resp_set_hdr(request, "Content-Encoding", "gzip");
        httpd_resp_set_hdr(request, "Cache-Control",
                           assetPath == "/index.html" ? "no-cache" : "public, max-age=31536000, immutable");
        const auto size = static_cast<std::size_t>(asset.end - asset.start);
        return httpd_resp_send(request, reinterpret_cast<const char*>(asset.start), size);
    }
    return error(request, "404 Not Found", "asset_not_found");
}

bool ProvisioningApi::eligible(httpd_req_t* request, std::uint32_t& peer, bool allowStation,
                              bool* isApContext) const noexcept {
    const int socket = httpd_req_to_sockfd(request);
    sockaddr_storage local{}, remote{};
    in_addr localIp{}, remoteIp{};
    socklen_t length = sizeof(local);
    if (getsockname(socket, reinterpret_cast<sockaddr*>(&local), &length) != 0 ||
        !socketIpv4Address(local, localIp)) {
        ESP_LOGW(kTag, "request rejected: unsupported local socket family=%d", local.ss_family);
        return false;
    }
    const bool apContext = localIp.s_addr == apAddress_;
    const auto stationAddress = allowStation ? provisioner_.stationAddress() : 0;
    if (!apContext && (stationAddress == 0 || localIp.s_addr != stationAddress)) {
        std::array<char, 16> localAddressText{}, apAddressText{};
        inet_ntop(AF_INET, &localIp, localAddressText.data(), localAddressText.size());
        in_addr expectedAddress{};
        expectedAddress.s_addr = apAddress_;
        inet_ntop(AF_INET, &expectedAddress, apAddressText.data(), apAddressText.size());
        ESP_LOGW(kTag, "request rejected: destination=%s AP=%s", localAddressText.data(), apAddressText.data());
        return false;
    }
    if (isApContext != nullptr) *isApContext = apContext;
    length = sizeof(remote);
    if (getpeername(socket, reinterpret_cast<sockaddr*>(&remote), &length) != 0 ||
        !socketIpv4Address(remote, remoteIp)) {
        ESP_LOGW(kTag, "request rejected: unsupported peer socket family=%d", remote.ss_family);
        return false;
    }
    peer = remoteIp.s_addr;
    if (httpd_req_get_hdr_value_len(request, "Origin") != 0) {
        std::array<char, 80> origin{}, expected{};
        std::array<char, 16> address{};
        inet_ntop(AF_INET, &localIp, address.data(), address.size());
        std::snprintf(expected.data(), expected.size(), "http://%s", address.data());
        if (httpd_req_get_hdr_value_str(request, "Origin", origin.data(), origin.size()) != ESP_OK ||
            std::strcmp(origin.data(), expected.data()) != 0) {
            ESP_LOGW(kTag, "request rejected: origin=%s expected=%s", origin.data(), expected.data());
            return false;
        }
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

esp_err_t ProvisioningApi::status(httpd_req_t* request) {
    auto& api = *static_cast<ProvisioningApi*>(request->user_ctx);
    std::uint32_t peer = 0;
    bool isApContext = false;
    if (!api.eligible(request, peer, true, &isApContext)) return error(request, "403 Forbidden", "provisioning_context_required");
    std::array<char, 16> stationIp{};
    std::array<char, 24> stationIpJson{};
    const auto stationAddress = api.provisioner_.stationAddress();
    if (stationAddress != 0 && inet_ntop(AF_INET, &stationAddress, stationIp.data(), stationIp.size()) != nullptr) {
        std::snprintf(stationIpJson.data(), stationIpJson.size(), "\"%s\"", stationIp.data());
    } else {
        std::snprintf(stationIpJson.data(), stationIpJson.size(), "null");
    }
    std::array<char, 256> body{};
    std::snprintf(body.data(), body.size(),
                  "{\"data\":{\"wifi\":\"%s\",\"wifi_wizard\":\"%s\",\"credential_storage\":\"%s\",\"station_ip\":%s,\"provisioning_available\":%s}}",
                  api.server_ != nullptr ? "ready" : "not_ready",
                  api.server_ != nullptr && api.apAddress_ != 0 ? "ready" : "not_ready",
                  api.credentialsEnabled_ ? "ready" : "not_ready", stationIpJson.data(),
                  isApContext ? "true" : "false");
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, body.data(), HTTPD_RESP_USE_STRLEN);
}

board_capabilities::RuntimeState ProvisioningApi::runtimeState(bool apContext) const noexcept {
    board_capabilities::RuntimeState runtime{};
    runtime.wifiLinkReady = apContext || provisioner_.stationAddress() != 0;
    runtime.wizardRunning = apContext && credentialsEnabled_;
    runtime.ipReachable = server_ != nullptr;
    runtime.webServerRunning = server_ != nullptr;
    runtime.credentialStorageReady = credentialsEnabled_;
    return runtime;
}

esp_err_t ProvisioningApi::deviceStatus(httpd_req_t* request) {
    auto& api = *static_cast<ProvisioningApi*>(request->user_ctx);
    std::uint32_t peer = 0;
    if (!api.eligible(request, peer, true)) return error(request, "403 Forbidden", "provisioning_context_required");
#if !CONFIG_APP_DEV_MODE
    return error(request, "503 Service Unavailable", "device_status_requires_development_profile");
#else
    const auto* description = esp_app_get_description();
    cJSON* root = cJSON_CreateObject();
    if (root == nullptr) return error(request, "500 Internal Server Error", "response_unavailable");
    cJSON* data = cJSON_AddObjectToObject(root, "data");
    if (data == nullptr ||
        cJSON_AddStringToObject(data, "firmware_version", description == nullptr ? "unknown" : description->version) == nullptr ||
        cJSON_AddNumberToObject(data, "uptime_seconds", static_cast<double>(esp_timer_get_time() / 1000000)) == nullptr ||
        cJSON_AddNumberToObject(data, "free_heap_bytes", static_cast<double>(esp_get_free_heap_size())) == nullptr ||
        cJSON_AddNullToObject(data, "camera") == nullptr ||
        cJSON_AddStringToObject(data, "camera_probe_state",
                                camera_hub::cameraProbeStateKey(api.cameraProbe_.state)) == nullptr) {
        cJSON_Delete(root);
        return error(request, "500 Internal Server Error", "response_unavailable");
    }
    const char* sensorName = camera_hub::cameraSensorName(api.cameraProbe_.sensor);
    if (sensorName == nullptr) {
        if (cJSON_AddNullToObject(data, "camera_hardware") == nullptr) {
            cJSON_Delete(root);
            return error(request, "500 Internal Server Error", "response_unavailable");
        }
    } else {
        cJSON* hardware = cJSON_AddObjectToObject(data, "camera_hardware");
        if (hardware == nullptr || cJSON_AddStringToObject(hardware, "sensor", sensorName) == nullptr ||
            cJSON_AddNullToObject(hardware, "module") == nullptr ||
            cJSON_AddNullToObject(hardware, "module_revision") == nullptr) {
            cJSON_Delete(root);
            return error(request, "500 Internal Server Error", "response_unavailable");
        }
    }
    char* body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (body == nullptr) return error(request, "500 Internal Server Error", "response_unavailable");
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    const auto result = httpd_resp_send(request, body, HTTPD_RESP_USE_STRLEN);
    cJSON_free(body);
    return result;
#endif
}

esp_err_t ProvisioningApi::deviceCapabilities(httpd_req_t* request) {
    auto& api = *static_cast<ProvisioningApi*>(request->user_ctx);
    std::uint32_t peer = 0;
    bool isApContext = false;
    if (!api.eligible(request, peer, true, &isApContext)) return error(request, "403 Forbidden", "provisioning_context_required");
#if !CONFIG_APP_DEV_MODE
    return error(request, "503 Service Unavailable", "device_status_requires_development_profile");
#else
    char* body = serializeDeviceCapabilities(api.board_, api.capabilities_, api.runtimeState(isApContext));
    if (body == nullptr) return error(request, "500 Internal Server Error", "response_unavailable");
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    const auto result = httpd_resp_send(request, body, HTTPD_RESP_USE_STRLEN);
    cJSON_free(body);
    return result;
#endif
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
    std::array<char, 16> stationAddress{};
    std::array<char, 24> stationAddressJson{};
    if (snapshot.stationAddress != 0 &&
        inet_ntop(AF_INET, &snapshot.stationAddress, stationAddress.data(), stationAddress.size()) != nullptr) {
        std::snprintf(stationAddressJson.data(), stationAddressJson.size(), "\"%s\"", stationAddress.data());
    } else {
        std::snprintf(stationAddressJson.data(), stationAddressJson.size(), "null");
    }
    std::array<char, 192> body{};
    std::snprintf(body.data(), body.size(),
                  "{\"data\":{\"operation_id\":\"wifi-%u\",\"state\":\"%s\",\"station_ip\":%s}}",
                  static_cast<unsigned int>(id), states[static_cast<std::size_t>(snapshot.state)], stationAddressJson.data());
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, body.data(), HTTPD_RESP_USE_STRLEN);
}

esp_err_t ProvisioningApi::scan(httpd_req_t* request) {
    auto& api = *static_cast<ProvisioningApi*>(request->user_ctx);
    if (!api.authorized(request)) {
        ESP_LOGW(kTag, "scan request rejected: unauthorized provisioning session");
        return error(request, "401 Unauthorized", "unauthorized");
    }
    if (request->content_len != 0) return error(request, "400 Bad Request", "unexpected_body");

    std::uint32_t operationId = 0;
    const auto started = api.scanService_.start(operationId);
    if (started != app_services::NetworkScanStartResult::Accepted) {
        ESP_LOGW(kTag, "scan start rejected: result=%u", static_cast<unsigned int>(started));
        if (started == app_services::NetworkScanStartResult::Busy)
            return error(request, "409 Conflict", "scan_busy");
        if (started == app_services::NetworkScanStartResult::NotReady)
            return error(request, "503 Service Unavailable", "wifi_not_ready");
        return error(request, "503 Service Unavailable", "scan_unavailable");
    }
    ESP_LOGI(kTag, "scan started: operation=%u", static_cast<unsigned int>(operationId));

    std::array<char, 64> body{};
    std::snprintf(body.data(), body.size(), "{\"data\":{\"operation_id\":\"scan-%u\"}}",
                  static_cast<unsigned int>(operationId));
    httpd_resp_set_status(request, "202 Accepted");
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, body.data(), HTTPD_RESP_USE_STRLEN);
}

esp_err_t ProvisioningApi::scanOperation(httpd_req_t* request) {
    auto& api = *static_cast<ProvisioningApi*>(request->user_ctx);
    if (!api.authorized(request)) return error(request, "401 Unauthorized", "unauthorized");
    constexpr std::string_view prefix = "/api/v1/network/scan/";
    const std::string_view uri(request->uri);
    if (uri.substr(0, prefix.size()) != prefix) return error(request, "404 Not Found", "scan_not_found");
    std::uint32_t id = 0;
    const auto parsed = std::from_chars(uri.data() + prefix.size(), uri.data() + uri.size(), id);
    const auto snapshot = api.scanService_.snapshot();
    if (parsed.ec != std::errc{} || parsed.ptr != uri.data() + uri.size() || id == 0 || id != snapshot.operationId) {
        return error(request, "404 Not Found", "scan_not_found");
    }
    char* body = serializeScanSnapshot(snapshot);
    if (body == nullptr) return error(request, "500 Internal Server Error", "response_unavailable");
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    const auto result = httpd_resp_send(request, body, HTTPD_RESP_USE_STRLEN);
    cJSON_free(body);
    return result;
}

}