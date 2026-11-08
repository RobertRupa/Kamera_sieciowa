# Coding agents and swarm execution protocol

## Canonical sequence
1. **DISCOVER**: read repo instructions, Kconfig/CMake/dependencies/board pinout and current behavior. Output a concise capability matrix: ESP-IDF version, target, Flash/PSRAM, radio, LCD/touch, security policy.
2. **CONTRACT**: coordinator publishes interface signatures, DTOs, REST endpoints, state transitions, event schema, partition plan and owner of each directory. Resolve hard dependencies *before* parallel code.
3. **PLAN**: split into independent slices with explicit owner, deliverable, acceptance criteria and integration point.
4. **IMPLEMENT**: agents commit small changes to agreed paths, never rewrite another agent's files; stay within approved dependency and security policy.
5. **VERIFY**: static checks, host tests, incremental target build, hardware smoke tests (only when hardware available), WebGUI asset build.
6. **DOCUMENT**: update API, hardware assumptions, source-of-truth config, screenshots only if actual outputs available, and limitations.
7. **REVIEW**: integration agent checks conflicts, layering, secrets, memory/Flash, lifecycle, race conditions and test evidence.

## Agent roles (enable only if work size warrants)
| Agent | Owned output | Acceptance evidence |
| --- | --- | --- |
| Architect / coordinator | ADRs, capability matrix, APIs/contracts, task graph | no unresolved cross-module assumptions |
| BSP / drivers | `board_support`, LCD/touch/radio pin map, drivers | pin references verified against schematic/BSP |
| Connectivity | Wi-Fi adapter, AP, mDNS, captive portal | first boot → provision → reconnect → fallback |
| Security/storage | encrypted NVS schema, dev/release gates, auth | encrypted key strategy confirmed per chip |
| Backend/API | application services, controllers, JSON/API | tests against DTO and error contract |
| Frontend | Vue + Vite + PrimeVue WebGUI, locale/theme | en/pl and light/dark/system tested |
| LVGL | GUI views/presenters, touch input, thread ownership | builds for display target; no off-thread LVGL calls |
| QA/integration | host tests, build, basic e2e, docs | evidence report with actual results |
One person/agent may own several roles on a small project; **do not spawn roles to produce empty abstractions**.

## Contract handoff template
```yaml
task_id: NET-001
owner: connectivity-agent
target: esp32s3
idf: "6.1"               # verify local toolchain
paths_owned:
  - components/connectivity/**
inputs:
  - app_contracts/IWifiService.hpp
  - board_support radio capability
outputs:
  - EspWifiService.hpp
  - EspWifiService.cpp
depends_on:
  - encrypted-nvs-contract
constraints:
  - no plaintext password logging
  - event handler must not block
  - no direct LVGL access
acceptance:
  - builds on stated target
  - static validation of empty/oversize SSID
  - reconnect bounded with AP fallback
evidence:
  - commands actually executed
```

## Rules for parallel changes
- Every agent reads `AGENTS.md` and affected `docs/` first; machine-generated TODO lists do not override architecture/security rules.
- Single owner for `CMakeLists.txt`, `sdkconfig.defaults*`, `partitions.csv`, `dependencies.lock`, root docs and public contract headers. Only coordinator merges shared-file modifications.
- Avoid parallel updates to one dependency graph or generated files. Merge contract changes before implementation.
- Declare interfaces and mockable contract tests first. For cross-agent API changes, request coordinator review.
- No autonomous eFuse burns, Flash erases, destructive Git operations, cloud credentials, external publication or release deployment without explicit permission.
- If blocked by unavailable physical hardware, provide compile-level evidence and **say hardware validation was not performed**.

## Required agent final report
```text
Scope and target:
Files changed:
Public contracts/API or schema changed:
Dependencies / resolved versions:
Build / lint / tests executed (commands and results):
Hardware validation (yes/no, device, actual output):
Security state (dev/release; encrypted-NVS strategy):
Known gaps, assumptions and next action:
```

## Effective issue slicing
A task is good if one agent can implement it using already agreed contracts without editing the files of another. If a task needs an unspecified pin, unknown Wi-Fi coprocessor API or missing NVS key scheme, stop that piece and surface the dependency. Keep progress on independent tasks instead of fabricating a solution.