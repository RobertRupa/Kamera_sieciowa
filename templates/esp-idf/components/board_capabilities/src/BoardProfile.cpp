#include "board_capabilities/BoardProfile.hpp"

namespace board_capabilities {

BoardProfile makeIllustrativeP4C6Profile() noexcept {
    return {
        "example-esp32p4-c6-display",
        "esp32p4",
        WifiTopology::Companion,
        Verification::Unverified, // Actual P4<->C6 transport not verified.
        Verification::Unverified, // LCD controller/pins not verified.
        Verification::Unverified, // Touch controller/pins not verified.
        Verification::Unverified, // Protected NVS key mechanism not verified.
    };
}

}  // namespace board_capabilities
