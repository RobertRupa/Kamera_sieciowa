# Copyable kickoff prompt for agent/swarm

> Read this repository's AGENTS.md and linked docs first. Inspect the firmware repo, board schematics/pin map, exact target, Flash/PSRAM, Kconfig, CMake and dependency lock. Produce a capability matrix and identify unsupported features. Design typed ports and DTO/API contracts before parallel tasks. Implement ESP-IDF C++ in small cohesive class-per-file components: domain and application services depend only on ports; ESP drivers/NVS/LVGL/HTTP are adapters; MCU-specific `app_main` composes them. Apply SOLID/DRY/KISS/YAGNI and explicit lifetimes.
>
> On Wi-Fi-capable devices include AP wizard/captive portal/STA/retry/mDNS, authenticated WebGUI (Vue+Vite+PrimeVue, en/pl, system/light/dark), and ordinary plaintext NVS credentials with the Flash-readout risk disclosed and driver persistence audited. On LCD boards include LVGL 9 through Espressif port, proper GUI task ownership. Development build may use admin/admin **only** under explicit dev flag; release rejects this. Prefer newest compatible stable ESP-IDF and libraries, locked dependencies. Build and flash incrementally with standard `idf.py`, no extra backups/manual hashes/full erases, and no disabling intrinsic integrity checks.
>
> Assign exclusive file ownership to agents. Never infer unsupported GPIO, security mechanism, framework APIs or test success. Never erase all Flash or burn eFuses without authorization. Produce working vertical slice and tests; report exact changed files, actual commands and results, hardware untested areas and remaining blockers.

## Agent task contract
```yaml
task: "Provisioning baseline"
hardware_target: "esp32s3"    # replace after verification
sdk_version: "6.1"           # validate compatibility
paths_owned: ["components/provisioning/**"]
contracts: ["app_contracts/IProvisioningService.hpp"]
depends_on: ["storage/NVS encryption policy", "connectivity Wi-Fi adapter"]
outputs: ["AP state machine", "wizard API", "unit tests"]
constraints: ["no secrets in logs", "bounded callbacks", "no LVGL outside GUI task"]
acceptance: ["build succeeds", "first-boot flow documented", "wrong password handled"]
permission_for_erase_or_efuse: false
```

## Review prompt
> Review the diff against AGENTS.md and docs/project-checklist.md. Prioritize incorrect hardware/SDK assumptions, layering violations, lifetime/race bugs, heavy callbacks, secrets/plaintext NVS, forbidden release admin/admin, flash/destructive scripts and unverified test claims. Give concrete severity and minimal fixes, no speculative infrastructure.