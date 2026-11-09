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
    void stop() noexcept;

private:
    bool started_{false};
};

}