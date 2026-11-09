#include "app_services/NetworkProvisioner.hpp"

#include "camera_hub/WifiCredentialCodec.hpp"

namespace app_services {

NetworkProvisioner::NetworkProvisioner(camera_hub::ICredentialStore& store,
                                       camera_hub::IWifiStation& station) noexcept
    : store_(store), station_(station) {}

SubmitResult NetworkProvisioner::submit(const camera_hub::WifiCredentials& credentials,
                                        std::uint32_t& operationId) noexcept {
    operationId = 0;
    camera_hub::CredentialRecord record{};
    const auto validation = camera_hub::WifiCredentialCodec::encode(credentials, record);
    volatile std::uint8_t* bytes = record.data();
    for (std::size_t index = 0; index < record.size(); ++index) bytes[index] = 0;
    if (validation != camera_hub::CredentialError::None) return SubmitResult::InvalidArgument;
    if (!store_.ready()) return SubmitResult::NotReady;
    const std::lock_guard<std::mutex> lock(mutex_);
    if (snapshot_.state == ProvisioningState::Queued || snapshot_.state == ProvisioningState::Persisting ||
        snapshot_.state == ProvisioningState::Connecting) return SubmitResult::Busy;
    pending_ = credentials;
    ++snapshot_.operationId;
    if (snapshot_.operationId == 0) ++snapshot_.operationId;
    snapshot_.state = ProvisioningState::Queued;
    snapshot_.storageError = camera_hub::CredentialError::None;
    snapshot_.wifiError = camera_hub::WifiError::None;
    operationId = snapshot_.operationId;
    return SubmitResult::Accepted;
}

RestoreResult NetworkProvisioner::restoreSavedCredentials() noexcept {
    if (!store_.ready()) return RestoreResult::StoreUnavailable;
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        if (snapshot_.state == ProvisioningState::Queued || snapshot_.state == ProvisioningState::Persisting ||
            snapshot_.state == ProvisioningState::Connecting) return RestoreResult::Busy;
        snapshot_.state = ProvisioningState::Persisting;
    }

    camera_hub::WifiCredentials credentials;
    const auto loaded = store_.load(credentials);
    if (loaded == camera_hub::CredentialError::NotFound) {
        const std::lock_guard<std::mutex> lock(mutex_);
        snapshot_.state = ProvisioningState::Idle;
        snapshot_.storageError = camera_hub::CredentialError::None;
        snapshot_.wifiError = camera_hub::WifiError::None;
        return RestoreResult::NoCredentials;
    }
    if (loaded != camera_hub::CredentialError::None) {
        const std::lock_guard<std::mutex> lock(mutex_);
        snapshot_.state = ProvisioningState::Failed;
        snapshot_.storageError = loaded;
        snapshot_.wifiError = camera_hub::WifiError::None;
        return RestoreResult::StorageFailure;
    }

    const auto requested = station_.connectStation(credentials);
    credentials.clear();
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        ++snapshot_.operationId;
        if (snapshot_.operationId == 0) ++snapshot_.operationId;
        snapshot_.storageError = camera_hub::CredentialError::None;
        snapshot_.wifiError = requested;
        snapshot_.state = requested == camera_hub::WifiError::None
            ? ProvisioningState::Connecting : ProvisioningState::Failed;
    }
    return requested == camera_hub::WifiError::None ? RestoreResult::Started : RestoreResult::WifiFailure;
}

void NetworkProvisioner::processPending() noexcept {
    camera_hub::WifiCredentials credentials;
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        if (snapshot_.state != ProvisioningState::Queued) return;
        snapshot_.state = ProvisioningState::Persisting;
        credentials = pending_;
        pending_.clear();
    }
    const auto stored = store_.save(credentials);
    const auto requested = stored == camera_hub::CredentialError::None
        ? station_.connectStation(credentials) : camera_hub::WifiError::None;
    credentials.clear();
    const std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.storageError = stored;
    snapshot_.wifiError = requested;
    snapshot_.state = stored == camera_hub::CredentialError::None && requested == camera_hub::WifiError::None
        ? ProvisioningState::Connecting : ProvisioningState::Failed;
}

void NetworkProvisioner::updateConnection(bool timedOut) noexcept {
    std::uint32_t operationId = 0;
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        if (snapshot_.state != ProvisioningState::Connecting) return;
        operationId = snapshot_.operationId;
    }
    const bool hasIp = station_.stationHasIp();
    const std::lock_guard<std::mutex> lock(mutex_);
    if (snapshot_.state != ProvisioningState::Connecting || snapshot_.operationId != operationId) return;
    if (hasIp) snapshot_.state = ProvisioningState::Connected;
    else if (timedOut) snapshot_.state = ProvisioningState::Failed;
}

ProvisioningSnapshot NetworkProvisioner::snapshot() const noexcept {
    const std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_;
}

}