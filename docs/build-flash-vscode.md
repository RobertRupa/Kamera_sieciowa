# VS Code, incremental build and fast firmware flashing

## Official toolchain
Use Visual Studio Code with the **Espressif IDF extension** (`espressif.esp-idf-extension`) and official installed ESP-IDF. Prefer maintained stable ESP-IDF 6.1 when target/BSP supports it; select compatible branch explicitly. Configure local Python/toolchain through extension/Espressif Installation Manager, not arbitrary system libraries. Use CMake + Ninja and IDF Component Manager; keep frontend build separate.

## One-time device setup
```bash
# Inside exported ESP-IDF environment
idf.py --version
idf.py set-target esp32s3      # choose EXACT actual MCU, once per target/profile
idf.py build
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