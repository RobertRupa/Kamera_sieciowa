#include "camera_hub/FrameLease.hpp"

#include <utility>

#include "camera_hub/FrameBroker.hpp"

namespace camera_hub {

FrameLease::FrameLease(FrameBroker& broker, std::size_t slot, const std::uint8_t* data,
                       std::size_t size, std::uint64_t timestampUs, VideoCodec codec) noexcept
    : broker_(&broker), slot_(slot), data_(data), size_(size), timestampUs_(timestampUs), codec_(codec) {}

FrameLease::~FrameLease() { reset(); }

FrameLease::FrameLease(FrameLease&& other) noexcept
    : broker_(std::exchange(other.broker_, nullptr)), slot_(other.slot_),
      data_(std::exchange(other.data_, nullptr)), size_(std::exchange(other.size_, 0)),
    timestampUs_(std::exchange(other.timestampUs_, 0)), codec_(other.codec_) {}

FrameLease& FrameLease::operator=(FrameLease&& other) noexcept {
    if (this != &other) {
        reset();
        broker_ = std::exchange(other.broker_, nullptr);
        slot_ = other.slot_;
        data_ = std::exchange(other.data_, nullptr);
        size_ = std::exchange(other.size_, 0);
        timestampUs_ = std::exchange(other.timestampUs_, 0);
        codec_ = other.codec_;
    }
    return *this;
}

FrameLease::operator bool() const noexcept { return broker_ != nullptr; }
const std::uint8_t* FrameLease::data() const noexcept { return data_; }
std::size_t FrameLease::size() const noexcept { return size_; }
std::uint64_t FrameLease::timestampUs() const noexcept { return timestampUs_; }
VideoCodec FrameLease::codec() const noexcept { return codec_; }

void FrameLease::reset() noexcept {
    if (broker_ != nullptr) {
        broker_->release(slot_);
        broker_ = nullptr;
        data_ = nullptr;
        size_ = 0;
        timestampUs_ = 0;
    }
}

}