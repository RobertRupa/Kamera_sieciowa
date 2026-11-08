# Architecture: ESP-IDF C++ + modular MVC / ports and adapters

## Decision
Use **component-oriented, layered architecture**, with MVC at presentation boundaries and ports-and-adapters between application logic and ESP-IDF details. MVC itself is not sufficient to isolate hardware and FreeRTOS. Composition root wires services explicitly in `app_main`.

```text
WebGUI (browser MVC/MVVM)    LVGL view/controller      CLI/other transport
        | HTTP/JSON                 | view events
        +-------------+-------------+
                      v
             Application controllers / use cases
                      | contract interfaces (ports)
                      v
          Domain entities / policies / state machines
                      |
      +---------------+-----------------+
      |               |                 |
 WiFi/NVS adapter  Display adapter   HTTP/OTA/IO adapters
      |               |                 |
 ESP-IDF + FreeRTOS drivers + BSP / external radio
```

### Dependency direction
- `domain` depends on neither ESP-IDF nor UI. Owns pure rules/value objects/state machines.
- `application` depends on domain and **port interfaces**, not concrete implementations.
- `infrastructure` implements ports with NVS, Wi-Fi, esp_netif, esp_event, HTTP, LVGL, drivers and BSP.
- `presentation` depends on application service contracts; web receives JSON DTOs, LVGL receives view models.
- `main` (composition root) constructs adapters/services/controllers, registers callbacks and owns lifecycle.
- No reverse include from domain to infrastructure; no circular component dependencies.

## Recommended project tree
```text
firmware/
├── CMakeLists.txt
├── sdkconfig.defaults
├── sdkconfig.defaults.esp32s3   # optional target defaults
├── partitions.csv               # designed per target/profile
├── dependencies.lock            # commit resolved version locks
├── .vscode/
│   ├── extensions.json
│   └── settings.json            # workspace-local, avoid absolute machine paths
├── main/
│   ├── CMakeLists.txt
│   ├── idf_component.yml
│   └── app_main.cpp             # composition root, almost no business logic
├── components/
│   ├── app_domain/
│   │   ├── CMakeLists.txt
│   │   ├── include/app_domain/
│   │   │   ├── DeviceState.hpp
│   │   │   └── NetworkPolicy.hpp
│   │   └── src/
│   │       ├── DeviceState.cpp
│   │       └── NetworkPolicy.cpp
│   ├── app_contracts/
│   │   ├── CMakeLists.txt
│   │   └── include/app_contracts/
│   │       ├── IWifiService.hpp
│   │       ├── IConfigStore.hpp
│   │       ├── IProvisioningService.hpp
│   │       ├── IDeviceInfoService.hpp
│   │       ├── IDisplayService.hpp
│   │       ├── NetworkDto.hpp
│   │       └── AppResult.hpp
│   ├── app_services/
│   │   ├── CMakeLists.txt
│   │   ├── include/app_services/
│   │   │   ├── NetworkCoordinator.hpp
│   │   │   └── SettingsService.hpp
│   │   └── src/
│   │       ├── NetworkCoordinator.cpp
│   │       └── SettingsService.cpp
│   ├── connectivity/
│   │   ├── CMakeLists.txt
│   │   ├── include/connectivity/
│   │   │   └── EspWifiService.hpp
│   │   └── src/EspWifiService.cpp
│   ├── persistence/
│   │   ├── CMakeLists.txt
│   │   ├── include/persistence/EncryptedNvsStore.hpp
│   │   └── src/EncryptedNvsStore.cpp
│   ├── provisioning/
│   │   ├── CMakeLists.txt
│   │   ├── include/provisioning/ApWizard.hpp
│   │   └── src/ApWizard.cpp
│   ├── discovery/
│   │   ├── CMakeLists.txt
│   │   └── src/MdnsService.cpp
│   ├── web_api/
│   │   ├── CMakeLists.txt
│   │   ├── include/web_api/SettingsController.hpp
│   │   ├── src/SettingsController.cpp
│   │   └── assets/             # generated frontend asset staging
│   ├── ui_lvgl/                 # omit entirely without a display
│   │   ├── CMakeLists.txt
│   │   ├── include/ui_lvgl/DisplayPresenter.hpp
│   │   ├── src/DisplayPresenter.cpp
│   │   └── src/views/SettingsView.cpp
│   └── board_support/
│       ├── CMakeLists.txt
│       ├── include/board_support/BoardConfig.hpp
│       └── src/BoardInit.cpp
├── web/                         # Vue 3 + Vite + PrimeVue application
│   ├── package.json
│   └── src/
│       ├── api/                 # typed network clients
│       ├── views/               # MVC presentation views
│       ├── components/          # reusable controls
│       ├── stores/              # Pinia view state
│       ├── locales/             # en.json / pl.json
│       └── themes/
├── tests/                       # host tests + hardware tests
├── scripts/                     # build & flash wrappers
└── docs/                        # hardware truth, APIs, ADRs
```

The tree is a **template**, not a mandate to create empty folders or one-file wrapper classes. For a tiny project merge cohesive adjacent components; keep dependency boundaries. Each nontrivial class uses e.g. `Thing.hpp` + `Thing.cpp` in matching include/src component, not one monolithic `main.cpp`. Pure DTOs, enums, traits and small templates may be header-only.

## Hardware truth and runtime capabilities

Keep a versioned, verified board manifest in `docs/hardware/board.json`. Convert it into a compiled `BoardProfile` at the BSP boundary. Use an injected, read-only `CapabilityRegistry` to combine verified hardware, selected components and runtime service status; controllers (REST/LVGL) consume the same capability snapshot rather than probing pins or copying compile flags. Example and host-testable C++17 component: [Hardware and Capability Registry](hardware-and-capabilities.md).

## Boot lifecycle
1. `app_main` initializes logging, NVS key/security scheme, event loop, network interfaces, and resources.
2. Instantiate concrete adapters, validate partition/config schema, then inject them into application services.
3. Start HTTP/UI and provisioning capabilities per feature flags; trigger Wi-Fi STA or AP fallback.
4. Start mDNS **after** appropriate interface/IP readiness; update service records when state changes.
5. Start display/LVGL only if hardware is present and initialized successfully.
6. Enter event-driven operation; shut down subsystems in reverse order on restart/reconfigure when possible.

## Patterns (use when they solve a concrete problem)
- **Constructor DI**: `NetworkCoordinator(IWifiService&, IConfigStore&)`; no service locator.
- **Adapter**: wrap ESP-IDF C APIs, test with fakes.
- **Strategy**: selection of STA/AP provisioning, storage backend, transport or board variant.
- **State machine / State**: provisioning/connection/retry lifecycle with explicit legal transitions.
- **Observer/event bus**: bounded FreeRTOS queue; no cross-task calls directly into GUI.
- **Factory**: create BSP/display driver based on compile-time target.
- **RAII**: own NVS handles, mutexes, tasks, timers, HTTP handlers; prohibit copy when ownership unique.
- **Repository**: config store port hiding NVS schema and migrations.
Avoid applying every pattern by default; do not use inheritance for convenience when a plain function or composition is simpler.

## Threads, memory, failure
- Define event ownership, lock order, queue backpressure and maximum blocking time.
- No heap allocation or expensive JSON in ISR; use `FromISR` FreeRTOS functions and notify worker.
- Never call LVGL from HTTP callback; send commands to UI task, marshal through adapter.
- Prefer static or bounded allocations for hot paths, use PSRAM for framebuffers/large assets only when DMA/cache capabilities permit.
- Handle disconnect, bad credentials, full NVS, low memory, reboot during config write, AP client disconnect and network interface changes.
- Document per-target task stack, estimated heap at startup and after provisioning, LVGL buffer sizes, and watchdog policy.