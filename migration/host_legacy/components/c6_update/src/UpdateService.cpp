#include "c6_update/UpdateService.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include "esp_app_desc.h"
#include "esp_app_format.h"
#include "esp_hosted_ota.h"
#include "esp_log.h"

namespace c6_update {

UpdateResult UpdateService::apply(const std::uint8_t* image, std::size_t length) const noexcept {
    constexpr std::size_t descriptorOffset = sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t);
    if (image == nullptr || length < descriptorOffset + sizeof(esp_app_desc_t) || length > 0x1c0000) {
        return {ESP_ERR_INVALID_SIZE, false, false};
    }
    esp_image_header_t header{};
    esp_app_desc_t descriptor{};
    std::memcpy(&header, image, sizeof(header));
    std::memcpy(&descriptor, image + descriptorOffset, sizeof(descriptor));
    if (header.magic != ESP_IMAGE_HEADER_MAGIC || header.chip_id != ESP_CHIP_ID_ESP32C6 ||
        descriptor.magic_word != ESP_APP_DESC_MAGIC_WORD ||
        std::strncmp(descriptor.version, "3.1.0", sizeof(descriptor.version)) != 0) {
        return {ESP_ERR_INVALID_ARG, false, false};
    }
    ESP_LOGI("c6_update", "OTA begin: verified C6/version header, %u bytes", static_cast<unsigned int>(length));
    auto result = esp_hosted_slave_ota_begin();
    if (result != ESP_OK) return {result, false, false};
    std::array<std::uint8_t, 1024> block{};
    for (std::size_t offset = 0; offset < length; offset += block.size()) {
        const auto size = std::min(block.size(), length - offset);
        std::memcpy(block.data(), image + offset, size);
        result = esp_hosted_slave_ota_write(block.data(), size);
        if (result != ESP_OK) return {result, false, false};
        if (offset % (64 * 1024) == 0) {
            ESP_LOGI("c6_update", "OTA transferred %u/%u", static_cast<unsigned int>(offset + size),
                     static_cast<unsigned int>(length));
        }
    }
    result = esp_hosted_slave_ota_end();
    if (result != ESP_OK) return {result, false, false};
    ESP_LOGI("c6_update", "OTA end accepted by C6; requesting activation");
    result = esp_hosted_slave_ota_activate();
    return {result, true, result == ESP_OK};
}

}