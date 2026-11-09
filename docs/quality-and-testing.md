# Testing, acceptance and diagnostics

## Test pyramid
1. **Host tests** for pure domain/state machine, validation, settings migration, retries/backoff, DTO serialization, error mapping and locale key parity (Catch2/GoogleTest or similar).
2. **ESP-IDF component integration** using Unity/IDF test infrastructure for NVS, networking, queues and driver adapters; scope test NVS writes to an isolated namespace.
3. **Hardware smoke**: boot on specific target; AP and captive portal; Wi-Fi join/wrong password/reconnect after restart; plaintext NVS retained; mDNS; authenticated WebGUI; en/pl/theme; LVGL and touch if fitted.
4. **Fault tests**: credentials failure, low heap, NVS full/read failure, lost Wi-Fi, AP client disconnect mid-write, IPv4/mDNS changes, power interruption, invalid/huge API input, long uptime.
5. **Browser tests**: onboarding at mobile width, forms and operation progress, translations/contrasts/themes, offline/reconnect recovery.

## Required evidence (never fabricate)
| Capability | Evidence |
| --- | --- |
| Build | actual `idf.py build` exit result for declared IDF/target |
| Wi-Fi | first boot, invalid login, join, lost-link fallback, reboot persistence |
| Captive | mobile probe test + direct AP IP fallback |
| mDNS | real host name/service on same broadcast domain |
| Plaintext NVS | partition and driver storage path, credential reboot read; document physical Flash exposure |
| WebGUI | API status codes/auth, locale switch en/pl, theme system/light/dark |
| LVGL | task ownership + touch/render tests only when LCD exists |
| Release | rejection of default admin/admin + debug routes |
| Resource use | free/minimum heap, memory budget and stack/Flash sizes |

If hardware/USB or NVS persistence is not available, report **NOT RUN / BLOCKED**, never PASS.

## Runtime diagnostics
Per-component `ESP_LOGx` tags; report firmware version, target, dev/release profile, reset reason, uptime, heap free/minimum, PSRAM and network status without secrets. Optional metrics: Wi-Fi retry counters, HTTP 4xx/5xx, NVS failures, LVGL FPS, watchdog resets. Log throttling, bounded buffers, no passwords or credential-bearing payloads. Development core dumps only when their data exposure has been reviewed.

## Definition of done
Stable public contract, unit/failure tests, actual build, target-specific smoke where possible, docs/settings/API updated, no test credentials in release, verified plaintext storage path and explicit exposure warning, no new unbounded tasks/queues/allocations. A successful compilation alone is not hardware proof.