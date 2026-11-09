#include "camera_hub/WifiCredentialCodec.hpp"

#include <algorithm>
#include <cstring>
#include <string_view>

#include "camera_hub/ProvisioningPolicy.hpp"

namespace camera_hub {

CredentialError WifiCredentialCodec::encode(const WifiCredentials& credentials,
                                            CredentialRecord& record) noexcept {
    record.fill(0);
    const auto ssidEnd = std::find(credentials.ssid.begin(), credentials.ssid.end(), '\0');
    const auto passwordEnd = std::find(credentials.passphrase.begin(), credentials.passphrase.end(), '\0');
    if (ssidEnd == credentials.ssid.end() || passwordEnd == credentials.passphrase.end()) {
        return CredentialError::InvalidArgument;
    }
    const std::string_view ssid(credentials.ssid.data(), ssidEnd - credentials.ssid.begin());
    const std::string_view password(credentials.passphrase.data(), passwordEnd - credentials.passphrase.begin());
    if (!ProvisioningPolicy::validSsid(ssid) || !ProvisioningPolicy::validPassphrase(password)) {
        return CredentialError::InvalidArgument;
    }
    record[0] = 1;
    record[1] = static_cast<std::uint8_t>(ssid.size());
    std::memcpy(record.data() + 2, ssid.data(), ssid.size());
    record[34] = static_cast<std::uint8_t>(password.size());
    std::memcpy(record.data() + 35, password.data(), password.size());
    return CredentialError::None;
}

CredentialError WifiCredentialCodec::decode(const CredentialRecord& record,
                                            WifiCredentials& credentials) noexcept {
    credentials.clear();
    if (record[0] != 1) {
        return CredentialError::UnsupportedVersion;
    }
    const auto ssidLength = record[1];
    const auto passwordLength = record[34];
    if (ssidLength == 0 || ssidLength > 32 || passwordLength < 8 || passwordLength > 63) {
        return CredentialError::CorruptRecord;
    }
    const std::string_view ssid(reinterpret_cast<const char*>(record.data() + 2), ssidLength);
    const std::string_view password(reinterpret_cast<const char*>(record.data() + 35), passwordLength);
    if (!ProvisioningPolicy::validSsid(ssid) || !ProvisioningPolicy::validPassphrase(password)) {
        return CredentialError::CorruptRecord;
    }
    std::memcpy(credentials.ssid.data(), ssid.data(), ssid.size());
    std::memcpy(credentials.passphrase.data(), password.data(), password.size());
    return CredentialError::None;
}

}