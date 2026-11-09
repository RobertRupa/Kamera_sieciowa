#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "board_capabilities/BoardProfile.hpp"

namespace board_capabilities {

enum class Feature : std::uint8_t {
    Wifi,
    WifiWizard,
    CaptivePortal,
    Mdns,
    WebGui,
    Display,
    Touch,
    Lvgl,
    CredentialStorage,
    Camera,
    Count,
};

enum class CapabilityState : std::uint8_t {
    Unsupported,  // Physical hardware absent, or required hardware absent.
    Unverified,   // Hardware/wiring/key protection unverified.
    Disabled,     // Feature not compiled/selected for this firmware.
    NotReady,     // Feature built but runtime initialization is pending/failed.
    Ready,        // Snapshot indicates prerequisites and service are running.
};

// Build-time component selection. Defaults are deliberately safe/off.
// Fill from Kconfig and linked modules at the application's composition root.
struct BuildFeatures {
    bool wifiAdapter{false};
    bool wifiWizard{false};
    bool captivePortal{false};
    bool mdns{false};
    bool webgui{false};
    bool displayDriver{false};
    bool touchDriver{false};
    bool lvgl{false};
    bool credentialStorage{false};
    bool cameraCapture{false};
};

// Consistent snapshot captured by the application from live service contracts.
// No ESP-IDF calls, global mutable state, credentials or raw driver handles.
struct RuntimeState {
    bool wifiLinkReady{false};
    bool wizardRunning{false};
    bool captivePortalRunning{false};
    bool ipReachable{false};     // Configured IP on AP/STA that hosts WebGUI.
    bool mdnsRunning{false};
    bool webServerRunning{false};
    bool displayReady{false};
    bool touchReady{false};
    bool lvglRunning{false};
    bool credentialStorageReady{false};
    bool cameraCaptureReady{false};
};

constexpr std::size_t kFeatureCount = static_cast<std::size_t>(Feature::Count);
using CapabilitySnapshot = std::array<CapabilityState, kFeatureCount>;

// Immutable service: no heap allocations, I/O, FreeRTOS locks or ESP-IDF dependency.
// Pass RuntimeState by value/snapshot from the orchestrator, not volatile globals.
class CapabilityRegistry final {
public:
    CapabilityRegistry(BoardProfile board, BuildFeatures built) noexcept;

    [[nodiscard]] CapabilityState get(Feature feature,
                                      const RuntimeState& live) const noexcept;
    [[nodiscard]] CapabilitySnapshot snapshot(const RuntimeState& live) const noexcept;

    [[nodiscard]] static const char* featureKey(Feature feature) noexcept;
    [[nodiscard]] static const char* stateKey(CapabilityState state) noexcept;

private:
    BoardProfile board_;
    BuildFeatures built_;
};

}  // namespace board_capabilities
