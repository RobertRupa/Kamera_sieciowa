# ESP32 My Project Guidelines

**Canonical engineering instructions for ESP-IDF, C/C++, VS Code, coding agents and multi-agent swarms.** These are reusable rules and reference templates; this repository is **not** a board-specific, immediately flashable firmware.

## Quick start
1. Copy or reference [AGENTS.md](AGENTS.md) from the target firmware repository. Also link [.github/copilot-instructions.md](.github/copilot-instructions.md) for VS Code/Copilot.
2. Identify the **exact** MCU, board revision, Flash/PSRAM, display/touch interfaces, connected radio, pinout and ESP-IDF compatibility before choosing dependencies or GPIOs.
3. Select `dev` or `release` profile and resolve the encrypted-NVS provisioning prerequisite; do not claim encryption is enabled without validating target support.
4. Use native ESP-IDF + CMake + IDF Component Manager. Keep application code modular C++ and low-level SDK/ISR callbacks in C-compatible adapters.
5. Implement the mandatory baseline: Wi-Fi provisioning/AP wizard + captive portal (for Wi-Fi capable boards), mDNS, HTTP WebGUI with i18n/themes, encrypted credential storage, and LVGL only when a display exists.
6. Use the acceptance checklist and run a build + relevant hardware smoke tests before declaring the firmware complete.

## The rulebook

| Topic | Reference |
| --- | --- |
| Agent instructions / precedence | [AGENTS.md](AGENTS.md) |
| MVC, modules, dependency direction, project tree | [Architecture](docs/architecture.md) |
| Hardware Manifest + tested Capability Registry reference | [Hardware and capabilities](docs/hardware-and-capabilities.md) |
| SOLID, DRY, KISS, DI, RAII, concurrency | [Engineering principles](docs/engineering-principles.md) |
| Interfaces and HTTP/event contracts | [Service contracts](docs/service-contracts.md) |
| Stack, compatibility and version policy | [Dependencies and targets](docs/dependencies-and-targets.md) |
| Wi-Fi setup, AP wizard, captive portal and mDNS | [Network and provisioning](docs/network-and-provisioning.md) |
| API/WebGUI, themes, localization | [Web UI](docs/web-ui.md) |
| LVGL, display and touch | [LVGL and hardware](docs/lvgl-and-hardware.md) |
| Encrypted NVS, admin/admin development mode | [Security profiles](docs/security-and-profiles.md) |
| Fast builds, flash, VS Code | [Build and flash](docs/build-flash-vscode.md) |
| Swarm roles, task contracts, review | [Agent workflow](docs/agent-swarm.md) |
| Tests and measurable done criteria | [Quality](docs/quality-and-testing.md) |
| Project setup / acceptance | [Checklist](docs/project-checklist.md) |

## Non-negotiable default capabilities

- **Platform**: ESP-IDF 6.1 stable as the starting point for *supported* targets (verified 2026-11-08), or newest compatible maintained stable branch for older SDK/BSP constraints. Do not blindly upgrade managed components.
- **C++**: small cohesive classes, one public class/interface per corresponding pair of files where meaningful, component-local `include/` and `src/`, explicit ownership, C interop boundary.
- **Architecture**: domain + application services + ports/adapters, MVC for presentation (not all business logic in controllers).
- **Networking**: provisioning wizard for Wi-Fi capable devices, AP fallback, captive DNS/OS redirect, STA retries, mDNS only when IP available.
- **WebGUI**: API + packaged modern frontend, English and Polish, light/dark/system themes, responsive accessibility.
- **Display**: LVGL 9 via Espressif port when supported; disable LVGL entirely on headless targets.
- **Secrets**: encrypted NVS, target-specific key management and provisioning. Never use plain NVS while claiming security.
- **Development**: `admin/admin` is allowed **only** in explicitly flagged development firmware, and documented as insecure. Release builds reject it.
- **Flashing**: incremental `idf.py build` and `idf.py flash`; do not add redundant manual checksum checks, backups or full erases to every upload. **Do not remove** intrinsic ESP-IDF image integrity checks.
- **Agents**: never invent GPIO assignments, APIs, dependencies, test results or supported hardware.

## Reference links
- ESP-IDF releases: https://github.com/espressif/esp-idf/releases
- ESP-IDF documentation: https://docs.espressif.com/projects/esp-idf/
- Component Registry: https://components.espressif.com/
- ESP-IDF Component Manager manifests: https://docs.espressif.com/projects/idf-component-manager/en/latest/reference/manifest_file.html

Rules are intentionally adaptable at the edges: hardware capabilities, hard real-time constraints, secure provisioning mechanisms and target memory limits take precedence over a convenient UI framework.