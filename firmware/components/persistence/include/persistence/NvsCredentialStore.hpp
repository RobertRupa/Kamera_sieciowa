#pragma once

#include <mutex>

#include "camera_hub/ICredentialStore.hpp"
#include "nvs.h"

namespace persistence {

class NvsCredentialStore final : public camera_hub::ICredentialStore {
public:
    NvsCredentialStore() noexcept = default;
    ~NvsCredentialStore();
    NvsCredentialStore(const NvsCredentialStore&) = delete;
    NvsCredentialStore& operator=(const NvsCredentialStore&) = delete;
    [[nodiscard]] camera_hub::CredentialError initialize() noexcept;
    [[nodiscard]] bool ready() const noexcept override;
    [[nodiscard]] camera_hub::CredentialError save(const camera_hub::WifiCredentials& credentials) noexcept override;
    [[nodiscard]] camera_hub::CredentialError load(camera_hub::WifiCredentials& credentials) noexcept override;
    [[nodiscard]] camera_hub::CredentialError clear() noexcept override;
    [[nodiscard]] esp_err_t lastPlatformError() const noexcept;

private:
    mutable std::mutex mutex_;
    nvs_handle_t handle_{0};
    bool initialized_{false};
    esp_err_t lastError_{ESP_OK};
};

}