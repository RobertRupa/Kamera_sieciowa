#include "app_services/NetworkScanService.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace camera_hub;
using namespace app_services;

void require(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::abort(); }
}

class Scanner final : public IWifiScanner {
public:
    WifiError startResult{WifiError::None};
    WifiError collectResult{WifiError::None};
    bool complete{false};
        bool cancelled{false};
    int startCalls{0};
    WifiError startScan() noexcept override { ++startCalls; return startResult; }
    bool scanComplete() const noexcept override { return complete; }
        void cancelScan() noexcept override { cancelled = true; }
    WifiError collectScanResults(WifiScanResult* results, std::size_t capacity,
                                 std::size_t& count) noexcept override {
        count = 0;
        if (collectResult != WifiError::None) return collectResult;
        if (capacity == 0) return WifiError::InvalidArgument;
        std::memcpy(results[0].ssid.data(), "fixture-network", 15);
        results[0].rssi = -42;
        results[0].security = WifiSecurity::Wpa2Psk;
        count = 1;
        return WifiError::None;
    }
};

int main() {
    Scanner scanner;
    NetworkScanService service(scanner);
    std::uint32_t operation = 0;
    require(service.start(operation) == NetworkScanStartResult::Accepted && operation == 1,
            "scan starts as an asynchronous operation");
    require(service.snapshot().state == NetworkScanState::Scanning, "operation remains pending");
    require(service.start(operation) == NetworkScanStartResult::Busy && operation == 0,
            "only one scan can be active");
    service.processPending();
    require(service.snapshot().state == NetworkScanState::Scanning, "pending scan is not collected early");
    scanner.complete = true;
    service.processPending();
    const auto completed = service.snapshot();
    require(completed.state == NetworkScanState::Complete && completed.resultCount == 1,
            "completed results are published");
    require(std::strcmp(completed.results[0].ssid.data(), "fixture-network") == 0 &&
            completed.results[0].rssi == -42 && completed.results[0].security == WifiSecurity::Wpa2Psk,
            "scan result fields are preserved");
    require(service.start(operation) == NetworkScanStartResult::Accepted && operation == 2,
            "completed scan can be retried with a new operation id");
    scanner.complete = false;
    service.processPending(true);
    require(scanner.cancelled && service.snapshot().state == NetworkScanState::Failed,
            "timed out scan is cancelled and releases the active operation");
    require(service.start(operation) == NetworkScanStartResult::Accepted && operation == 3,
            "a timed out scan does not block retry");
    scanner.complete = true;
    scanner.collectResult = WifiError::DriverFailure;
    service.processPending();
    require(service.snapshot().state == NetworkScanState::Failed && service.snapshot().resultCount == 0,
            "driver failure publishes no partial results");
    scanner.startResult = WifiError::NotReady;
    require(service.start(operation) == NetworkScanStartResult::NotReady && operation == 0,
            "radio readiness is reported without creating an operation");
    std::puts("PASS: asynchronous bounded Wi-Fi scan operation lifecycle");
}