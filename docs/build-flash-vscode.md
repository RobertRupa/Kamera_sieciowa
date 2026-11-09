# VS Code, incremental build and fast firmware flashing

## Official toolchain
Use Visual Studio Code with the **Espressif IDF extension** (`espressif.esp-idf-extension`) and official installed ESP-IDF. Prefer maintained stable ESP-IDF 6.1 when target/BSP supports it; select compatible branch explicitly. Configure local Python/toolchain through extension/Espressif Installation Manager, not arbitrary system libraries. Use CMake + Ninja and IDF Component Manager; keep frontend build separate.

### ESP-IDF "Set target failed" in the repository root

If the ESP-IDF output shows `idf.py -B .../Kamera_sieciowa/build set-target esp32p4`
followed by `Directory .../build doesn't seem to be a CMake build directory`,
then **VS Code selected the repository root**, not the ESP-IDF project in
`firmware/`. This is a project-selection error, not a sensor, flash or CPU
detection problem.

1. Open **File → Open Workspace from File** →
   `Kamera_sieciowa.code-workspace`. This workspace now contains **only**
   the `firmware/` project to make accidental root selection impossible.
2. Or open the folder `firmware/` directly in a new VS Code window.
   If using a custom multi-root workspace, choose **ESP-IDF: Pick a Workspace
   Folder** → **ESP32-P4 Firmware (ESP-IDF)**.
3. Select **ESP-IDF: Select Project Configuration** → `default`.
   This preset fixes `IDF_TARGET=esp32p4`, using `firmware/build/default`.
4. Build/flash from the official ESP-IDF Activity Bar. Do not repeat `set-target` on this single-target project.

**Do not delete `.../Kamera_sieciowa/build`** to work around this error:
that directory may contain independent host tests. The root folder's own
`.vscode/settings.json` disables ESP-IDF activation by design; `firmware/`
continues to enable it and uses `firmware/build`. ESP-IDF `set-target`
normally performs a full clean of the selected firmware build directory, so
avoid re-running it on every build.

### Recovery: `build/default` exists without `CMakeCache.txt`

This can happen after an earlier CMake configure failure. A second
**Set Espressif Device Target** action invokes `idf.py set-target`, which
always runs `fullclean` first. ESP-IDF refuses to delete an incomplete
directory lacking `CMakeCache.txt`. It is not an ESP32-P4 detection error.

The repository now pins `IDF_TARGET=esp32p4` and
`SDKCONFIG_DEFAULTS=sdkconfig.defaults` in **both** the `default` and
`production` CMake presets. It also aligns the VS Code `idf.buildPath`
with `firmware/build/default`. Do **not** run **Set Target** again to
initialise this ESP32-P4-only project: open **ESP-IDF: Select Project
Configuration → default**, then use **Build your Project**.

**One-time recovery of the old incomplete directory** (run from the
repository root; no data is deleted):

```bash
python3 scripts/repair_incomplete_idf_build.py --preset default
source /home/konatsu/.espressif/v6.1/esp-idf/export.sh
cd firmware
idf.py --preset default build
```

You can instead run **Terminal → Run Task →
ESP32-P4: Repair incomplete CMake build (default)** in VS Code, and then
select **Build your Project** in the ESP-IDF sidebar.

The repair script checks the selected preset and moves
`firmware/build/default` to a unique
`firmware/build/default.incomplete-YYYYMMDD-HHMMSS` name **only when
the directory exists without `CMakeCache.txt`**. It preserves its
logs, configuration and other files; a directory with a CMake cache is
never touched. After a subsequent configure/build error, fix the **first**
CMake/compiler error rather than repeatedly selecting Set Target.

Machine-specific paths such as `idf.currentSetup`, `idf.port` and
`clangd.path` belong in VS Code user settings or a local
`CMakeUserPresets.json` (ignored by Git), not in tracked workspace
settings. Existing local uncommitted settings should be saved
separately before updating tracked files if needed.

### ESP-IDF 6.1: missing `esp_h264` during `set-target`

If CMake reports `Failed to resolve component 'esp_h264' required by component
'camera_stream'` and the component manager prints `Deleting 1 unused
components: espressif__esp_h264`, update this branch first. The
`camera_stream/idf_component.yml` manifest now declares
`espressif/esp_h264: 1.4.*` explicitly, and CMake depends on the
registry component name `espressif__esp_h264`. The dependency is required
because `CameraHardwareProbe.cpp` directly includes
`esp_h264_enc_single_hw.h`. This prevents the component manager from
pruning the library when `esp_video` treats H.264 as a conditional
dependency.

From an ESP-IDF 6.1 terminal in the project directory, run:

```bash
idf.py reconfigure
idf.py build
```

If the current `set-target` action previously failed, repair the incomplete
build directory using the script above, then run Build (without Set Target).
The component manager re-resolves `firmware/dependencies.lock` and
populates `firmware/managed_components/` automatically; do not manually
copy files into that directory or remove unrelated build folders.
If a CMake preset is active, the extension may use
`firmware/build/default` and a preset-specific `sdkconfig` instead of
`firmware/build` directly.

## Camera_sieciowa: build and flash from the sidebar

1. In VS Code select **File → Open Workspace from File** and open
   `Kamera_sieciowa.code-workspace` at the repository root.
2. Install the recommended `espressif.esp-idf-extension` and configure
   **ESP-IDF 6.1** (extension Setup / Espressif Installation Manager).
3. In the left Activity Bar select **Espressif / ESP-IDF**. If prompted,
   choose **ESP32-P4 Firmware (ESP-IDF)** as the current project folder.
   Select the **default** preset (target is **esp32p4**) and the *programming/debug* serial
   interface (Windows e.g. COM8; Linux e.g. /dev/ttyACM0).
4. Use **Build Your Project**, **Flash Your Project**, **Monitor Device**
   or **Build, Flash and Start a Monitor** in the Espressif sidebar.
   Flash method is configured as **UART**; the USB HS OTG **UVC port**
   is a different peripheral and is not a firmware flashing port.
5. VS Code also exposes named **ESP32-P4:** tasks under
   **Terminal → Run Task** (`firmware/.vscode/tasks.json`). They require
   an ESP-IDF-initialized terminal environment. The official Espressif
   sidebar is the preferred path because it configures the toolchain.

The local-only ESP-IDF pre-build hook runs
`python ../scripts/prepare_web_assets.py` from `firmware/`:
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
# Inside exported ESP-IDF environment
idf.py --version
idf.py --preset default build  # default preset sets IDF_TARGET=esp32p4
```
`set-target` can regenerate project configuration and build state, so **never run it before each flash**. Commit appropriate `sdkconfig.defaults` and project `dependencies.lock`. Do not put personal port paths, Wi-Fi passwords or credentials in Git.

## Fast upload
```bash
# Linux (choose actual port):
idf.py -p /dev/ttyUSB0 flash
idf.py -p /dev/ttyUSB0 monitor
# Build-if-needed, flash and monitor:
idf.py -p /dev/ttyUSB0 flash monitor
# Windows PowerShell in active IDF environment:
idf.py -p COM5 flash monitor
```
`idf.py flash` invokes required incremental build stages; `idf.py build` is optional as a separate test step. Avoid automatic pre-flash backup, manual SHA/MD5 readback comparison, `fullclean`, partition erase or `erase-flash` as everyday actions. **Do not disable normal ESP-IDF/esptool firmware header/checksum/integrity validation**.

Separate explicit operations: chip erase, new partition schema, NVS reset, key or eFuse provisioning, flash encryption/secure boot and recovery. These can be destructive/irreversible; require explicit authorization. For slow upload diagnose cable, baud, boot-mode GPIO, USB-serial driver, permissions and target before disabling guards.

Copy [fast-flash script](../templates/esp-idf/scripts/quick-flash.sh) to firmware's `scripts/`. CI should build/verify but never auto-flash unknown USB devices.

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