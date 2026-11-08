#pragma once

#include <cstdint>

namespace board_capabilities {

// Static, audited BOARD facts. Unknown does not mean false or true.
enum class Verification : std::uint8_t {
    Absent,
    Unverified,
    Verified,
};

enum class WifiTopology : std::uint8_t {
    None,
    Native,
    Companion,
};

struct BoardProfile {
    const char* boardId;
    const char* idfTarget;
    WifiTopology wifiTopology;
    Verification wifiRadio;
    Verification display;
    Verification touch;
    Verification nvsKeyProtection;
};

// Demonstration ONLY. Returned profile deliberately has no verified peripherals.
// Replace with a profile derived from the board schematic, BSP and hardware manifest.
[[nodiscard]] BoardProfile makeIllustrativeP4C6Profile() noexcept;

}  // namespace board_capabilities
