#include "camera_hub/WifiCredentialCodec.hpp"

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

int main() {
    WifiCredentials input;
    std::memcpy(input.ssid.data(), "test-fixture", 12);
    std::memcpy(input.passphrase.data(), "fixture-pass", 12);
    CredentialRecord record{};
    require(WifiCredentialCodec::encode(input, record) == CredentialError::None, "valid credential record");
    require(record[0] == 1 && record[1] == 12 && record[34] == 12, "stable version and lengths");
    WifiCredentials output;
    require(WifiCredentialCodec::decode(record, output) == CredentialError::None, "decode record");
    require(input.ssid == output.ssid && input.passphrase == output.passphrase, "roundtrip");
    record[0] = 2;
    require(WifiCredentialCodec::decode(record, output) == CredentialError::UnsupportedVersion, "unknown version");
    require(output.ssid[0] == 0 && output.passphrase[0] == 0, "failure clears old credentials");
    record[0] = 1;
    record[1] = 33;
    require(WifiCredentialCodec::decode(record, output) == CredentialError::CorruptRecord, "SSID length overflow");
    record[1] = 12;
    record[34] = 64;
    require(WifiCredentialCodec::decode(record, output) == CredentialError::CorruptRecord, "password length overflow");
    record[34] = 12;
    record[2] = 0;
    require(WifiCredentialCodec::decode(record, output) == CredentialError::CorruptRecord, "embedded null rejected");
    input.ssid.fill('a');
    require(WifiCredentialCodec::encode(input, record) == CredentialError::InvalidArgument, "unterminated SSID");
    input.ssid.fill(0);
    input.ssid.fill('a');
    input.ssid.back() = 0;
    input.passphrase.fill('b');
    input.passphrase.back() = 0;
    require(WifiCredentialCodec::encode(input, record) == CredentialError::None, "maximum lengths");
    require(WifiCredentialCodec::decode(record, output) == CredentialError::None, "maximum lengths decode");
    require(output.ssid == input.ssid && output.passphrase == input.passphrase, "maximum lengths roundtrip");
    input.clear();
    require(WifiCredentialCodec::encode(input, record) == CredentialError::InvalidArgument, "empty credentials");
    std::puts("PASS: versioned Wi-Fi record, bounds, roundtrip and failure clearing");
}