# camera_hub

Current hardware status: native flash and application boot on UART **COM8**
are confirmed. C6 was updated through P4/SDIO OTA to ESP-Hosted **3.1.0**;
runtime now reports matching versions and successful RPC V2 identification.
BLOCK_KEY0/HMAC_UP is now provisioned with
read/write protection after explicit authorization; Secure Boot and Flash Encryption
remain disabled. No network credentials were submitted during OTA. See
[migration notes](migration/README.md). Older COM7/absent-key/mismatch notes
below describe preceding stages, not the current device state.

Latest camera hardware result: the ESP-IDF 6.1 firmware was incrementally
flashed to `/dev/ttyACM0`. The device detected OV5647, received an 800x800 CSI
frame, and served a persistent 800x800 MJPEG preview capped at 20 FPS (earlier tested baseline).
The experimental USB UVC branch now targets the sensor's enumerated FPS instead;
new USB and higher-FPS operation have not yet been tested on the device. The
embedded WebGUI decoded a live frame. `http_mjpeg=ready`; the board-level
`camera` capability remains `unverified` pending PCB revision and sustained
operation checks. See [latest verification](docs/verification.md).

Development firmware for one MIPI-CSI camera on a Waveshare ESP32-P4-Module-DEV-KIT with an ESP32-C6 Wi-Fi/BLE coprocessor connected by the existing SDIO bus.
The current unit has a verified OV5647 preview over HTTP MJPEG and a packaged
device-side WebGUI, driver-derived runtime configuration, and RTP/UDP streams.
USB UVC hardware validation, simultaneous second-camera capture, IMX219 support, production authentication,
long-duration camera endurance and full release onboarding are not implemented
or verified. The board's static camera capability remains `unverified`.
The limited provisioning API and conditional AP/STA/mDNS startup are implemented.
Ordinary NVS stores Wi-Fi credentials without encryption; anyone with a Flash
dump can recover them.
See [implementation status](docs/implementation-status.md).

Wi-Fi update: AP/STA, plain NVS and mDNS adapters compile, as do the three
limited provisioning endpoints. In the local dev profile, the AP starts DHCP at
192.168.4.1 after an ESP-Hosted 3.1.0 C6/RPC V2 handshake. Windows did not detect
the AP SSID; STA did acquire 192.168.69.126. Device status/capability GETs and
the embedded WebGUI's same-origin auto-connect were verified on STA. mDNS
resolution and credential reboot persistence remain untested; no new
credentials were submitted during the camera probe. Credentials remain one
plaintext P4 NVS record. The HMAC_UP eFuse is present but unused by this firmware.

## Implemented

- Native ESP-IDF 6.1/ESP32-P4 project rooted in this repository; component sources remain in [firmware](firmware/).
- Shared immutable JPEG frame leases, bounded external storage and independent
	two-frame queues for USB, HTTP and RTSP.
- Shared-profile validation and rejection of profile changes during active USB.
- Host tests covering ownership, slow clients, pool exhaustion and concurrent readers.
- Persistent OV5647 preview through the bounded FrameBroker and HTTP MJPEG endpoint.
- Driver-enumerated camera settings and per-camera MJPEG/RTSP links in the WebGUI.
- Selectable hardware JPEG or hardware H.264 encoder, with H.264 target bitrate
	and codec-correct RTSP/RTP UDP/SDP. H.264 has no browser MJPEG preview and is
	offered only for compatible native capture dimensions.
- All five upstream OV5647 modes selectable, from 800x640 through 1920x1080,
  including 800x1280 portrait and 1280x960 binning; default remains 800x800.
  Target capture FPS now follows each selected sensor mode (no separate UI FPS
  control); throughput can be lower due to hardware JPEG and USB/HTTP/RTP load.
- RTSP-negotiated RTP/JPEG UDP and direct UDP with an SDP download. See the
	[camera API](docs/camera-api.md) for ports, authentication and resource limits.
- Latest-frame RTP delivery and bounded direct-UDP burst pacing reduce avoidable
	sender backlog. Actual end-to-end delay also depends on Wi-Fi and player caching.
- The board has one CSI camera connector; its other ribbon connector is DSI for
	a display. A second simultaneous camera requires a separately verified input,
	not firmware that treats DSI as CSI.
- Vue/TypeScript/PrimeVue panel with offline/error states, capability diagnostics,
  English/Polish and system/light/dark themes. These preferences are currently
  session-local, not persisted to the device.
- [Hardware manifest](docs/hardware/board.json) with the manufacturer GPIO
	allocation table and model-level peripheral inventory; OV5647 ID and one CSI
	frame are observed, while PCB revision, module revision and sustained camera
	readiness remain explicitly unverified.

The main `default` board profile is **Waveshare ESP32-P4 + ESP32-C6
(SDIO)**. Firmware builds and flashes to ESP32-P4, while the ESP32-C6
remains an onboard ESP-Hosted coprocessor with unchanged SDIO wiring.
VS Code has P4 flash/monitor tasks for native USB Serial/JTAG and USB-UART
bridge; both ask for the connected serial port.

## USB UVC Webcam (experimental branch)

- ESP32-P4 USB 2.0 **High-Speed OTG device** class, using Espressif
  `usb_device_uvc ~1.3.1` with MJPEG/Bulk and existing FrameBroker JPEG frames.
- Four advertised USB profiles: **800×800/50**, **800×640/50**,
  **1280×960/45**, **1920×1080/30** FPS (nominal sensor mode rates).
  The fifth sensor mode, 800×1280/50, remains selectable in WebGUI.
- Windows Camera/OBS and Linux V4L2 can use the native webcam interface without
  virtual camera software. The USB sink does not require Wi-Fi to start.
- **Hardware warning:** Waveshare SKU 30560's two USB-C connectors are
  programming/serial links, not the P4 HS UVC peripheral. The manufacturer
  routes direct HS OTG to USB-A socket **1** (jumper **HOST**); jumper
  **DEVICE** selects the CH334 hub sockets **2–4**. A PC cannot safely be
  connected by an arbitrary male-A/male-A cable; verify the board-side
  VBUS switch and use a proper device-mode adapter/breakout first.
- ESP-IDF 6.1 build, USB 2.0 High-Speed enumeration (`303a:8000`) and
  Linux `uvcvideo` binding are now observed. **Actual UVC FPS remains
  unmeasured**. The second capture-loop FPS gate is removed and the new
  5-second CSI/JPEG/USB feeder counters aid performance diagnosis.
- USB host disconnect now releases the UVC camera reservation and restores
  the previous sensor mode. WebGUI polls live USB ownership, supports
  lab-only camera settings login (admin/admin in the `default` development
  preset), and explicitly labels nominal FPS. While USB UVC is active,
  browser MJPEG is limited to 20 FPS by default to prioritize the USB webcam.
- Development Wi-Fi setup requires the P4/C6 transport-wiring and open-AP
  gates. `sdkconfig.dev.defaults` enables them for the previously tested SDIO
  wiring, but any existing generated `build/default/sdkconfig` may retain
  older disabled values. The setup AP is **open and unauthenticated**; never
  use that development profile on an untrusted network.
  See [USB webcam bring-up](docs/usb-uvc.md) and
  [VS Code build/flash](docs/build-flash-vscode.md).

## Build and Test

From an ESP-IDF 6.1 terminal, prepare the embedded WebGUI once/when changed:

```bash
python scripts/prepare_web_assets.py
```

Open the **repository root** (`Kamera_sieciowa/`) directly in VS Code
or use [`Kamera_sieciowa.code-workspace`](Kamera_sieciowa.code-workspace).
The Espressif sidebar builds the **root** ESP-IDF project; component sources
remain in `firmware/`. The extension pre-build hook prepares WebGUI assets.

Then run from the repository root in an ESP-IDF 6.1 terminal:

```bash
idf.py --preset default build
# Optional: flash through the board's programming USB-UART interface
idf.py --preset default -p /dev/ttyACM0 flash monitor
```

The `default` CMake preset already fixes `IDF_TARGET=esp32p4`. Do not run
`idf.py set-target` on every build. A stale pre-migration
`firmware/build/` is not used. If root `build/default` is incomplete,
run `python3 scripts/repair_incomplete_idf_build.py --preset default`
once before building. See [VS Code instructions](docs/build-flash-vscode.md).

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

The local Vite preview is at `http://localhost:5173/`. The embedded WebGUI on the
device uses its same-origin status API and port-81 MJPEG endpoint; the local Vite
origin is separate and is not the verified live-camera deployment. No video,
credentials or runtime values are fabricated. Camera settings are runtime-only.
The active lab profile explicitly enables camera login with `admin/admin` and
shows an insecure-development warning; release/default configuration does not.
Persistent device settings and release authentication remain unimplemented.

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
Waveshare documents an ESP32-C6/SDIO companion, 16 MiB NOR Flash, 32 MiB PSRAM,
100 Mbps RJ45 Ethernet, TF/SDIO, two-lane MIPI-CSI and MIPI-DSI, USB 2.0 OTG HS,
onboard microphone/audio codec/amplifier, speaker connector, I2C/I3C ports and a
reserved PoE-module interface. The DSI connector does not mean a display is
attached; the PoE interface does not mean a PoE module is installed. These are
model-level documentation facts, not proof of this PCB revision or operational
peripherals. See the [manufacturer-based pin map](docs/hardware/pin-map.md) and
[peripheral integration guide](docs/hardware/peripheral-integration.md) for the
ESP-IDF drivers, existing component versions, bus-sharing constraints and
bring-up requirements.
The manufacturer also lists P4 ISP, H.264/JPEG encode, and PPA/2D-DMA/JPEG-decode
capabilities. These silicon features do not mean the project's camera/display
paths are implemented or that the published maximum frame rates were measured.

The GPIO table assigns the C6 SDIO/EN signals used by the current ESP-Hosted
configuration. GPIO7/8 are listed for DSI/CSI, audio codec and I2C SDA/SCL;
firmware does not configure camera I2C. Other listed audio, Ethernet, USB, UART
and TF signals are not currently brought up as application peripherals. Board
revision correspondence, connector routing, electrical details and runtime
readiness remain unverified. Do not connect two USB hosts or use an ordinary
A-to-A cable; audit device routing, jumpers and VBUS first.

First-version scope: USB MJPEG, HTTP MJPEG, RTSP/RTP JPEG, Ethernet and Wi-Fi.
No WebRTC, HLS, ONVIF, audio, recording or LCD is included. Hardware H.264 is now
available for compatible modes through RTP/RTSP; NVR interoperability and release
security are not certified. No 1080p/30 FPS performance is claimed.

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