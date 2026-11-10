# VS Code, incremental build and fast firmware flashing

## Root ESP-IDF project (ESP32-P4)

The only ESP-IDF project root is now **the repository root**,
`Kamera_sieciowa/`. This is where `CMakeLists.txt`,
`CMakePresets.json`, `sdkconfig.defaults`, `dependencies.lock` and
`.vscode/` reside. Source components remain under `firmware/main`
and `firmware/components`, registered using `EXTRA_COMPONENT_DIRS`.
The partition CSV stays at `firmware/partitions.csv`; its SDKCONFIG
path is relative to the new root.

Open `Kamera_sieciowa/` directly in VS Code or open
`Kamera_sieciowa.code-workspace`. Do **not** open `firmware/` as
the ESP-IDF project. The firmware subdirectory no longer contains an
ESP-IDF root CMake project.

ESP-IDF 6.1 and the Espressif extension use `default` automatically
(or select it explicitly under **ESP-IDF: Select Project Configuration**).
The preset already sets `IDF_TARGET=esp32p4`; **do not execute
Set Target repeatedly**, as that invokes a destructive `fullclean` of
the selected build directory before attempting configure.

From the repository root:

```bash
python3 scripts/prepare_web_assets.py
source "$HOME/.espressif/v6.1/esp-idf/export.sh"
idf.py --preset default build
# After selecting the correct programming USB-UART interface:
idf.py --preset default -p /dev/ttyACM0 flash monitor
```

The `production` preset uses `build/production` with a separate
`sdkconfig`; `default` uses `build/default`. Both use
`sdkconfig.defaults` at the repository root. The WebGUI build hook
runs `python scripts/prepare_web_assets.py` with the repository as cwd.
Do not move `firmware/main` or `firmware/components` individually.

### Board profile: ESP32-P4 + ESP32-C6 over SDIO

`default` is the main profile for **Waveshare ESP32-P4-Module-DEV-KIT
(SKU 30560)**, incorporating ESP32-P4 and its onboard ESP32-C6 radio
coprocessor. ESP-IDF compiles **ESP32-P4** firmware (`IDF_TARGET=esp32p4`).
There is no Espressif IDF compilation target called `esp32p4+c6`.
The ESP32-C6 retains its existing ESP-Hosted 3.1.0 firmware and P4-to-C6
four-bit SDIO wiring; these USB flash actions do not flash the C6.

Both board Type-C programming interfaces support an ESP-IDF/esptool serial
flash transport (the corresponding OS port can be `/dev/ttyACM*` or
`/dev/ttyUSB*` based on the interface). **Neither is the HS USB-A UVC
data port.** In VS Code, run one of:

- **ESP32-P4+C6: Flash + Monitor via USB Serial/JTAG**
- **ESP32-P4+C6: Flash + Monitor via UART bridge**

Each task prompts for an actual port so no per-computer serial path is
committed. Identify ports with `ls -l /dev/serial/by-id/` or
`python -m serial.tools.list_ports -v`.

From a terminal in the repository root, choose the port explicitly:

```bash
idf.py --preset default build
idf.py --preset default -p /dev/ttyACM0 flash monitor  # example native USB
# or
idf.py --preset default -p /dev/ttyUSB0 flash monitor  # example bridge UART
```

The P4 ROM may require BOOT+RESET to enter download mode if automatic
reset is not available on the selected programming port. The flash
method is serial in both cases; changing to the HS UVC connector is not
a supported alternative for `idf.py flash`.

### Repair an incomplete root CMake build

If Set Target prints
`Directory '.../Kamera_sieciowa/build/default' doesn't seem to be
a CMake build directory` or
`There is no CMakeCache.txt`, a previous CMake configure failed
and left `build/default` in an incomplete state. From the **root**:

```bash
python3 scripts/repair_incomplete_idf_build.py --preset default
idf.py --preset default build
```

The repair command **renames**, not deletes, an incomplete
`build/default` to `build/default.incomplete-...`, preserving
configuration files and logs. It leaves complete build directories
(with `CMakeCache.txt`) untouched. VS Code also provides the
**ESP32-P4: Repair incomplete CMake build (default)** task.

An old `firmware/build/` from previous versions is no longer used.
It remains ignored by Git and need not be deleted to build from the
root. Do not use `set-target` as a remedy for a failed CMake configure;
fix the first actual configure/compiler error.

### Missing H.264 component

`camera_stream` uses `esp_h264_enc_single_hw.h`. The
`firmware/components/camera_stream/idf_component.yml` declares
`espressif/esp_h264` explicitly; CMake requires the managed component
`espressif__esp_h264`. The Component Manager now resolves dependencies
from the **root** `dependencies.lock` and stores downloads in the
root `managed_components/`; do not copy libraries manually.

## Camera_sieciowa: build and flash from the sidebar

1. In VS Code select **File → Open Workspace from File** and open
   `Kamera_sieciowa.code-workspace` at the repository root.
2. Install the recommended `espressif.esp-idf-extension` and configure
   **ESP-IDF 6.1** (extension Setup / Espressif Installation Manager).
3. In the left Activity Bar select **Espressif / ESP-IDF**. If prompted,
   choose **Kamera_sieciowa (ESP32-P4)** as the project folder.
   Select the **default** preset (target **esp32p4**) and the *programming/debug* serial
   interface (Windows e.g. COM8; Linux e.g. /dev/ttyACM0).
4. Use **Build Your Project**, **Flash Your Project**, **Monitor Device**
   or **Build, Flash and Start a Monitor** in the Espressif sidebar.
   Flash method is configured as **UART**; the USB HS OTG **UVC port**
   is a different peripheral and is not a firmware flashing port.
5. VS Code also exposes named **ESP32-P4:** tasks under
   **Terminal → Run Task** (`.vscode/tasks.json`). They require
   an ESP-IDF-initialized terminal environment. The official Espressif
   sidebar is the preferred path because it configures the toolchain.

The local-only ESP-IDF pre-build hook runs
`python scripts/prepare_web_assets.py` from the repository root:
it checks `web/src`, `web/public` and package metadata, runs `npm ci`
only when dependencies are absent, and performs `npm run build`
only when assets are missing or stale. **Node.js/npm is required**
because `web_api` embeds generated assets. No implicit eFuse write,
backup, erase, or full clean is performed. Project/serial paths are
relative or auto-detected; pick your own serial port on each machine.

### USB role caution

Before connecting the computer to the dedicated USB OTG port, check
the board's physical role selector, schematic and VBUS routing. Use
USB **device** role for UVC. Never connect two USB hosts with a
standard A-to-A cable. The existing USB-UART console/flash port
remains separate.

## One-time device setup
```bash
# From the repository root in an exported ESP-IDF environment
idf.py --version
idf.py --preset default build  # default preset sets IDF_TARGET=esp32p4
```
`set-target` can regenerate project configuration and build state, so **never run it before each flash**. Commit appropriate `sdkconfig.defaults` and project `dependencies.lock`. Do not put personal port paths, Wi-Fi passwords or credentials in Git.

## Fast upload
```bash
# Linux (choose actual port):
idf.py --preset default -p /dev/ttyUSB0 flash
idf.py --preset default -p /dev/ttyUSB0 monitor
# Build-if-needed, flash and monitor:
idf.py --preset default -p /dev/ttyUSB0 flash monitor
# Windows PowerShell in active IDF environment:
idf.py --preset default -p COM5 flash monitor
```
`idf.py flash` invokes required incremental build stages; `idf.py build` is optional as a separate test step. Avoid automatic pre-flash backup, manual SHA/MD5 readback comparison, `fullclean`, partition erase or `erase-flash` as everyday actions. **Do not disable normal ESP-IDF/esptool firmware header/checksum/integrity validation**.

Separate explicit operations: chip erase, new partition schema, NVS reset, key or eFuse provisioning, flash encryption/secure boot and recovery. These can be destructive/irreversible; require explicit authorization. For slow upload diagnose cable, baud, boot-mode GPIO, USB-serial driver, permissions and target before disabling guards.

See [fast-flash script template](../templates/esp-idf/scripts/quick-flash.sh) for other projects. CI should build/verify but never auto-flash unknown USB devices.

## VS Code checklist
- Install Espressif IDF extension and one C++ IntelliSense provider (clangd or MS C/C++) with generated compilation database.
- Select the IDF version, toolchain, target, serial port and menuconfig through extension UI.
- Configure ESP-IDF Build, Flash and Monitor tasks/buttons. Keep `.vscode/extensions.json` in Git; avoid sharing machine-specific absolute paths in `settings.json`.
- Format C/C++ using clang-format, shared `.editorconfig`, optionally clang-tidy. Avoid unrelated bulk refactors.
- Use `idf.py size` and `idf.py size-components` to measure Flash/DRAM impact; debug with JTAG/GDB on equipped boards as needed.
- Keep `build/` and generated frontend files ignored or intentionally packaged, not mixed into sources.

## Build-time optimization
Incremental Ninja builds, per-component CMake dependencies, less heavy transitive headers, reuse of compiled tools, CI cache of downloaded components, fixed SDK. Do not force clean rebuild or refresh dependency graph on every firmware upload. Unit-test domain on host before full on-target build.

## CI verification matrix
For every supported target/profile: CMake configure, target build, memory report; for frontend: `npm ci`, tests, `npm run build`; formatting, locale-key parity, dependency lockfile and documentation checks. Mark physical hardware tests as not run if no device was connected.