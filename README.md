# camera_hub

Current hardware status: native flash and application boot on UART **COM8**
are confirmed. C6 was updated through P4/SDIO OTA to ESP-Hosted **3.1.0**;
runtime now reports matching versions and successful RPC V2 identification.
BLOCK_KEY0/HMAC_UP is now provisioned with
read/write protection after explicit authorization; Secure Boot and Flash Encryption
remain disabled. No network credentials were submitted during OTA. See
[migration notes](migration/README.md). Older COM7/absent-key/mismatch notes
below describe preceding stages, not the current device state.

Latest hardware result: the normal P4 host and updated C6 companion report
matching ESP-Hosted 3.1.0, with successful C6 identity over SDIO. BLOCK_KEY0/
HMAC_UP is programmed and protected but is unused by firmware; Secure Boot and
Flash Encryption remain disabled. The latest firmware was flashed on COM8 and
booted. C6 RPC V2 and AP DHCP at 192.168.4.1 were observed; Windows did not see
the AP SSID, and client HTTP, STA and credential reboot persistence remain
untested. See
[latest verification](docs/verification.md).

Development foundation for one MIPI-CSI camera on a Waveshare
ESP32-P4-Module-DEV-KIT, simultaneously exposed to a Windows computer as USB UVC
and to the network as HTTP MJPEG and RTSP/RTP JPEG.

**This is not yet a working camera appliance.** The current firmware only logs
capability diagnostics by default. Camera capture, USB, MJPEG/RTSP, scan/reconnect,
the admin/device-status API and complete device-side WebGUI are not implemented.
The limited provisioning API and conditional AP/STA/mDNS startup are implemented
and compiled. Ordinary NVS initializes on device; the firmware does not encrypt
Wi-Fi credentials, and anyone with a Flash dump can recover them.
See [implementation status](docs/implementation-status.md).

Wi-Fi update: AP/STA, plain NVS and mDNS adapters compile, as do the three
limited provisioning endpoints. In the local dev profile, the AP starts DHCP at
192.168.4.1 after an ESP-Hosted 3.1.0 C6/RPC V2 handshake. Windows did not detect
the AP SSID, so client reachability remains unconfirmed. Credentials are stored
as one plaintext P4 NVS record; no credentials have been submitted, and STA,
HTTP-client, mDNS-resolution and reboot-persistence tests remain unrun. The
HMAC_UP eFuse is present but unused by this firmware.

## Implemented

- Native ESP-IDF 6.1/P4 diagnostic project in [firmware](firmware/).
- Shared immutable JPEG frame leases, bounded external storage and independent
	two-frame queues for USB, HTTP and RTSP.
- Shared-profile validation and rejection of profile changes during active USB.
- Host tests covering ownership, slow clients, pool exhaustion and concurrent readers.
- Vue/TypeScript/PrimeVue panel with offline/error states, capability diagnostics,
  English/Polish and system/light/dark themes. These preferences are currently
  session-local, not persisted to the device.
- [Hardware manifest](docs/hardware/board.json) with explicit unknown pinout,
	sensor, PCB revision and key protection. No default account or credential storage.

## Build and Test

From an ESP-IDF 6.1 terminal:

```powershell
Set-Location firmware
idf.py set-target esp32p4
idf.py build
```

The unchanged partition table contains an ordinary `app_config` NVS partition at
0xE10000/24 KiB. It is not encrypted; Flash readers can recover saved Wi-Fi
credentials. Initialization errors never trigger automatic erase. No key
generation or eFuse programming is part of the build or flash workflow. Do not
deploy this as a complete camera appliance.

Host tests with CMake and a C++17 compiler:

```powershell
cmake -S tests/host -B build/host
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

With Docker on Windows, [scripts/test-host.ps1](scripts/test-host.ps1) runs the
same tests in a Linux container. [Verification](docs/verification.md) records
only commands actually observed.

For the browser panel (Node.js 22):

```powershell
Set-Location web
npm ci
npm run typecheck
npm test
npm run build
npm run dev -- --host 127.0.0.1
```

The current local preview is at `http://localhost:5173/`. It shows No signal until
a real device implements the [planned API](docs/camera-api.md) and confirms stream
readiness. No video, credentials or runtime values are fabricated. Camera/network
configuration, login and device persistence are not implemented in this panel yet.

The setup view at `http://localhost:5173/#/wifi` shows prerequisite status. Its
SSID/password form is hidden unless the device reports Wi-Fi, wizard and
credential storage Ready. The frontend provisioning client is implemented; the corresponding
on-device session/credential/operation endpoints started in the local dev profile,
but no live HTTP client request has been tested. Serving the frontend and the
device capability API is still pending. Browser tests use controlled fixtures.

Browser tests: `npx playwright install --with-deps chromium`, then
`npm run test:browser`. CI is configured for host tests, frontend tests and the
diagnostic firmware build; it does not certify hardware or a production release.

## Hardware and Scope

The candidate sensor is OV5647; compatibility of the actual Raspberry Pi Camera
or Pi NoIR module and ribbon cable is unverified. The P4 has no native Wi-Fi;
this board's vendor documents an ESP32-C6/SDIO companion and IP101GRI Ethernet.
Camera, USB and network readiness are not inferred from those model-level facts.
The reported C6 SDIO/EN and camera I2C GPIOs are recorded in the
[supplied pin map](docs/hardware/pin-map.md) and manifest. They match the current
ESP-Hosted configuration, but do not establish runtime radio/security readiness.
Do not connect two USB hosts or use an ordinary
A-to-A cable; audit device routing, jumpers and VBUS first.

First-version scope: USB MJPEG, HTTP MJPEG, RTSP/RTP JPEG, Ethernet and Wi-Fi.
No WebRTC, HLS, ONVIF, audio, recording, LCD or H.264 is included. NVRs requiring
H.264 are outside this first-version scope. No 1080p/30 FPS performance is claimed.

## Repository Origin

The upstream history and engineering guidelines are preserved below. This local
checkout comes from RobertRupa/esp32-my-project-guidelines; a GitHub fork named
camera_hub has **not** been created or published. No license is inferred for the
upstream repository in the absence of an explicit license grant.

---

# ESP32 My Project Guidelines

**Canonical engineering instructions for ESP-IDF, C/C++, VS Code, coding agents and multi-agent swarms.** These are reusable rules and reference templates; this repository is **not** a board-specific, immediately flashable firmware.

## Quick start
1. Copy or reference [AGENTS.md](AGENTS.md) from the target firmware repository. Also link [.github/copilot-instructions.md](.github/copilot-instructions.md) for VS Code/Copilot.
2. Identify the **exact** MCU, board revision, Flash/PSRAM, display/touch interfaces, connected radio, pinout and ESP-IDF compatibility before choosing dependencies or GPIOs.
3. Select `dev` or `release` profile and disclose that Wi-Fi credentials are stored in plaintext NVS and recoverable from a Flash dump.
4. Use native ESP-IDF + CMake + IDF Component Manager. Keep application code modular C++ and low-level SDK/ISR callbacks in C-compatible adapters.
5. Implement the mandatory baseline: Wi-Fi provisioning/AP wizard + captive portal (for Wi-Fi capable boards), mDNS, HTTP WebGUI with i18n/themes, plaintext credential storage with an audited driver path, and LVGL only when a display exists.
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
| Plaintext NVS, admin/admin development mode | [Security profiles](docs/security-and-profiles.md) |
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
- **Wi-Fi credentials**: intentionally stored in plaintext NVS; explicitly document physical Flash-readout exposure and verify the driver persistence path.
- **Development**: `admin/admin` is allowed **only** in explicitly flagged development firmware, and documented as insecure. Release builds reject it.
- **Flashing**: incremental `idf.py build` and `idf.py flash`; do not add redundant manual checksum checks, backups or full erases to every upload. **Do not remove** intrinsic ESP-IDF image integrity checks.
- **Agents**: never invent GPIO assignments, APIs, dependencies, test results or supported hardware.

## Reference links
- ESP-IDF releases: https://github.com/espressif/esp-idf/releases
- ESP-IDF documentation: https://docs.espressif.com/projects/esp-idf/
- Component Registry: https://components.espressif.com/
- ESP-IDF Component Manager manifests: https://docs.espressif.com/projects/idf-component-manager/en/latest/reference/manifest_file.html

Rules are intentionally adaptable at the edges: hardware capabilities, hard real-time constraints, secure provisioning mechanisms and target memory limits take precedence over a convenient UI framework.