#pragma once

#include "camera_hub/CameraProbeResult.hpp"

namespace camera_stream {

class CameraHardwareProbe final {
public:
    [[nodiscard]] camera_hub::CameraProbeResult run() const noexcept;
};

} // namespace camera_stream