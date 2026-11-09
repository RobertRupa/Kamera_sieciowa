#pragma once

#include <cstdint>
#include <mutex>

#include "camera_hub/ICredentialStore.hpp"
#include "camera_hub/IWifiStation.hpp"

namespace app_services {

enum class ProvisioningState : std::uint8_t { Idle, Queued, Persisting, Connecting, Connected, Failed };
enum class SubmitResult : std::uint8_t { Accepted, InvalidArgument, NotReady, Busy };
enum class RestoreResult : std::uint8_t { Started, NoCredentials, StoreUnavailable, Busy, StorageFailure, WifiFailure };
struct ProvisioningSnapshot {
    std::uint32_t operationId{0};
    ProvisioningState state{ProvisioningState::Idle};
    camera_hub::CredentialError storageError{camera_hub::CredentialError::None};
    camera_hub::WifiError wifiError{camera_hub::WifiError::None};
};

class NetworkProvisioner final {
public:
    NetworkProvisioner(camera_hub::ICredentialStore& store, camera_hub::IWifiStation& station) noexcept;
    [[nodiscard]] SubmitResult submit(const camera_hub::WifiCredentials& credentials, std::uint32_t& operationId) noexcept;
    [[nodiscard]] RestoreResult restoreSavedCredentials() noexcept;
    void processPending() noexcept;
    void updateConnection(bool timedOut) noexcept;
    [[nodiscard]] ProvisioningSnapshot snapshot() const noexcept;

private:
    camera_hub::ICredentialStore& store_;
    camera_hub::IWifiStation& station_;
    mutable std::mutex mutex_;
    camera_hub::WifiCredentials pending_;
    ProvisioningSnapshot snapshot_{};
};

}