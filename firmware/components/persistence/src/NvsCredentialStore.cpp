#include "persistence/NvsCredentialStore.hpp"

#include "camera_hub/WifiCredentialCodec.hpp"
#include "esp_partition.h"
#include "mbedtls/platform_util.h"
#include "nvs_flash.h"

namespace persistence {

NvsCredentialStore::~NvsCredentialStore() {
    if (handle_ != 0) nvs_close(handle_);
    if (initialized_) nvs_flash_deinit_partition("app_config");
}

camera_hub::CredentialError NvsCredentialStore::initialize() noexcept {
    using camera_hub::CredentialError;
    const std::lock_guard<std::mutex> lock(mutex_);
    if (handle_ != 0) return CredentialError::None;
    const auto* partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_DATA_NVS, "app_config");
    if (partition == nullptr || partition->size < 0x6000) {
        lastError_ = ESP_ERR_NOT_FOUND;
        return CredentialError::StorageFailure;
    }
    lastError_ = nvs_flash_init_partition("app_config");
    if (lastError_ != ESP_OK) return CredentialError::StorageFailure;
    initialized_ = true;
    lastError_ = nvs_open_from_partition("app_config", "camera_hub", NVS_READWRITE, &handle_);
    if (lastError_ != ESP_OK) {
        nvs_flash_deinit_partition("app_config");
        initialized_ = false;
        handle_ = 0;
        return CredentialError::StorageFailure;
    }
    return CredentialError::None;
}

bool NvsCredentialStore::ready() const noexcept {
    const std::lock_guard<std::mutex> lock(mutex_);
    return handle_ != 0 && initialized_;
}

esp_err_t NvsCredentialStore::lastPlatformError() const noexcept {
    const std::lock_guard<std::mutex> lock(mutex_);
    return lastError_;
}

camera_hub::CredentialError NvsCredentialStore::save(const camera_hub::WifiCredentials& credentials) noexcept {
    using namespace camera_hub;
    const std::lock_guard<std::mutex> lock(mutex_);
    if (handle_ == 0) return CredentialError::NotReady;
    CredentialRecord record{};
    const auto encoded = WifiCredentialCodec::encode(credentials, record);
    if (encoded != CredentialError::None) {
        mbedtls_platform_zeroize(record.data(), record.size());
        return encoded;
    }
    lastError_ = nvs_set_blob(handle_, "wifi_v1", record.data(), record.size());
    mbedtls_platform_zeroize(record.data(), record.size());
    if (lastError_ == ESP_OK) lastError_ = nvs_commit(handle_);
    return lastError_ == ESP_OK ? CredentialError::None : CredentialError::StorageFailure;
}

camera_hub::CredentialError NvsCredentialStore::load(camera_hub::WifiCredentials& credentials) noexcept {
    using namespace camera_hub;
    const std::lock_guard<std::mutex> lock(mutex_);
    credentials.clear();
    if (handle_ == 0) return CredentialError::NotReady;
    std::size_t size = 0;
    lastError_ = nvs_get_blob(handle_, "wifi_v1", nullptr, &size);
    if (lastError_ == ESP_ERR_NVS_NOT_FOUND) return CredentialError::NotFound;
    if (lastError_ != ESP_OK) return CredentialError::StorageFailure;
    CredentialRecord record{};
    if (size != record.size()) return CredentialError::CorruptRecord;
    lastError_ = nvs_get_blob(handle_, "wifi_v1", record.data(), &size);
    const auto result = lastError_ == ESP_OK && size == record.size()
        ? WifiCredentialCodec::decode(record, credentials) : CredentialError::StorageFailure;
    mbedtls_platform_zeroize(record.data(), record.size());
    return result;
}

camera_hub::CredentialError NvsCredentialStore::clear() noexcept {
    using camera_hub::CredentialError;
    const std::lock_guard<std::mutex> lock(mutex_);
    if (handle_ == 0) return CredentialError::NotReady;
    lastError_ = nvs_erase_key(handle_, "wifi_v1");
    if (lastError_ == ESP_ERR_NVS_NOT_FOUND) {
        lastError_ = ESP_OK;
        return CredentialError::None;
    }
    if (lastError_ == ESP_OK) lastError_ = nvs_commit(handle_);
    return lastError_ == ESP_OK ? CredentialError::None : CredentialError::StorageFailure;
}

}