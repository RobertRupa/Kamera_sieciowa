# CapabilityRegistry — ESP-IDF component and host-testable C++17 example

This component is a reusable **domain/application contract**, not a hardware driver. It implements a deterministic capability-state projection over verified static board facts, compiled features and live service readiness. Pure C++17, one class per matching header and source, no global mutable state, dynamic allocations or ESP-IDF dependencies.

## Source files
- `include/board_capabilities/BoardProfile.hpp`: board profile DTO, wireless topology and verification states.
- `src/BoardProfile.cpp`: deliberately **unverified** ESP32-P4 + ESP32-C6 demonstration profile. Never ship it as production board facts.
- `include/board_capabilities/CapabilityRegistry.hpp`: feature enum, build/runtime inputs, immutable registry API.
- `src/CapabilityRegistry.cpp`: readiness and dependency logic.
- `tests/`: self-contained host tests with C++17 compiler.

## Host test
```bash
bash templates/esp-idf/components/board_capabilities/tests/run_host_tests.sh
```

## Example integration into a real ESP-IDF app
Include as a standard project component; in application's `main/CMakeLists.txt` add `PRIV_REQUIRES board_capabilities`. In `app_main` create a **verified** board profile, derive `BuildFeatures` from Kconfig/compiler link choices, collect `RuntimeState` from actual Wi-Fi/NVS/LVGL/HTTP services, then construct:

```cpp
#include "board_capabilities/CapabilityRegistry.hpp"

// Example inside composition root; boardProfile, compiledModules and serviceSnapshot
// must be constructed/collected elsewhere from verified sources.
board_capabilities::CapabilityRegistry registry(boardProfile, compiledModules);
const auto web = registry.get(board_capabilities::Feature::WebGui, serviceSnapshot);
if (web == board_capabilities::CapabilityState::Ready) {
    // Pass the snapshot to the WebGUI controller / Vue UI.
}
```

No service is initialized by this component. Integrate an ESP-IDF adapter that:
- verifies expected target and peripheral drivers against the manifest;
- supplies runtime status from actual initialized services;
- exposes a **read-only** `GET /api/v1/device/capabilities`, serialized using `featureKey()`/`stateKey()`; production endpoints require authentication;
- projects the same snapshot into LVGL presenter if installed.

Use a consistent runtime snapshot for all capabilities to avoid torn/cross-task reads; reconcile if service statuses are updated concurrently. Registry readiness is advisory, not authorization or a transactional guarantee. Recheck permissions and actual state in every action handler.

A direct `wifiLinkReady` signal does not mean an assigned IP, and an HTTP server running does not imply it is reachable. Do not set `credentialStorageReady` from a config flag alone: verify ordinary NVS initialization and Wi-Fi driver persistence path.

See [Security profiles](../../../../docs/security-and-profiles.md) and [Hardware and Capability Registry](../../../../docs/hardware-and-capabilities.md).