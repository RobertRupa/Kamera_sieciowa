# Engineering principles and C/C++ conventions

## Priority order
Correctness + hardware truth > safety/security invariants > compatibility > clarity > measured resource use > cleverness. Apply **KISS** and **YAGNI** first; prefer two small clear classes to a speculative abstract framework.

| Principle | Concrete rule | Anti-pattern |
| --- | --- | --- |
| SRP | one purpose/change reason per component/class | `GodService` handling Wi-Fi, NVS, UI, HTTP |
| OCP | add a driver by implementing a port | modify dozens of controllers |
| LSP | adapters honor every service contract | mocks behave unlike production |
| ISP | small `IWifiService` / `IConfigStore` ports | 40-method `IDeviceManager` |
| DIP | use case depends on interface | include `esp_wifi.h` from domain |
| DRY | one source for configuration validation, common DTO | duplicate SSID validation in web/LVGL/serial |
| KISS | smallest robust implementation | dynamic plugin loader for one board |
| YAGNI | build interfaces only at real boundaries | over-generalized abstractions with one trivial use |
| SoC | split transport, presentation, state and storage | direct NVS writes in views |
| Fail fast | validate irreversible config before action | delayed errors in flash/eFuse steps |
| Explicitness | clear units, ownership, error codes | magic integers and implicit task ownership |

## Code style
- C++ in application modules; `extern "C" void app_main(void)` bridge for ESP-IDF.
- Prefer `enum class`, `constexpr`, `std::array`, `std::span` when toolchain/target allow, fixed-width integers, strong types and named constants.
- Prefer construction with non-owning injected references for services with common lifecycle; use `std::unique_ptr` for exclusive dynamic ownership only where necessary.
- Use `const` everywhere possible, `[[nodiscard]]` for results, namespaces and include guards/`#pragma once` consistently.
- For error handling at SDK edges, map `esp_err_t` to typed application errors; avoid exceptions unless enabled and justified by cost/toolchain.
- Disable copying for hardware resource owners, avoid global mutable state, avoid raw owning `new/delete`. Do not create a `shared_ptr` forest or use virtual inheritance for every tiny helper.
- One public class per matching header/source pair for significant classes; helper data types may share a header. Keep public `include/` minimal and implementation in `src/`.
- Document *why*, not obvious *what*; comments for constraints/timing/pin routing/ownership, no generated verbosity.
- Names: `PascalCase` types, `camelCase` methods/local variables or project-consistent `snake_case`; choose once per project; `kConstants`; prefix implementation only where useful. Avoid cryptic abbreviations.
- Header files expose stable contracts without dragging in board headers; C APIs wrapped by C++ adapters.
- Explicit UTF-8 for translations, stable machine-readable translation keys, no hard-coded language strings in UI classes.

## Configuration hierarchy
`Board hardware manifest → sdkconfig.defaults/Kconfig → compile-time feature flags → plaintext runtime settings (NVS) → transient state`. Do not hardcode GPIO pin numbers inside business logic. Validate a persisted settings schema version; migrate or reset only owned namespaces, never entire Flash as routine recovery.

## Real-time policy
- Never delay in a network event callback, LVGL tick hook or ISR.
- Prefer `esp_timer` for short timers and FreeRTOS tasks/queues for work. Configure stack/cpu affinity only after measuring.
- Avoid busy loops, synchronous waits in HTTP handlers, unbounded memory/string growth, `printf` spam and unnecessary polling.
- Rate-limit logs in high-frequency code and redact secrets. Use `ESP_LOGx` tags, tune log levels by profile.
- Define task priorities based on actual latency requirements. Protect shared state with a mutex or message ownership rather than `volatile` alone.
- Watchdog resets indicate a bug or stall: investigate, don't simply disable watchdog.

## Resource discipline
Record Flash and heap budget; test Wi-Fi + WebGUI + LVGL simultaneously. Identify PSRAM/DMA restrictions for displays. Keep network response size, JSON limits, request body size, authentication session count, framebuffer count, stack size, queue depth, and retry count bounded and documented.

## API and change discipline
- Contract-first changes: update public interface and tests before adapters.
- No breaking API or NVS schema change without versioning/migration note.
- No ad-hoc copying of third-party libraries: use upstream component registry where feasible and satisfy licenses.
- A new class must justify its responsibility; delete dead code and speculative wrappers.
- Prefer smallest focused diff and test; no unrelated reformatting while fixing a hardware bug.