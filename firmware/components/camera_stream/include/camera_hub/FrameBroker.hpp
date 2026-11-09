#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>

#include "camera_hub/FrameLease.hpp"

namespace camera_hub {

enum class Output : std::uint8_t { Usb, Http, Rtsp, Count };
enum class PublishResult : std::uint8_t { Published, InvalidFrame, TooLarge, PoolExhausted };

struct StreamStats {
    std::uint64_t published{0};
    std::uint64_t poolExhausted{0};
    std::array<std::uint64_t, 3> dropped{};
    std::array<std::size_t, 3> queued{};
    std::size_t occupiedSlots{0};
};

class FrameBroker final {
public:
    static constexpr std::size_t outputCount = 3;
    static constexpr std::size_t queueDepth = 2;
    static constexpr std::size_t slotCount = 10;

    FrameBroker(std::uint8_t* storage, std::size_t storageBytes,
                std::size_t maxFrameBytes) noexcept;
    FrameBroker(const FrameBroker&) = delete;
    FrameBroker& operator=(const FrameBroker&) = delete;

    [[nodiscard]] bool valid() const noexcept;
    void setEnabled(Output output, bool enabled) noexcept;
    [[nodiscard]] PublishResult publish(const std::uint8_t* jpeg, std::size_t size,
                                        std::uint64_t timestampUs) noexcept;
    [[nodiscard]] FrameLease tryTake(Output output) noexcept;
    [[nodiscard]] FrameLease tryTakeLatest(Output output) noexcept;
    [[nodiscard]] StreamStats stats() const noexcept;

private:
    friend class FrameLease;
    struct Slot {
        std::size_t references{0};
        std::size_t size{0};
        std::uint64_t timestampUs{0};
    };
    struct Queue {
        bool enabled{false};
        std::array<std::size_t, queueDepth> slots{};
        std::size_t size{0};
    };
    void release(std::size_t slot) noexcept;
    void dropFront(Queue& queue) noexcept;
    [[nodiscard]] FrameLease take(Output output, bool latest) noexcept;
    std::uint8_t* storage_;
    std::size_t maxFrameBytes_;
    bool valid_;
    mutable std::mutex mutex_;
    std::array<Slot, slotCount> slots_{};
    std::array<Queue, outputCount> queues_{};
    StreamStats stats_{};
};

}