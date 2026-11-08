# Stack, compatibility and version selection

**Verified baseline as of 2026-11-08**: ESP-IDF **v6.1** is the latest listed feature release, and **v5.5.6** is an active v5.5 bugfix release. Prefer v6.1 for newly created **supported** targets; select a supported maintained stable branch for BSP/radio/vendor restrictions. Confirm current official releases at creation time: https://github.com/espressif/esp-idf/releases

## Dependency policy
1. Prefer official ESP-IDF APIs, Espressif BSP and ESP Component Registry packages; external vetted MIT/Apache components as needed.
2. Before selecting “newest”, check release status, target compatibility, required SDK version, dependencies, advisories, license, memory impact and migration guide.
3. Set compatible semver constraints in `idf_component.yml`; **commit resolved `dependencies.lock`** in actual firmware repository. Normal builds must remain reproducible; refresh lockfile only as an intentional dependency upgrade.
4. No unqualified `master`, `main`, `latest` or mutable release URL for production dependency builds. No copying a whole library into `components` without license and reason.
5. Evaluate Flash/heap/IRAM growth in PRs. Prefer upstream bugfix rather than local forks.
6. Keep framework versions as project input, not forever-hardcoded global policies. Re-validate compatibility when selecting a new target.

## Recommended components/tools
| Capability | Recommended baseline | Notes |
| --- | --- | --- |
| Firmware | ESP-IDF stable + CMake + IDF Component Manager | native `idf.py`, no Arduino dependency by default |
| Editor | VS Code + Espressif IDF extension + clangd/CMake awareness | configured per project |
| Core event/network | `esp_event`, `esp_netif`, `esp_wifi` | Wi-Fi only if hardware/transport supports it |
| Provisioning | **`espressif/network_provisioning`** | replaces built-in `wifi_provisioning` in IDF 6.x |
| HTTP/API | `esp_http_server`; `esp_https_server` when TLS required | use maintained SDK implementations |
| mDNS | `espressif/mdns` | publish service when IP available |
| GUI | `lvgl/lvgl` **v9** + `espressif/esp_lvgl_port` | Espressif `esp_lcd` + target BSP/touch |
| Captive portal | reviewed `nordesems/esp-captive-portal` or maintained equivalent | verify target/version; gives DNS/OS probe handling, not full Wi-Fi wizard |
| Storage | ESP-IDF `nvs_flash` + `nvs_sec_provider` strategy | verify supported NVS encryption key protection |
| Frontend | Vue 3 + Vite + PrimeVue + Pinia + Vue I18n | built at development time into static assets |
| Testing | Unity/ESP-IDF tests + host Catch2 or GoogleTest + pytest-embedded | choose based on coverage and build footprint |
| Formatting | clang-format, clang-tidy (where practical), editorconfig | avoid unrelated global reformat |

References:
- https://docs.espressif.com/projects/esp-idf/en/release-v6.1/esp32/migration-guides/release-6.x/6.0/provisioning.html
- https://components.espressif.com/components/espressif/esp_lvgl_port
- https://components.espressif.com/components/nordesems/esp-captive-portal
- https://docs.espressif.com/projects/idf-component-manager/en/latest/reference/manifest_file.html

## ESP-IDF 6.x migration warnings
- Legacy `wifi_provisioning` **moved** and renamed `network_provisioning`; old APIs are not drop-in compatible.
- Legacy peripheral drivers removed; use new ADC/DAC/I2S/PCNT/MCPWM/RMT etc APIs.
- Some extra components (including MQTT/cJSON) now come from registry.
- C/C++ language defaults and warnings differ from IDF 5.x; set and validate required language standard for each project instead of assuming old toolchain behavior.
- Verify changes from `6.0` to `6.1` and any target-specific errata for the exact chip.

## Hardware capability matrix (examples, not board claims)
| Family | Wi-Fi | LVGL | Notes |
| --- | --- | --- | --- |
| ESP32, S2, S3, C3, C5, C6 | when onboard radio enabled | only if display connected and memory permits | PSRAM availability varies by chip/module |
| ESP32-P4 | **external Wi-Fi companion required** | strong fit for large GUI with suitable BSP/PSRAM | Wi-Fi gateway must have explicit integration |
| ESP32-H2 | **no native Wi-Fi** | only if feasible | 802.15.4/BLE roles; requires external IP/Wi-Fi bridge to offer WebGUI |
| Other targets | discover per official target page | optional | never assume flash size or pinout |

Hardware manifest in each real project should document exact board revision, schematic URL/commit, GPIO allocation table, power rails, SPI/I2C/UART buses, DMA/PSRAM constraints, Flash partitions and supported feature matrix. Keep unsupported features unavailable in Kconfig/UI rather than stubbing fake implementations.

## Example dependency manifest (illustrative, validate compatibility)
```yaml
# main/idf_component.yml — resolve current compatible releases
dependencies:
  idf: ">=6.1,<7"
  espressif/network_provisioning: "^1.1.0"
  espressif/mdns: "*"
  # Only enable the following with a supported display/BSP:
  # lvgl/lvgl: "^9"
  # espressif/esp_lvgl_port: "*"
  # Review external captive portal component license/target support:
  # nordesems/esp-captive-portal: "*"
```
`*` is **discovery placeholder only**, not an endorsed application lock policy; replace it with a reviewed compatible range and commit `dependencies.lock`. A headless/ESP32-H2 project must omit unsupported features. Use target-appropriate examples and never blindly paste all dependencies.