# AGENTS.md — authoritative instructions for coding assistants

Read this file **before** planning or editing any ESP32 application using these guidelines. Follow linked `docs/` sections for the affected subsystem. If this file is copied into another repository, resolve paths relative to that repository or carry the linked docs over. Project-specific hardware facts override generic examples, never safety/security invariants.

## Goal and interpretation
Create maintainable ESP-IDF C/C++ firmware with modular, testable domain/services, practical MVC for UI, and complete device onboarding. Act autonomously on reversible coding tasks; raise unknown hardware or irreversible eFuse decisions explicitly. Prefer a working minimal vertical slice to partially implemented infrastructure.

## Mandatory rules (MUST)
1. **Survey before edits**: read existing README, AGENTS, CMakeLists, Kconfig, sdkconfig.defaults*, idf_component.yml, dependencies.lock, partition table, schematic/pin map and relevant source. Preserve working behavior and public API compatibility unless the task explicitly changes them.
2. **Target truth**: document selected `IDF_TARGET`, chip capabilities, Wi-Fi source (on-chip, external coprocessor or absent), available RAM/PSRAM/Flash, attached display/touch and pins. ESP32-P4 has no internal Wi-Fi; ESP32-H2 is not a normal Wi-Fi target. Don't infer board wiring from a similar product.
3. **Architecture**: isolate `domain`, `application` services and `infrastructure`; UI controllers never access SDK directly. UI is MVC/MVVM on its edge, not an excuse for a giant controller. One responsibility per class and cohesive components; no global mutable singletons as the default DI mechanism.
4. **Interfaces/contracts**: define typed APIs before parallel implementation; distinguish sync results from asynchronous events and document ownership/thread context. Prefer dependency injection via constructors; use adapters for Wi-Fi/HTTP/NVS/LVGL/FreeRTOS.
5. **Baseline**: on Wi-Fi-capable builds ship AP wizard + captive portal + STA + mDNS + WebGUI; LVGL only if display supported. If the target lacks network capability, declare a dedicated external-radio adapter or explicit capability-limited profile, not fake Wi-Fi support.
6. **Credentials**: this project intentionally stores Wi-Fi credentials unencrypted in ordinary NVS. State clearly that passwords are recoverable from a Flash dump. Audit the ESP-IDF/companion driver persistence path so credentials are stored only in the application-owned NVS record; do not add NVS encryption, HMAC key derivation or eFuse provisioning.
7. **Web**: versioned REST contract, validated JSON, error schema, authentication for state-changing operations, localization (`en` default, `pl` included), persisted theme (`system`/`light`/`dark`) and responsive interface. Serve prepared static UI assets via ESP-IDF HTTP server; no Node server on device.
8. **Debug-only defaults**: `admin/admin` is acceptable in `CONFIG_APP_DEV_MODE=y` only. Release build must reject default credentials and disable unauthenticated debug/admin routes. Do not embed real SSIDs, passphrases, tokens or private keys.
9. **Safety and reliability**: callbacks/ISRs do minimum work; queues/event buses decouple subsystems; bound memory, buffers, timeouts, retries and queue lengths. Keep LVGL calls on its owning task/under its lock. Never block ESP event loop or HTTP callbacks on lengthy actions.
10. **Dependencies**: prefer latest compatible stable components from maintained upstream sources, declare via `idf_component.yml`, commit `dependencies.lock` in application repos for reproducibility; verify API and target support rather than guessing versions.
11. **Build/flash**: run incremental `idf.py build`; flash via `idf.py -p PORT flash`; monitor on request/need. Never add routine pre-flash backup, redundant manual checksum comparison, full erase, or arbitrary clean builds. Never disable built-in integrity or write protection checks.
12. **Verification**: cite build/test commands and outcome; never say built, flashed or tested unless executed and observed. No blanket `ESP_ERROR_CHECK` in recoverable user-input paths; propagate meaningful errors.
13. **Documentation**: update architecture/contracts/API/config/README whenever behavior or config changes. Report target, commands, changed files, tests, limitations, and possible eFuse/security impact.

## Design defaults
- C++ for business rules and services; C wrappers where ESP-IDF requires C ABI.
- SOLID, KISS, DRY, YAGNI and explicit lifetimes; composition over inheritance, RAII where appropriate.
- IDF Component Manager with per-module `CMakeLists.txt`; business logic tests compile on host without ESP-IDF whenever possible.
- Avoid exceptions/RTTI dependencies unless explicitly enabled and justified; check ESP-IDF compiler settings for chosen version.
- Use existing ESP-IDF drivers (`esp_lcd`, `esp_http_server`, `esp_netif`, `esp_event`, NVS, mDNS component), Espressif LVGL port, and a ready frontend framework; avoid handwritten HTTP stacks, bespoke LVGL schedulers or reinvention of captive portal.
- Match concurrency to actual platform: on ESP32-C3/C5/H2 one core or constrained resources may change task architecture; P4 with PSRAM can support larger GUI.

## Hardware manifest and feature gating

Each board-specific agent task MUST inspect and maintain the [Hardware Manifest + Capability Registry](docs/hardware-and-capabilities.md). Unknown GPIOs, Wi-Fi companion links, Flash/PSRAM or displays remain `unverified`; do not mark them `ready`. Plaintext NVS is project policy, not a security-readiness claim. Keep a consistent read-only runtime capability snapshot across WebGUI and LVGL; test derived dependencies (Wi-Fi, NVS, mDNS, display). Separate verified board facts from service readiness. The included example profile is illustrative, not a hardware fact sheet.

## Agent execution protocol
`DISCOVER → CONTRACT → PLAN → IMPLEMENT → VERIFY → DOCUMENT → REVIEW`.
Each agent owns agreed files; no overlapping writes. At contract stage define DTOs, namespaces, error codes, lifecycle, concurrency, resource budgets and tests. Parallelize independent tasks only after contracts stabilize. Coordinator integrates and owns the final build and flash. Follow [Agent workflow](docs/agent-swarm.md).

## Prohibited
- Inventing pinouts, HAL APIs, release versions, passing tests, board peripherals.
- Shipping debug `admin/admin` in release or exposing dev HTTP portal over public internet.
- Plaintext credentials in code/commits or logging passwords.
- Silently burning eFuses, secure boot, flash encryption, or erasing user data.
- Globally disabling safety checks in order to speed flash.
- Replacing supported framework facilities with parallel homegrown engines without a documented need.

## Definition of done
Read and apply [docs/project-checklist.md](docs/project-checklist.md) and [docs/quality-and-testing.md](docs/quality-and-testing.md); unfinished features must be listed as open items rather than presented as complete.