#include <cassert>
#include <cstring>
#include <iostream>
#include "board_capabilities/BoardProfile.hpp"
#include "board_capabilities/CapabilityRegistry.hpp"

using namespace board_capabilities;

int main() {
    const auto example = makeIllustrativeP4C6Profile();
    assert(std::strcmp(example.idfTarget, "esp32p4") == 0);
    assert(example.wifiTopology == WifiTopology::Companion);

    BuildFeatures compiled{};
    compiled.wifiAdapter = true;
    compiled.wifiWizard = true;
    compiled.captivePortal = true;
    compiled.mdns = true;
    compiled.webgui = true;
    compiled.displayDriver = true;
    compiled.touchDriver = true;
    compiled.lvgl = true;
    compiled.credentialStorage = true;
    RuntimeState runtime{};

    CapabilityRegistry unknown(example, compiled);
    assert(unknown.get(Feature::Wifi, runtime) == CapabilityState::Unverified);
    assert(unknown.get(Feature::Display, runtime) == CapabilityState::Unverified);
    assert(unknown.get(Feature::CredentialStorage, runtime) == CapabilityState::Unverified);
    assert(unknown.get(Feature::WifiWizard, runtime) == CapabilityState::Unverified);

    // Simulation of an independently confirmed board, not actual hardware verification.
    auto verified = example;
    verified.wifiRadio = Verification::Verified;
    verified.display = Verification::Verified;
    verified.touch = Verification::Verified;
    verified.credentialStoragePartition = Verification::Verified;
    CapabilityRegistry registry(verified, compiled);

    assert(registry.get(Feature::Wifi, runtime) == CapabilityState::NotReady);
    assert(registry.get(Feature::WebGui, runtime) == CapabilityState::NotReady);
    runtime.wifiLinkReady = true;
    assert(registry.get(Feature::Wifi, runtime) == CapabilityState::Ready);
    assert(registry.get(Feature::CredentialStorage, runtime) == CapabilityState::NotReady);
    assert(registry.get(Feature::WifiWizard, runtime) == CapabilityState::NotReady);

    runtime.wizardRunning = true;
    runtime.captivePortalRunning = true;
    assert(registry.get(Feature::WifiWizard, runtime) == CapabilityState::NotReady);
    assert(registry.get(Feature::CaptivePortal, runtime) == CapabilityState::NotReady);
    runtime.credentialStorageReady = true;
    assert(registry.get(Feature::CredentialStorage, runtime) == CapabilityState::Ready);
    assert(registry.get(Feature::WifiWizard, runtime) == CapabilityState::Ready);
    assert(registry.get(Feature::CaptivePortal, runtime) == CapabilityState::Ready);

    runtime.webServerRunning = true;
    runtime.mdnsRunning = true;
    assert(registry.get(Feature::WebGui, runtime) == CapabilityState::NotReady);
    assert(registry.get(Feature::Mdns, runtime) == CapabilityState::NotReady);
    runtime.ipReachable = true;
    assert(registry.get(Feature::WebGui, runtime) == CapabilityState::Ready);
    assert(registry.get(Feature::Mdns, runtime) == CapabilityState::Ready);

    runtime.displayReady = true;
    assert(registry.get(Feature::Display, runtime) == CapabilityState::Ready);
    assert(registry.get(Feature::Lvgl, runtime) == CapabilityState::NotReady);
    runtime.lvglRunning = true;
    assert(registry.get(Feature::Lvgl, runtime) == CapabilityState::Ready);
    assert(registry.get(Feature::Touch, runtime) == CapabilityState::NotReady);
    runtime.touchReady = true;
    assert(registry.get(Feature::Touch, runtime) == CapabilityState::Ready);

    const auto snap = registry.snapshot(runtime);
    for (const auto state : snap) assert(state == CapabilityState::Ready);
    assert(std::strcmp(CapabilityRegistry::featureKey(Feature::WifiWizard),
                       "wifi_wizard") == 0);
    assert(std::strcmp(CapabilityRegistry::stateKey(CapabilityState::NotReady),
                       "not_ready") == 0);

    auto headless = verified;
    headless.display = Verification::Absent;
    headless.touch = Verification::Absent;
    headless.wifiTopology = WifiTopology::None;
    CapabilityRegistry noRadioOrDisplay(headless, compiled);
    assert(noRadioOrDisplay.get(Feature::Wifi, runtime) == CapabilityState::Unsupported);
    assert(noRadioOrDisplay.get(Feature::WebGui, runtime) == CapabilityState::Unsupported);
    assert(noRadioOrDisplay.get(Feature::Display, runtime) == CapabilityState::Unsupported);
    assert(noRadioOrDisplay.get(Feature::Lvgl, runtime) == CapabilityState::Unsupported);

    auto noWeb = compiled;
    noWeb.webgui = false;
    CapabilityRegistry webDisabled(verified, noWeb);
    assert(webDisabled.get(Feature::WebGui, runtime) == CapabilityState::Disabled);

    runtime.wifiLinkReady = false;
    assert(snap[static_cast<std::size_t>(Feature::Wifi)] == CapabilityState::Ready);
    assert(registry.get(Feature::Wifi, runtime) == CapabilityState::NotReady);

    std::cout << "CapabilityRegistry C++17 host tests PASS\n";
    return 0;
}
