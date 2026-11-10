#pragma once

#include <cstdint>
#include <string_view>

#include "esp_err.h"

namespace discovery {

class MdnsService final {
public:
    ~MdnsService();
    MdnsService() noexcept = default;
    MdnsService(const MdnsService&) = delete;
    MdnsService& operator=(const MdnsService&) = delete;
    [[nodiscard]] esp_err_t start(std::string_view hostname, bool ipReady,
                                  std::uint16_t activeHttpPort = 0) noexcept;
    // Keep the standards-compliant camera-hub.local compatibility alias
    // pointing to the currently reachable AP or STA IPv4 address.
    [[nodiscard]] esp_err_t updateFallbackAddress(std::uint32_t ipv4) noexcept;
    [[nodiscard]] bool running() const noexcept { return started_; }
    void stop() noexcept;

private:
    bool started_{false};
    bool fallbackRegistered_{false};
    std::uint32_t fallbackIpv4_{0};
};

}