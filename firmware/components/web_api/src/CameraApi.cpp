#include "web_api/CameraApi.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string_view>

#include "cJSON.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "lwip/sockets.h"
#include "mbedtls/platform_util.h"
extern "C" {
#include "mbedtls/constant_time.h"
}
#include "sdkconfig.h"

namespace web_api {
namespace {

#if CONFIG_APP_DEV_MODE && CONFIG_APP_DEV_DEFAULT_CREDENTIALS
constexpr bool kDevelopmentLogin = true;
#else
constexpr bool kDevelopmentLogin = false;
#endif

esp_err_t failure(httpd_req_t* request, const char* status, const char* code) {
    std::array<char, 192> body{};
    std::snprintf(body.data(), body.size(),
                  "{\"error\":{\"code\":\"%s\",\"message\":\"%s\",\"retryable\":false}}", code, code);
    httpd_resp_set_status(request, status);
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, body.data(), HTTPD_RESP_USE_STRLEN);
}

esp_err_t sendJson(httpd_req_t* request, cJSON* root, bool valid) {
    char* body = valid && root != nullptr ? cJSON_PrintUnformatted(root) : nullptr;
    cJSON_Delete(root);
    if (body == nullptr) return failure(request, "500 Internal Server Error", "response_unavailable");
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    const auto result = httpd_resp_send(request, body, HTTPD_RESP_USE_STRLEN);
    cJSON_free(body);
    return result;
}

cJSON* readJson(httpd_req_t* request) {
    if (request->content_len <= 0 || request->content_len > 256) return nullptr;
    std::array<char, 64> contentType{};
    if (httpd_req_get_hdr_value_str(request, "Content-Type", contentType.data(), contentType.size()) != ESP_OK ||
        (std::strcmp(contentType.data(), "application/json") != 0 &&
         std::strcmp(contentType.data(), "application/json; charset=utf-8") != 0)) return nullptr;
    std::array<char, 257> body{};
    std::size_t received = 0;
    const auto deadline = esp_timer_get_time() + 3000000;
    while (received < request->content_len && esp_timer_get_time() < deadline) {
        const auto count = httpd_req_recv(request, body.data() + received, request->content_len - received);
        if (count <= 0) break;
        received += count;
    }
    cJSON* root = nullptr;
    if (received == request->content_len && std::memchr(body.data(), 0, received) == nullptr &&
        std::string_view(body.data(), received).find("\\u0000") == std::string_view::npos)
        root = cJSON_ParseWithLengthOpts(body.data(), received + 1, nullptr, true);
    mbedtls_platform_zeroize(body.data(), body.size());
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return nullptr;
    }
    return root;
}

bool integer(const cJSON* value, double minimum, double maximum) {
    return cJSON_IsNumber(value) && std::isfinite(value->valuedouble) &&
           std::floor(value->valuedouble) == value->valuedouble &&
           value->valuedouble >= minimum && value->valuedouble <= maximum;
}

const char* configurationState(camera_hub::ConfigurationState state) {
    switch (state) {
    case camera_hub::ConfigurationState::Idle: return "idle";
    case camera_hub::ConfigurationState::Queued: return "queued";
    case camera_hub::ConfigurationState::Applying: return "applying";
    case camera_hub::ConfigurationState::Applied: return "applied";
    case camera_hub::ConfigurationState::Failed: return "failed";
    }
    return "failed";
}

} // namespace

CameraApi::CameraApi(camera_stream::CameraHardwareProbe& camera,
                     camera_hub::CameraProbeResult probe,
                     camera_stream::CameraNetworkStream& streams) noexcept
    : camera_(camera), probe_(probe), streams_(streams) {}

bool CameraApi::authorized(httpd_req_t* request, std::uint32_t peer) const noexcept {
    if (peer != peer_ || esp_timer_get_time() >= expiresAt_) return false;
    std::array<char, 72> header{};
    if (httpd_req_get_hdr_value_len(request, "Authorization") != 71 ||
        httpd_req_get_hdr_value_str(request, "Authorization", header.data(), header.size()) != ESP_OK) return false;
    const bool valid = std::memcmp(header.data(), "Bearer ", 7) == 0 &&
        mbedtls_ct_memcmp(header.data() + 7, token_.data(), 64) == 0;
    mbedtls_platform_zeroize(header.data(), header.size());
    return valid;
}

esp_err_t CameraApi::handle(httpd_req_t* request, std::uint32_t peer) {
#if !CONFIG_APP_DEV_MODE
    return failure(request, "503 Service Unavailable", "camera_development_only");
#else
    if (std::strcmp(request->uri, "/api/v1/camera/session") == 0) return session(request, peer);
    if (request->method != HTTP_GET && !authorized(request, peer))
        return failure(request, "401 Unauthorized", "unauthorized");
    if (std::strcmp(request->uri, "/api/v1/cameras") == 0) return inventory(request);
    if (std::strcmp(request->uri, "/api/v1/cameras/csi-0/udp") == 0) return udp(request, peer);
    if (std::strcmp(request->uri, "/api/v1/cameras/csi-0/stream.sdp") == 0) return sdp(request, peer);
    return configuration(request);
#endif
}

esp_err_t CameraApi::session(httpd_req_t* request, std::uint32_t peer) {
#if !CONFIG_APP_DEV_MODE || !CONFIG_APP_DEV_DEFAULT_CREDENTIALS
    return failure(request, "503 Service Unavailable", "camera_login_disabled");
#else
    if (request->method == HTTP_DELETE) {
        if (!authorized(request, peer)) return failure(request, "401 Unauthorized", "unauthorized");
        const auto network = streams_.status();
        if (network.receiver == peer)
            static_cast<void>(streams_.configureDirect(peer, network.port, false));
        mbedtls_platform_zeroize(token_.data(), token_.size());
        expiresAt_ = 0;
        httpd_resp_set_status(request, "204 No Content");
        return httpd_resp_send(request, nullptr, 0);
    }
    const auto now = esp_timer_get_time();
    if (now < nextLoginAt_) return failure(request, "429 Too Many Requests", "login_rate_limited");
    nextLoginAt_ = now + 5000000;
    cJSON* root = readJson(request);
    cJSON* username = cJSON_GetObjectItemCaseSensitive(root, "username");
    cJSON* password = cJSON_GetObjectItemCaseSensitive(root, "password");
    const bool valid = root != nullptr && cJSON_GetArraySize(root) == 2 &&
        cJSON_IsString(username) && cJSON_IsString(password) &&
        std::strlen(username->valuestring) == 5 && std::strlen(password->valuestring) == 5 &&
        mbedtls_ct_memcmp(username->valuestring, "admin", 5) == 0 &&
        mbedtls_ct_memcmp(password->valuestring, "admin", 5) == 0;
    if (cJSON_IsString(password)) mbedtls_platform_zeroize(password->valuestring, std::strlen(password->valuestring));
    cJSON_Delete(root);
    if (!valid) return failure(request, "401 Unauthorized", "unauthorized");
    if (now < expiresAt_ && peer != peer_) return failure(request, "409 Conflict", "session_busy");
    std::array<std::uint8_t, 32> random{};
    esp_fill_random(random.data(), random.size());
    constexpr char hex[] = "0123456789abcdef";
    for (std::size_t index = 0; index < random.size(); ++index) {
        token_[index * 2] = hex[random[index] >> 4];
        token_[index * 2 + 1] = hex[random[index] & 15];
    }
    mbedtls_platform_zeroize(random.data(), random.size());
    token_[64] = 0;
    peer_ = peer;
    expiresAt_ = now + 900000000;
    std::array<char, 160> body{};
    std::snprintf(body.data(), body.size(), "{\"data\":{\"token\":\"%s\",\"expires_in_seconds\":900}}", token_.data());
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    const auto result = httpd_resp_send(request, body.data(), HTTPD_RESP_USE_STRLEN);
    mbedtls_platform_zeroize(body.data(), body.size());
    return result;
#endif
}

esp_err_t CameraApi::inventory(httpd_req_t* request) {
    const auto capture = camera_.captureStatus();
    const auto streams = streams_.status();
    cJSON* root = cJSON_CreateObject();
    cJSON* data = cJSON_AddObjectToObject(root, "data");
    cJSON* cameras = cJSON_AddArrayToObject(data, "cameras");
    bool valid = root != nullptr && data != nullptr && cameras != nullptr &&
        cJSON_AddNumberToObject(data, "simultaneous_csi_inputs", 1) != nullptr &&
        cJSON_AddBoolToObject(data, "development_login", kDevelopmentLogin) != nullptr;
    const char* sensor = camera_hub::cameraSensorName(probe_.sensor);
    if (sensor != nullptr) {
        cJSON* camera = cJSON_CreateObject();
        valid = valid && camera != nullptr && cJSON_AddStringToObject(camera, "id", "csi-0") != nullptr &&
            cJSON_AddStringToObject(camera, "sensor", sensor) != nullptr &&
            cJSON_AddStringToObject(camera, "interface", "MIPI-CSI") != nullptr &&
            cJSON_AddBoolToObject(camera, "capturing", capture.running && capture.frameReceived) != nullptr &&
            cJSON_AddStringToObject(camera, "codec", camera_hub::videoCodecKey(capture.codec)) != nullptr &&
            (capture.codec == camera_hub::VideoCodec::Jpeg
                ? cJSON_AddStringToObject(camera, "mjpeg_path", "/stream.mjpg")
                : cJSON_AddNullToObject(camera, "mjpeg_path")) != nullptr;
        valid = valid && cJSON_AddNumberToObject(camera, "mjpeg_port", 81) != nullptr &&
            cJSON_AddNumberToObject(camera, "rtsp_port", 8554) != nullptr &&
            (streams.ready && streams.codec == capture.codec && capture.running && capture.frameReceived
                ? cJSON_AddStringToObject(camera, "rtsp_path", "/cameras/csi-0")
                : cJSON_AddNullToObject(camera, "rtsp_path")) != nullptr;
        if (camera != nullptr && !cJSON_AddItemToArray(cameras, camera)) {
            cJSON_Delete(camera);
            valid = false;
        }
    }
    return sendJson(request, root, valid);
}

esp_err_t CameraApi::configuration(httpd_req_t* request) {
    if (request->method == HTTP_PATCH) {
        cJSON* root = readJson(request);
        cJSON* mode = cJSON_GetObjectItemCaseSensitive(root, "mode_id");
        cJSON* quality = cJSON_GetObjectItemCaseSensitive(root, "jpeg_quality");
        cJSON* codec = cJSON_GetObjectItemCaseSensitive(root, "codec");
        cJSON* bitrate = cJSON_GetObjectItemCaseSensitive(root, "h264_bitrate");
        const bool validCodec = codec == nullptr || (cJSON_IsString(codec) &&
            (std::strcmp(codec->valuestring, "jpeg") == 0 || std::strcmp(codec->valuestring, "h264") == 0));
        const bool valid = root != nullptr && cJSON_GetArraySize(root) == 2 + (codec != nullptr) + (bitrate != nullptr) &&
            integer(mode, 0, 31) && integer(quality, 1, 100) && validCodec &&
            (bitrate == nullptr || integer(bitrate, 64000, 10000000));
        auto settings = camera_.configuration().settings;
        if (valid) {
            settings.modeId = static_cast<std::uint32_t>(mode->valuedouble);
            // Target FPS follows selected driver mode, not a user-supplied FPS.
            const auto config = camera_.configuration();
            for (std::size_t i = 0; i < config.modeCount; ++i) {
                if (config.modes[i].id == settings.modeId &&
                    config.modes[i].sensorFps <= UINT8_MAX)
                    settings.previewFps = static_cast<std::uint8_t>(config.modes[i].sensorFps);
            }
            settings.jpegQuality = static_cast<std::uint8_t>(quality->valuedouble);
            if (codec != nullptr) settings.codec = std::strcmp(codec->valuestring, "h264") == 0
                ? camera_hub::VideoCodec::H264 : camera_hub::VideoCodec::Jpeg;
            if (bitrate != nullptr) settings.h264Bitrate = static_cast<std::uint32_t>(bitrate->valuedouble);
        }
        cJSON_Delete(root);
        if (!valid) return failure(request, "400 Bad Request", "invalid_camera_configuration");
        const auto result = camera_.requestConfiguration(settings);
        if (result != ESP_OK) return failure(request, result == ESP_ERR_INVALID_ARG
            ? "400 Bad Request" : "409 Conflict", result == ESP_ERR_INVALID_ARG
            ? "unsupported_camera_configuration" : "camera_busy");
        httpd_resp_set_status(request, "202 Accepted");
    }
    const auto configuration = camera_.configuration();
    cJSON* root = cJSON_CreateObject();
    cJSON* data = cJSON_AddObjectToObject(root, "data");
    cJSON* settings = cJSON_AddObjectToObject(data, "settings");
    cJSON* modes = cJSON_AddArrayToObject(data, "modes");
    bool valid = root != nullptr && data != nullptr && settings != nullptr && modes != nullptr &&
        cJSON_AddBoolToObject(data, "persistent", false) != nullptr &&
        cJSON_AddBoolToObject(data, "usb_streaming", camera_.usbStreaming()) != nullptr &&
        cJSON_AddStringToObject(data, "state", configurationState(configuration.state)) != nullptr &&
        cJSON_AddNumberToObject(settings, "mode_id", configuration.settings.modeId) != nullptr &&
        cJSON_AddNumberToObject(settings, "preview_fps", configuration.settings.previewFps) != nullptr &&
        cJSON_AddNumberToObject(settings, "jpeg_quality", configuration.settings.jpegQuality) != nullptr &&
        cJSON_AddStringToObject(settings, "codec", camera_hub::videoCodecKey(configuration.settings.codec)) != nullptr &&
        cJSON_AddNumberToObject(settings, "h264_bitrate", configuration.settings.h264Bitrate) != nullptr;
    cJSON* codecs = cJSON_AddArrayToObject(data, "hardware_codecs");
    cJSON* jpeg = cJSON_CreateString("jpeg");
    cJSON* h264 = cJSON_CreateString("h264");
    if (!cJSON_AddItemToArray(codecs, jpeg)) { cJSON_Delete(jpeg); valid = false; }
    if (!cJSON_AddItemToArray(codecs, h264)) { cJSON_Delete(h264); valid = false; }
    for (std::size_t index = 0; index < configuration.modeCount; ++index) {
        const auto& mode = configuration.modes[index];
        cJSON* entry = cJSON_CreateObject();
        valid = valid && entry != nullptr && cJSON_AddNumberToObject(entry, "id", mode.id) != nullptr &&
            cJSON_AddStringToObject(entry, "name", mode.name.data()) != nullptr &&
            cJSON_AddNumberToObject(entry, "width", mode.width) != nullptr &&
            cJSON_AddNumberToObject(entry, "height", mode.height) != nullptr &&
            cJSON_AddNumberToObject(entry, "sensor_fps", mode.sensorFps) != nullptr &&
            cJSON_AddBoolToObject(entry, "available", camera_hub::captureModeFits(mode)) != nullptr &&
            cJSON_AddBoolToObject(entry, "h264_available", camera_hub::hardwareH264ModeFits(mode)) != nullptr;
        if (entry != nullptr && !cJSON_AddItemToArray(modes, entry)) {
            cJSON_Delete(entry);
            valid = false;
        }
    }
    return sendJson(request, root, valid);
}

esp_err_t CameraApi::udp(httpd_req_t* request, std::uint32_t peer) {
    if (request->method == HTTP_PUT) {
        cJSON* root = readJson(request);
        cJSON* enabled = cJSON_GetObjectItemCaseSensitive(root, "enabled");
        cJSON* port = cJSON_GetObjectItemCaseSensitive(root, "port");
        const bool valid = root != nullptr && cJSON_GetArraySize(root) == 2 &&
            cJSON_IsBool(enabled) && integer(port, 1024, 65534) && port->valueint % 2 == 0;
        esp_err_t result = ESP_ERR_INVALID_ARG;
        if (valid) result = streams_.configureDirect(peer, static_cast<std::uint16_t>(port->valueint),
                                                     cJSON_IsTrue(enabled));
        cJSON_Delete(root);
        if (result != ESP_OK) return failure(request, valid ? "503 Service Unavailable" : "400 Bad Request",
                                             valid ? "udp_unavailable" : "invalid_udp_configuration");
    }
    const auto status = streams_.status();
    std::array<char, 16> receiver{};
    if (status.receiver != 0) inet_ntop(AF_INET, &status.receiver, receiver.data(), receiver.size());
    cJSON* root = cJSON_CreateObject();
    cJSON* data = cJSON_AddObjectToObject(root, "data");
    const bool valid = root != nullptr && data != nullptr &&
        cJSON_AddBoolToObject(data, "ready", status.ready) != nullptr &&
        cJSON_AddStringToObject(data, "codec", camera_hub::videoCodecKey(status.codec)) != nullptr &&
        cJSON_AddBoolToObject(data, "enabled", status.directEnabled) != nullptr &&
        cJSON_AddNumberToObject(data, "port", status.port) != nullptr &&
        cJSON_AddStringToObject(data, "receiver", receiver.data()) != nullptr &&
        cJSON_AddNumberToObject(data, "frames_sent", status.framesSent) != nullptr &&
        cJSON_AddNumberToObject(data, "oversized_frames", status.oversizedFrames) != nullptr &&
        cJSON_AddNumberToObject(data, "send_errors", status.sendErrors) != nullptr &&
        cJSON_AddNumberToObject(data, "stale_frames", status.staleFrames) != nullptr &&
        cJSON_AddNumberToObject(data, "last_frame_age_us", status.lastFrameAgeUs) != nullptr &&
        cJSON_AddNumberToObject(data, "max_frame_age_us", status.maxFrameAgeUs) != nullptr &&
        cJSON_AddNumberToObject(data, "last_send_duration_us", status.lastSendDurationUs) != nullptr &&
        cJSON_AddNumberToObject(data, "max_send_duration_us", status.maxSendDurationUs) != nullptr;
    return sendJson(request, root, valid);
}

esp_err_t CameraApi::sdp(httpd_req_t* request, std::uint32_t peer) {
    const auto status = streams_.status();
    if (!status.ready || !status.directEnabled || status.receiver != peer)
        return failure(request, "409 Conflict", "udp_receiver_not_active");
    std::array<char, 16> receiver{};
    if (inet_ntop(AF_INET, &peer, receiver.data(), receiver.size()) == nullptr)
        return failure(request, "500 Internal Server Error", "response_unavailable");
    std::array<char, 384> body{};
    std::snprintf(body.data(), body.size(),
                  "v=0\r\no=- 0 0 IN IP4 %s\r\ns=Camera csi-0\r\nc=IN IP4 %s\r\nt=0 0\r\n"
                  "m=video %u RTP/AVP %u\r\na=rtpmap:%u %s/90000\r\n%sa=recvonly\r\n",
                  receiver.data(), receiver.data(), static_cast<unsigned int>(status.port),
                  status.codec == camera_hub::VideoCodec::H264 ? 96U : 26U,
                  status.codec == camera_hub::VideoCodec::H264 ? 96U : 26U,
                  status.codec == camera_hub::VideoCodec::H264 ? "H264" : "JPEG",
                  status.codec == camera_hub::VideoCodec::H264 ? "a=fmtp:96 packetization-mode=1\r\n" : "");
    httpd_resp_set_type(request, "application/sdp");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    httpd_resp_set_hdr(request, "Content-Disposition", "attachment; filename=csi-0.sdp");
    return httpd_resp_send(request, body.data(), HTTPD_RESP_USE_STRLEN);
}

} // namespace web_api