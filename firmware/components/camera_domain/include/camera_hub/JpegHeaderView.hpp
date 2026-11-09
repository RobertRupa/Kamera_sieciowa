#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace camera_hub {

struct JpegHeaderView {
    std::array<std::uint8_t, 4096> bytes{};
    std::size_t length{0};
    std::size_t scanOffset{0};
};

inline bool prepareJpegHeader(const std::uint8_t* frame, std::size_t size,
                              JpegHeaderView& header) noexcept {
    header.length = 0;
    header.scanOffset = 0;
    if (frame == nullptr || size < 4 || frame[0] != 0xff || frame[1] != 0xd8) return false;
    header.bytes[0] = 0xff;
    header.bytes[1] = 0xd8;
    header.length = 2;
    std::size_t offset = 2;
    while (size - offset >= 4) {
        if (frame[offset] != 0xff) return false;
        const auto marker = frame[offset + 1];
        const auto length = static_cast<std::size_t>(frame[offset + 2]) * 256 + frame[offset + 3];
        if (length < 2 || length > size - offset - 2) return false;
        const auto segmentBytes = length + 2;
        const bool metadata = (marker >= 0xe0 && marker <= 0xef) || marker == 0xfe;
        if (!metadata) {
            if (segmentBytes > header.bytes.size() - header.length) return false;
            std::memcpy(header.bytes.data() + header.length, frame + offset, segmentBytes);
            header.length += segmentBytes;
        }
        offset += segmentBytes;
        if (marker == 0xda) {
            header.scanOffset = offset;
            return size - offset > 2;
        }
    }
    return false;
}

} // namespace camera_hub