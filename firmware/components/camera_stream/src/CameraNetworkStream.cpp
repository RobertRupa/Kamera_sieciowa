#include "camera_stream/CameraNetworkStream.hpp"
#include "camera_hub/JpegHeaderView.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <fcntl.h>
#include <memory>
#include <limits>
#include <mutex>
#include <new>
#include <span>

#include "esp_random.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "rtsp_server.hpp"
#include "sdkconfig.h"

namespace camera_stream {
namespace {

constexpr std::size_t kMaxRtpFrameBytes = 256 * 1024;
constexpr std::size_t kScanBytesPerPacket = 1000;
constexpr std::uint64_t kMaxFrameAgeUs = 100000;

class ScanPacketizer final : public espp::RtpPacketizer {
public:
    ScanPacketizer() : espp::RtpPacketizer({.max_payload_size = 1200}, "Camera RTP") {}

    std::vector<espp::RtpPayloadChunk> packetize(std::span<const std::uint8_t> frame) override {
        if (frame.size() < 4 || frame.size() > kMaxRtpFrameBytes) return {};
        camera_hub::JpegHeaderView normalized{};
        if (!camera_hub::prepareJpegHeader(frame.data(), frame.size(), normalized)) return {};
        espp::JpegHeader header(std::span<const std::uint8_t>(normalized.bytes.data(), normalized.length));
        const auto width = header.get_width();
        const auto height = header.get_height();
        const auto q0 = header.get_quantization_table(0);
        const auto q1 = header.get_quantization_table(1);
        if (!header.is_valid() || normalized.scanOffset + 2 >= frame.size() ||
            width <= 0 || height <= 0 || width > 2040 || height > 2040 ||
            width % 8 != 0 || height % 8 != 0 || q0.size() != 64 || q1.size() != 64 ||
            frame[frame.size() - 2] != 0xff || frame.back() != 0xd9) return {};
        const auto scan = frame.subspan(normalized.scanOffset, frame.size() - normalized.scanOffset - 2);
        std::vector<espp::RtpPayloadChunk> chunks;
        chunks.reserve((scan.size() + kScanBytesPerPacket - 1) / kScanBytesPerPacket);
        for (std::size_t offset = 0; offset < scan.size(); offset += kScanBytesPerPacket) {
            const auto part = scan.subspan(offset, std::min(kScanBytesPerPacket, scan.size() - offset));
            auto packet = offset == 0
                ? espp::RtpJpegPacket(0, 0, 128, width, height, q0, q1, part)
                : espp::RtpJpegPacket(0, static_cast<int>(offset), 0, 128, width, height, part);
            const auto payload = packet.get_payload();
            espp::RtpPayloadChunk chunk{};
            chunk.data.assign(payload.begin(), payload.end());
            chunk.marker = offset + part.size() == scan.size();
            chunks.push_back(std::move(chunk));
        }
        return chunks;
    }

    int get_payload_type() const override { return 26; }
    std::uint32_t get_clock_rate() const override { return 90000; }
    std::string get_sdp_media_attributes() const override { return "a=rtpmap:26 JPEG/90000"; }
    std::string get_sdp_media_line() const override { return "m=video 0 RTP/AVP 26"; }
};

} // namespace

struct CameraNetworkStream::State {
    mutable std::mutex mutex;
    NetworkStreamStatus status{};
    std::atomic<bool> running{false};
    std::unique_ptr<espp::RtspServer> server;
    std::shared_ptr<ScanPacketizer> packetizer;
    TaskHandle_t task{nullptr};
    SemaphoreHandle_t stopped{nullptr};
    int udp{-1};
    std::int64_t directExpiresAt{0};
};

CameraNetworkStream::CameraNetworkStream(CameraHardwareProbe& camera) noexcept
    : camera_(camera), state_(new (std::nothrow) State{}) {}

CameraNetworkStream::~CameraNetworkStream() {
    stop();
    if (state_ != nullptr && !state_->running.load() && state_->task == nullptr) delete state_;
}

esp_err_t CameraNetworkStream::start(std::uint32_t address) noexcept {
#if !CONFIG_APP_DEV_MODE
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (state_ == nullptr) return ESP_ERR_NO_MEM;
    auto& state = *state_;
    if (state.running.load() || state.task != nullptr || address == 0 ||
        !camera_.captureStatus().running) return ESP_ERR_INVALID_STATE;
    std::array<char, 16> text{};
    if (inet_ntop(AF_INET, &address, text.data(), text.size()) == nullptr) return ESP_ERR_INVALID_ARG;
    espp::RtspServer::Config config{};
    config.server_address = text.data();
    config.port = 8554;
    config.path = "cameras/csi-0";
    config.max_data_size = 1200;
    state.server.reset(new (std::nothrow) espp::RtspServer(config));
    if (!state.server) return ESP_ERR_NO_MEM;
    state.packetizer = std::make_shared<ScanPacketizer>();
    state.server->add_track({.track_id = 0, .packetizer = state.packetizer});
    if (!state.server->start(std::chrono::milliseconds(100))) {
        state.server.reset();
        return ESP_FAIL;
    }
    state.udp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (state.udp < 0 || fcntl(state.udp, F_SETFL, O_NONBLOCK) < 0) {
        if (state.udp >= 0) close(state.udp);
        state.udp = -1;
        state.server.reset();
        return ESP_FAIL;
    }
    state.stopped = xSemaphoreCreateBinary();
    if (state.stopped == nullptr) {
        close(state.udp);
        state.udp = -1;
        state.server.reset();
        return ESP_ERR_NO_MEM;
    }
    state.running.store(true);
    if (xTaskCreate(taskEntry, "camera_rtp", 12288, this, 4, &state.task) != pdPASS) {
        state.running.store(false);
        close(state.udp);
        state.udp = -1;
        state.server.reset();
        vSemaphoreDelete(state.stopped);
        state.stopped = nullptr;
        return ESP_ERR_NO_MEM;
    }
    const std::lock_guard<std::mutex> lock(state.mutex);
    state.status.ready = true;
    return ESP_OK;
#endif
}

void CameraNetworkStream::stop() noexcept {
    if (state_ == nullptr) return;
    auto& state = *state_;
    {
        const std::lock_guard<std::mutex> lock(state.mutex);
        state.status.ready = false;
        state.status.directEnabled = false;
    }
    state.running.store(false);
    if (state.task != nullptr) {
        if (xSemaphoreTake(state.stopped, pdMS_TO_TICKS(5000)) != pdPASS) return;
        state.task = nullptr;
    }
    state.server.reset();
    if (state.stopped != nullptr) { vSemaphoreDelete(state.stopped); state.stopped = nullptr; }
    const std::lock_guard<std::mutex> lock(state.mutex);
    state.status.ready = false;
    state.status.directEnabled = false;
}

NetworkStreamStatus CameraNetworkStream::status() const noexcept {
    if (state_ == nullptr) return {};
    const std::lock_guard<std::mutex> lock(state_->mutex);
    return state_->status;
}

esp_err_t CameraNetworkStream::configureDirect(std::uint32_t receiver, std::uint16_t port,
                                               bool enabled) noexcept {
    if (state_ == nullptr || !state_->running.load()) return ESP_ERR_INVALID_STATE;
    if (receiver == 0 || port < 1024 || port > 65534 || port % 2 != 0) return ESP_ERR_INVALID_ARG;
    const std::lock_guard<std::mutex> lock(state_->mutex);
    state_->status.receiver = receiver;
    state_->status.port = port;
    state_->status.directEnabled = enabled;
    state_->directExpiresAt = esp_timer_get_time() + 900000000;
    return ESP_OK;
}

void CameraNetworkStream::taskEntry(void* context) {
    static_cast<CameraNetworkStream*>(context)->run();
    vTaskDelete(nullptr);
}

void CameraNetworkStream::run() noexcept {
    auto& state = *state_;
    const int udp = state.udp;
    std::uint16_t sequence = static_cast<std::uint16_t>(esp_random());
    const auto ssrc = esp_random();
    bool subscribed = false;
    while (state.running.load()) {
        NetworkStreamStatus snapshot{};
        {
            const std::lock_guard<std::mutex> lock(state.mutex);
            if (esp_timer_get_time() >= state.directExpiresAt) state.status.directEnabled = false;
            snapshot = state.status;
        }
        const bool rtspActive = state.server->has_active_sessions();
        const bool wanted = rtspActive || (snapshot.directEnabled && udp >= 0);
        if (wanted != subscribed) { camera_.setNetworkEnabled(wanted); subscribed = wanted; }
        auto frame = wanted ? camera_.tryTakeNetworkFrame() : camera_hub::FrameLease{};
        if (frame) {
            const auto now = static_cast<std::uint64_t>(esp_timer_get_time());
            const auto age = now >= frame.timestampUs() ? now - frame.timestampUs() : 0;
            {
                const std::lock_guard<std::mutex> lock(state.mutex);
                state.status.lastFrameAgeUs = static_cast<std::uint32_t>(
                    std::min<std::uint64_t>(age, std::numeric_limits<std::uint32_t>::max()));
                state.status.maxFrameAgeUs = std::max(state.status.maxFrameAgeUs, state.status.lastFrameAgeUs);
                if (age > kMaxFrameAgeUs) ++state.status.staleFrames;
            }
            if (age > kMaxFrameAgeUs) continue;
            if (frame.size() > kMaxRtpFrameBytes) {
                const std::lock_guard<std::mutex> lock(state.mutex);
                ++state.status.oversizedFrames;
            } else {
                const std::span<const std::uint8_t> bytes(frame.data(), frame.size());
                if (rtspActive) state.server->send_frame(0, bytes);
                if (snapshot.directEnabled && udp >= 0) {
                    const auto sendStarted = esp_timer_get_time();
                    sockaddr_in destination{};
                    destination.sin_family = AF_INET;
                    destination.sin_addr.s_addr = snapshot.receiver;
                    destination.sin_port = htons(snapshot.port);
                    const auto chunks = state.packetizer->packetize(bytes);
                    bool sent = !chunks.empty();
                    std::size_t burst = 0;
                    for (const auto& chunk : chunks) {
                        espp::RtpPacket packet;
                        packet.set_payload(chunk.data);
                        packet.set_version(2);
                        packet.set_payload_type(26);
                        packet.set_sequence_number(sequence++);
                        packet.set_timestamp(static_cast<std::uint32_t>(frame.timestampUs() * 9 / 100));
                        packet.set_ssrc(ssrc);
                        packet.set_marker(chunk.marker);
                        packet.serialize();
                        const auto data = packet.get_data();
                        if (sendto(udp, data.data(), data.size(), 0,
                                   reinterpret_cast<sockaddr*>(&destination), sizeof(destination)) !=
                            static_cast<int>(data.size())) { sent = false; break; }
                        if (++burst % 4 == 0) {
                            taskYIELD();
                            esp_rom_delay_us(500);
                        }
                    }
                    const auto sendDuration = static_cast<std::uint32_t>(esp_timer_get_time() - sendStarted);
                    const std::lock_guard<std::mutex> lock(state.mutex);
                    state.status.lastSendDurationUs = sendDuration;
                    state.status.maxSendDurationUs = std::max(state.status.maxSendDurationUs, sendDuration);
                    if (sent) ++state.status.framesSent;
                    else ++state.status.sendErrors;
                }
            }
        }
        if (frame) taskYIELD();
        else vTaskDelay(1);
    }
    camera_.setNetworkEnabled(false);
    if (udp >= 0) close(udp);
    state.udp = -1;
    xSemaphoreGive(state.stopped);
}

} // namespace camera_stream