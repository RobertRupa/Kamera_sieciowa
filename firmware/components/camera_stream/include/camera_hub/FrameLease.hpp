#pragma once

#include <cstddef>
#include <cstdint>

namespace camera_hub {

class FrameBroker;

class FrameLease final {
public:
    FrameLease() noexcept = default;
    ~FrameLease();
    FrameLease(const FrameLease&) = delete;
    FrameLease& operator=(const FrameLease&) = delete;
    FrameLease(FrameLease&& other) noexcept;
    FrameLease& operator=(FrameLease&& other) noexcept;

    [[nodiscard]] explicit operator bool() const noexcept;
    [[nodiscard]] const std::uint8_t* data() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::uint64_t timestampUs() const noexcept;
    void reset() noexcept;

private:
    friend class FrameBroker;
    FrameLease(FrameBroker& broker, std::size_t slot, const std::uint8_t* data,
               std::size_t size, std::uint64_t timestampUs) noexcept;
    FrameBroker* broker_{nullptr};
    std::size_t slot_{0};
    const std::uint8_t* data_{nullptr};
    std::size_t size_{0};
    std::uint64_t timestampUs_{0};
};

}