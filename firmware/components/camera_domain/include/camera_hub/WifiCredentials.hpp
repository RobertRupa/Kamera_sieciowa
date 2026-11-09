#pragma once

#include <array>
#include <cstdint>

namespace camera_hub {

enum class CredentialError : std::uint8_t {
    None, NotReady, InvalidArgument, NotFound,
    UnsupportedVersion, CorruptRecord, StorageFailure,
};

struct WifiCredentials {
    std::array<char, 33> ssid{};
    std::array<char, 64> passphrase{};

    ~WifiCredentials() { clear(); }
    void clear() noexcept {
        volatile char* ssidBytes = ssid.data();
        volatile char* passwordBytes = passphrase.data();
        for (std::size_t index = 0; index < ssid.size(); ++index) ssidBytes[index] = 0;
        for (std::size_t index = 0; index < passphrase.size(); ++index) passwordBytes[index] = 0;
    }
};

}