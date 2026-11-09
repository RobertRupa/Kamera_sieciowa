#include "app_services/NetworkProvisioner.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace camera_hub;
using namespace app_services;

void require(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::abort(); }
}

class Store final : public ICredentialStore {
public:
    bool available{true};
    CredentialError result{CredentialError::None};
    int saves{0};
    int loads{0};
    CredentialError loadResult{CredentialError::NotFound};
    bool ready() const noexcept override { return available; }
    CredentialError save(const WifiCredentials&) noexcept override { ++saves; return result; }
    CredentialError load(WifiCredentials& credentials) noexcept override {
        ++loads;
        if (loadResult == CredentialError::None) {
            std::memcpy(credentials.ssid.data(), "restored-network", 16);
            std::memcpy(credentials.passphrase.data(), "restored-pass", 13);
        }
        return loadResult;
    }
    CredentialError clear() noexcept override { return CredentialError::None; }
};
class Station final : public IWifiStation {
public:
    Store& store;
    int calls{0};
    bool hasIp{false};
    WifiError result{WifiError::None};
    explicit Station(Store& source) : store(source) {}
    WifiError connectStation(const WifiCredentials&) noexcept override {
        require(store.saves > 0 || store.loads > 0, "persisted or restored credentials precede driver connection");
        ++calls;
        return result;
    }
    bool stationHasIp() const noexcept override { return hasIp; }
};

int main() {
    Store restoreStore;
    Station restoreStation(restoreStore);
    NetworkProvisioner restoreService(restoreStore, restoreStation);
    require(restoreService.restoreSavedCredentials() == RestoreResult::NoCredentials, "empty store stays in setup mode");
    require(restoreStation.calls == 0 && restoreService.snapshot().state == ProvisioningState::Idle, "no credentials means no station attempt");
    restoreStore.loadResult = CredentialError::None;
    require(restoreService.restoreSavedCredentials() == RestoreResult::Started, "stored credentials start station on boot");
    require(restoreStore.saves == 0 && restoreStore.loads == 2 && restoreStation.calls == 1, "restore reads without rewriting credentials");
    require(restoreService.snapshot().state == ProvisioningState::Connecting, "restored network awaits connection confirmation");
    restoreService.updateConnection(true);
    restoreStore.loadResult = CredentialError::NotFound;
    require(restoreService.restoreSavedCredentials() == RestoreResult::NoCredentials, "missing credentials remain recoverable");

    Store store;
    Station station(store);
    NetworkProvisioner service(store, station);
    WifiCredentials credentials;
    std::memcpy(credentials.ssid.data(), "test-fixture", 12);
    std::memcpy(credentials.passphrase.data(), "fixture-pass", 12);
    std::uint32_t operation = 0;
    store.available = false;
    require(service.submit(credentials, operation) == SubmitResult::NotReady && operation == 0, "store unavailable");
    store.available = true;
    require(service.submit(credentials, operation) == SubmitResult::Accepted && operation == 1, "queued operation");
    require(store.saves == 0 && station.calls == 0, "submit performs no storage or driver I/O");
    require(service.submit(credentials, operation) == SubmitResult::Busy, "one bounded pending operation");
    store.result = CredentialError::StorageFailure;
    service.processPending();
    require(service.snapshot().state == ProvisioningState::Failed && station.calls == 0, "no connection after failed write");
    store.result = CredentialError::None;
    require(service.submit(credentials, operation) == SubmitResult::Accepted, "retry failure");
    service.processPending();
    require(service.snapshot().state == ProvisioningState::Connecting && station.calls == 1, "request is not connected");
    service.updateConnection(false);
    require(service.snapshot().state == ProvisioningState::Connecting, "wait for real IP");
    service.updateConnection(true);
    require(service.snapshot().state == ProvisioningState::Failed, "bounded timeout");
    require(service.submit(credentials, operation) == SubmitResult::Accepted, "retry timeout");
    service.processPending();
    station.hasIp = true;
    service.updateConnection(false);
    require(service.snapshot().state == ProvisioningState::Connected, "IP confirms connected");
    credentials.clear();
    require(service.submit(credentials, operation) == SubmitResult::InvalidArgument, "invalid credentials rejected");
    std::puts("PASS: queued provisioning, secure-write ordering, failure and real-IP confirmation");
}