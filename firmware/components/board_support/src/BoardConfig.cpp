#include "board_support/BoardConfig.hpp"

namespace board_support {

board_capabilities::BoardProfile BoardConfig::profile() const noexcept {
    using namespace board_capabilities;
    return {"waveshare-esp32-p4-module-dev-kit", "esp32p4", WifiTopology::Companion,
            Verification::Unverified, Verification::Absent, Verification::Absent,
            Verification::Unverified, Verification::Unverified};
}

}