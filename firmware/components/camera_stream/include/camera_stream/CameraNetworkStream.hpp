#pragma once

#include <cstdint>
#include "camera_stream/CameraHardwareProbe.hpp"

namespace camera_stream {

struct NetworkStreamStatus {
    bool ready{false};
    bool directEnabled{false};
    std::uint32_t receiver{0};
    std::uint16_t port{5004};
    std::uint32_t framesSent{0};
    std::uint32_t oversizedFrames{0};
    std::uint32_t sendErrors{0};
    std::uint32_t staleFrames{0};
    std::uint32_t lastFrameAgeUs{0};
    std::uint32_t maxFrameAgeUs{0};
    std::uint32_t lastSendDurationUs{0};
    std::uint32_t maxSendDurationUs{0};
};

class CameraNetworkStream final {
public:
    explicit CameraNetworkStream(CameraHardwareProbe& camera) noexcept;
    ~CameraNetworkStream();
    CameraNetworkStream(const CameraNetworkStream&) = delete;
    CameraNetworkStream& operator=(const CameraNetworkStream&) = delete;
    [[nodiscard]] esp_err_t start(std::uint32_t address) noexcept;
    void stop() noexcept;
    [[nodiscard]] NetworkStreamStatus status() const noexcept;
    [[nodiscard]] esp_err_t configureDirect(std::uint32_t receiver, std::uint16_t port,
                                           bool enabled) noexcept;

private:
    static void taskEntry(void* context);
    void run() noexcept;
    CameraHardwareProbe& camera_;
    struct State;
    State* state_{nullptr};
};

} // namespace camera_stream