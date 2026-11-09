#include "persistence/NvsCredentialStore.hpp"
#include "camera_hub/WifiCredentialCodec.hpp"
#include "esp_partition.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace camera_hub;

void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::abort();
    }
}

struct FakeNvs {
    bool partitionPresent{true};
    bool recordPresent{false};
    std::size_t partitionSize{0x6000};
    std::size_t storedSize{98};
    esp_err_t initResult{ESP_OK};
    esp_err_t openResult{ESP_OK};
    esp_err_t writeResult{ESP_OK};
    esp_err_t commitResult{ESP_OK};
    int initCalls{0};
    int openCalls{0};
    int deinitCalls{0};
    int eraseCalls{0};
    int writeCalls{0};
    int commitCalls{0};
    CredentialRecord record{};
} fake;

const esp_partition_t* esp_partition_find_first(int, int, const char* name) {
    require(std::strcmp(name, "app_config") == 0, "dedicated partition");
    static esp_partition_t partition{0x6000};
    partition.size = fake.partitionSize;
    return fake.partitionPresent ? &partition : nullptr;
}
esp_err_t nvs_flash_init_partition(const char* name) {
    require(std::strcmp(name, "app_config") == 0, "plain partition initialization");
    ++fake.initCalls;
    return fake.initResult;
}
esp_err_t nvs_flash_deinit_partition(const char* name) {
    require(std::strcmp(name, "app_config") == 0, "partition deinitialization");
    ++fake.deinitCalls;
    return ESP_OK;
}
esp_err_t nvs_flash_erase_partition(const char*) {
    ++fake.eraseCalls;
    return ESP_OK;
}
esp_err_t nvs_open_from_partition(const char* partition, const char* space, int mode, nvs_handle_t* handle) {
    require(std::strcmp(partition, "app_config") == 0 && std::strcmp(space, "camera_hub") == 0,
            "scoped namespace");
    require(mode == NVS_READWRITE, "read-write credential namespace");
    ++fake.openCalls;
    if (fake.openResult == ESP_OK) *handle = 1;
    return fake.openResult;
}
void nvs_close(nvs_handle_t) {}
esp_err_t nvs_set_blob(nvs_handle_t, const char* key, const void* bytes, std::size_t length) {
    require(std::strcmp(key, "wifi_v1") == 0 && length == 98, "one bounded credential record");
    ++fake.writeCalls;
    if (fake.writeResult == ESP_OK) {
        std::memcpy(fake.record.data(), bytes, length);
        fake.recordPresent = true;
    }
    return fake.writeResult;
}
esp_err_t nvs_get_blob(nvs_handle_t, const char* key, void* bytes, std::size_t* length) {
    require(std::strcmp(key, "wifi_v1") == 0, "read application key only");
    if (!fake.recordPresent) return ESP_ERR_NVS_NOT_FOUND;
    if (bytes != nullptr) {
        require(*length >= fake.storedSize, "bounded read buffer");
        std::memcpy(bytes, fake.record.data(), fake.storedSize);
    }
    *length = fake.storedSize;
    return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t) { ++fake.commitCalls; return fake.commitResult; }
esp_err_t nvs_erase_key(nvs_handle_t, const char* key) {
    require(std::strcmp(key, "wifi_v1") == 0, "clear never erases partition or unrelated keys");
    if (!fake.recordPresent) return ESP_ERR_NVS_NOT_FOUND;
    fake.recordPresent = false;
    return ESP_OK;
}

int main() {
    WifiCredentials input;
    std::memcpy(input.ssid.data(), "test-fixture", 12);
    std::memcpy(input.passphrase.data(), "fixture-pass", 12);
    {
        fake = {};
        fake.partitionPresent = false;
        persistence::NvsCredentialStore missingPartition;
        require(missingPartition.initialize() == CredentialError::StorageFailure, "missing partition");
        require(fake.initCalls == 0, "missing partition is not initialized");
    }
    {
        fake = {};
        fake.initResult = ESP_ERR_NVS_NO_FREE_PAGES;
        persistence::NvsCredentialStore corruptPartition;
        require(corruptPartition.initialize() == CredentialError::StorageFailure, "corrupt partition refused");
        require(!corruptPartition.ready() && fake.eraseCalls == 0, "failed initialization never erases data");
    }
    {
        fake = {};
        fake.openResult = ESP_FAIL;
        persistence::NvsCredentialStore openFailure;
        require(openFailure.initialize() == CredentialError::StorageFailure, "namespace open failure");
        require(fake.deinitCalls == 1 && fake.eraseCalls == 0, "failed open only deinitializes");
    }
    {
        fake = {};
        persistence::NvsCredentialStore store;
        require(store.initialize() == CredentialError::None && store.ready(), "store ready after plain init");
        require(store.initialize() == CredentialError::None && fake.initCalls == 1, "idempotent init");
        WifiCredentials output;
        require(store.load(output) == CredentialError::NotFound, "empty store");
        require(store.save(input) == CredentialError::None && fake.commitCalls == 1, "one commit after write");
        require(store.load(output) == CredentialError::None, "load credentials");
        require(output.ssid == input.ssid && output.passphrase == input.passphrase, "roundtrip");
        fake.storedSize = 99;
        require(store.load(output) == CredentialError::CorruptRecord, "oversized record rejected before read");
        require(output.passphrase[0] == 0, "failed read clears secrets");
        fake.storedSize = 98;
        fake.record[0] = 2;
        require(store.load(output) == CredentialError::UnsupportedVersion, "future record version");
        fake.writeResult = ESP_FAIL;
        require(store.save(input) == CredentialError::StorageFailure, "write error propagated");
        fake.writeResult = ESP_OK;
        fake.commitResult = ESP_FAIL;
        require(store.save(input) == CredentialError::StorageFailure, "commit error propagated");
        fake.commitResult = ESP_OK;
        require(store.clear() == CredentialError::None && !fake.recordPresent, "clear own record");
        require(store.clear() == CredentialError::None, "idempotent clear");
        require(fake.eraseCalls == 0, "credential operations never erase the partition");
    }
    std::puts("PASS: plain NVS credential-store contract using fake NVS");
}