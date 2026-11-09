#include "camera_hub/FrameBroker.hpp"

#include <cstring>

namespace camera_hub {

FrameBroker::FrameBroker(std::uint8_t* storage, std::size_t storageBytes,
                         std::size_t maxFrameBytes) noexcept
    : storage_(storage), maxFrameBytes_(maxFrameBytes),
      valid_(storage != nullptr && maxFrameBytes != 0 &&
             maxFrameBytes <= storageBytes / slotCount) {}

bool FrameBroker::valid() const noexcept { return valid_; }

void FrameBroker::dropFront(Queue& queue) noexcept {
    --slots_[queue.slots[0]].references;
    for (std::size_t position = 1; position < queue.size; ++position) {
        queue.slots[position - 1] = queue.slots[position];
    }
    --queue.size;
}

void FrameBroker::setEnabled(Output output, bool enabled) noexcept {
    const auto index = static_cast<std::size_t>(output);
    if (index >= outputCount) {
        return;
    }
    const std::lock_guard<std::mutex> lock(mutex_);
    auto& queue = queues_[index];
    queue.enabled = enabled;
    if (!enabled) {
        while (queue.size != 0) {
            dropFront(queue);
        }
    }
}

PublishResult FrameBroker::publish(const std::uint8_t* jpeg, std::size_t size,
                                   std::uint64_t timestampUs, VideoCodec codec) noexcept {
    if (!valid_ || jpeg == nullptr || size < 4) {
        return PublishResult::InvalidFrame;
    }
    if (size > maxFrameBytes_) {
        return PublishResult::TooLarge;
    }
    const bool validJpeg = codec == VideoCodec::Jpeg && jpeg[0] == 0xff && jpeg[1] == 0xd8 &&
        jpeg[size - 2] == 0xff && jpeg[size - 1] == 0xd9;
    const bool validH264 = codec == VideoCodec::H264 && jpeg[0] == 0 && jpeg[1] == 0 &&
        (jpeg[2] == 1 || (jpeg[2] == 0 && jpeg[3] == 1));
    if (!validJpeg && !validH264) {
        return PublishResult::InvalidFrame;
    }
    std::size_t selected = slotCount;
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        for (std::size_t slot = 0; slot < slotCount; ++slot) {
            if (slots_[slot].references == 0) {
                selected = slot;
                slots_[slot].references = 1;
                break;
            }
        }
        if (selected == slotCount) {
            ++stats_.poolExhausted;
            return PublishResult::PoolExhausted;
        }
    }
    auto& slot = slots_[selected];
    std::memcpy(storage_ + selected * maxFrameBytes_, jpeg, size);
    slot.size = size;
    slot.timestampUs = timestampUs;
    slot.codec = codec;
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        for (std::size_t index = 0; index < outputCount; ++index) {
            auto& queue = queues_[index];
            if (!queue.enabled) {
                continue;
            }
            if (queue.size == queueDepth) {
                dropFront(queue);
                ++stats_.dropped[index];
            }
            queue.slots[queue.size++] = selected;
            ++slot.references;
        }
        --slot.references;
        ++stats_.published;
    }
    return PublishResult::Published;
}

FrameLease FrameBroker::tryTake(Output output) noexcept {
    return take(output, false);
}

FrameLease FrameBroker::tryTakeLatest(Output output) noexcept {
    return take(output, true);
}

FrameLease FrameBroker::take(Output output, bool latest) noexcept {
    const auto index = static_cast<std::size_t>(output);
    if (index >= outputCount) {
        return {};
    }
    const std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
    if (!lock.owns_lock()) {
        return {};
    }
    auto& queue = queues_[index];
    if (queue.size == 0) {
        return {};
    }
    while (latest && queue.size > 1) {
        dropFront(queue);
        ++stats_.dropped[index];
    }
    const auto selected = queue.slots[0];
    const auto& slot = slots_[selected];
    ++slots_[selected].references;
    dropFront(queue);
    return FrameLease(*this, selected, storage_ + selected * maxFrameBytes_,
                      slot.size, slot.timestampUs, slot.codec);
}

void FrameBroker::release(std::size_t slot) noexcept {
    const std::lock_guard<std::mutex> lock(mutex_);
    --slots_[slot].references;
}

StreamStats FrameBroker::stats() const noexcept {
    const std::lock_guard<std::mutex> lock(mutex_);
    auto result = stats_;
    for (std::size_t index = 0; index < outputCount; ++index) {
        result.queued[index] = queues_[index].size;
    }
    for (const auto& slot : slots_) {
        result.occupiedSlots += slot.references != 0 ? 1 : 0;
    }
    return result;
}

}