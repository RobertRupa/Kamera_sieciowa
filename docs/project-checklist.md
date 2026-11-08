# Project checklist — copy into each firmware PR

Check items with evidence; note N/A explicitly for unsupported hardware.

## Hardware / architecture
- [ ] Exact board revision, MCU, Flash/PSRAM, schematics and GPIO/bus pin matrix documented.
- [ ] Machine-readable Hardware Manifest verified, including explicit unknowns and evidence for assigned GPIOs.
- [ ] Capability Registry evaluates verified hardware, compile-time modules and runtime readiness; WebGUI/LVGL use the same snapshot.
- [ ] Native ESP-IDF + CMake, official components where possible, `idf_component.yml` + resolved `dependencies.lock`.
- [ ] Domain/Application/Ports/Adapters + UI MVC split; small classes in matching .hpp/.cpp under appropriate components.
- [ ] Constructor DI, typed result/errors, state machine, event ownership and concurrency contract.
- [ ] C ABI wrappers at SDK/ISR boundary; no large business logic in `app_main` or HTTP callbacks.
- [ ] Versioned runtime settings and storage migration strategy.

## Baseline features
- [ ] Wi-Fi-capable target: STA + scan + reconnect, AP wizard, captive portal and direct AP IP fallback.
- [ ] mDNS hostname and HTTP service advertised after IP readiness.
- [ ] Credentials encrypted in NVS, **including ESP Wi-Fi driver's storage path**; key scheme supported by target.
- [ ] WebGUI dashboard/settings/Wi-Fi/security, authenticated API and responsive mobile UI.
- [ ] Language selector: en default and pl shipped; locale key parity.
- [ ] Theme: system/light/dark, saved in settings.
- [ ] Display target: LVGL v9 + `esp_lvgl_port`, proper touch, task-ownership; absent when no display.
- [ ] ESP32-P4 external network companion or ESP32-H2 no-native-Wi-Fi limitations accurately handled.

## Security and development
- [ ] Dev vs release flags explicit; default `admin/admin` allowed **dev only**.
- [ ] Dev warning shown; release build blocks defaults/debug routes.
- [ ] Real encrypted NVS mechanism documented; no automatic irreversible eFuse burn.
- [ ] No private passwords/tokens/NVS keys in repo, logs, WebGUI or diagnostics.
- [ ] Normal upload has no backup/full erase/redundant hash comparison; built-in image integrity preserved.
- [ ] Destructive Flash/partition/eFuse operations require explicit human approval.

## Quality
- [ ] Incremental build for actual target/profile executed, result recorded.
- [ ] Host/domain tests and frontend tests executed; hardware smoke performed or marked NOT RUN.
- [ ] Wi-Fi onboarding/rollback/fallback and reboot persistence scenarios covered.
- [ ] Heap/stack/PSRAM/Flash and LVGL throughput checked where relevant.
- [ ] Error codes, API contract, i18n, build docs and hardware manifest updated.
- [ ] Agent completion report lists modified paths, test evidence, assumptions and limits.

## Further improvements
- [ ] Capability registry drives which API/UI functions the device exposes.
- [ ] Observability for reset reason, boot cause, free/min heap and error counters.
- [ ] Architecture Decision Records for chip-specific compromises and encryption/eFuses.
- [ ] Optional OTA with safe partition/recovery plan **for deployed devices** (not a requirement for lab fast flash).
- [ ] Scheduled stable dependency reviews/security fixes; compile matrix for supported targets.