#include "board_capabilities/CapabilityRegistry.hpp"

namespace board_capabilities {
namespace {

CapabilityState verify(Verification present) noexcept {
    switch (present) {
        case Verification::Absent: return CapabilityState::Unsupported;
        case Verification::Unverified: return CapabilityState::Unverified;
        case Verification::Verified: return CapabilityState::Ready;
    }
    return CapabilityState::Unverified; // Fail closed for unexpected values.
}

// Propagate the most important unresolved dependency, never invent readiness.
CapabilityState dependencies(CapabilityState first, CapabilityState second) noexcept {
    if (first == CapabilityState::Unsupported || second == CapabilityState::Unsupported)
        return CapabilityState::Unsupported;
    if (first == CapabilityState::Unverified || second == CapabilityState::Unverified)
        return CapabilityState::Unverified;
    if (first == CapabilityState::Disabled || second == CapabilityState::Disabled)
        return CapabilityState::Disabled;
    if (first == CapabilityState::NotReady || second == CapabilityState::NotReady)
        return CapabilityState::NotReady;
    return CapabilityState::Ready;
}

CapabilityState enabledAndRunning(bool enabled, bool running) noexcept {
    if (!enabled) return CapabilityState::Disabled;
    return running ? CapabilityState::Ready : CapabilityState::NotReady;
}

CapabilityState withHardware(Verification hardware, bool enabled, bool running) noexcept {
    const auto verified = verify(hardware);
    if (verified != CapabilityState::Ready) return verified;
    return enabledAndRunning(enabled, running);
}

} // namespace

CapabilityRegistry::CapabilityRegistry(BoardProfile board, BuildFeatures built) noexcept
    : board_(board), built_(built) {}

CapabilityState CapabilityRegistry::get(Feature feature, const RuntimeState& live) const noexcept {
    switch (feature) {
        case Feature::Wifi:
            if (board_.wifiTopology == WifiTopology::None)
                return CapabilityState::Unsupported;
            return withHardware(board_.wifiRadio, built_.wifiAdapter, live.wifiLinkReady);

        case Feature::CredentialStorage:
            return withHardware(board_.credentialStoragePartition,
                                built_.credentialStorage, live.credentialStorageReady);

        case Feature::Camera:
            return withHardware(board_.camera, built_.cameraCapture, live.cameraCaptureReady);

        case Feature::Display:
            return withHardware(board_.display, built_.displayDriver, live.displayReady);

        case Feature::Touch:
            return withHardware(board_.touch, built_.touchDriver, live.touchReady);

        case Feature::Lvgl: {
            const auto display = get(Feature::Display, live);
            if (display != CapabilityState::Ready) return display;
            return enabledAndRunning(built_.lvgl, live.lvglRunning);
        }

        case Feature::WifiWizard: {
            // Do not offer credential persistence until ordinary NVS is ready.
            const auto deps = dependencies(get(Feature::Wifi, live),
                                           get(Feature::CredentialStorage, live));
            if (deps != CapabilityState::Ready) return deps;
            return enabledAndRunning(built_.wifiWizard, live.wizardRunning);
        }

        case Feature::CaptivePortal: {
            const auto wizard = get(Feature::WifiWizard, live);
            if (wizard != CapabilityState::Ready) return wizard;
            return enabledAndRunning(built_.captivePortal, live.captivePortalRunning);
        }

        case Feature::Mdns: {
            const auto wifi = get(Feature::Wifi, live);
            if (wifi != CapabilityState::Ready) return wifi;
            return enabledAndRunning(built_.mdns, live.ipReachable && live.mdnsRunning);
        }

        case Feature::WebGui: {
            const auto wifi = get(Feature::Wifi, live);
            if (wifi != CapabilityState::Ready) return wifi;
            return enabledAndRunning(built_.webgui,
                                     live.ipReachable && live.webServerRunning);
        }

        case Feature::Count:
            break;
    }
    return CapabilityState::Unverified;
}

CapabilitySnapshot CapabilityRegistry::snapshot(const RuntimeState& live) const noexcept {
    CapabilitySnapshot states{};
    for (std::size_t i = 0; i < states.size(); ++i)
        states[i] = get(static_cast<Feature>(i), live);
    return states;
}

const char* CapabilityRegistry::featureKey(Feature feature) noexcept {
    switch (feature) {
        case Feature::Wifi: return "wifi";
        case Feature::WifiWizard: return "wifi_wizard";
        case Feature::CaptivePortal: return "captive_portal";
        case Feature::Mdns: return "mdns";
        case Feature::WebGui: return "webgui";
        case Feature::Display: return "display";
        case Feature::Touch: return "touch";
        case Feature::Lvgl: return "lvgl";
        case Feature::CredentialStorage: return "credential_storage";
        case Feature::Camera: return "camera";
        case Feature::Count: break;
    }
    return "unknown";
}

const char* CapabilityRegistry::stateKey(CapabilityState state) noexcept {
    switch (state) {
        case CapabilityState::Unsupported: return "unsupported";
        case CapabilityState::Unverified: return "unverified";
        case CapabilityState::Disabled: return "disabled";
        case CapabilityState::NotReady: return "not_ready";
        case CapabilityState::Ready: return "ready";
    }
    return "unverified";
}

} // namespace board_capabilities
