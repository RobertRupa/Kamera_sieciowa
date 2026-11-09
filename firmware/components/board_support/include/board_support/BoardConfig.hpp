#pragma once

#include "board_capabilities/BoardProfile.hpp"

namespace board_support {

class BoardConfig final {
public:
    [[nodiscard]] board_capabilities::BoardProfile profile() const noexcept;
};

}