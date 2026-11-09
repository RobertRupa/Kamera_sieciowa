# Minimal ESP-IDF project skeleton (not a finished product)

Copy the contents into a new firmware repository, then implement modules from [Architecture](../../docs/architecture.md) and required features from [Checklist](../../docs/project-checklist.md). This skeleton is **only a native, compilable starting point**: it does not yet include WebGUI, provisioning, LVGL or credential storage. Never claim a new application meets the checklist just because the skeleton builds.

Usage after activating Espressif SDK:
```bash
idf.py set-target esp32s3  # actual MCU, only when selecting/changing target
idf.py -p /dev/ttyUSB0 flash monitor
```

The template includes a [host-testable Capability Registry example](components/board_capabilities/README.md), but does not initialize it in `app_main` until a board-specific verified profile, firmware module selection and real runtime service probes are supplied. Use the [sample Hardware Manifest](../../examples/hardware/esp32-p4-c6.reference.json) as a structure only, not a verified pin map.

The development Kconfig options are for the app to consume in its authentication implementation. This skeleton deliberately does not provision eFuses. Follow the repository's documented plaintext NVS policy and disclose its physical Flash-readout risk before implementing credential persistence.

Copy template `.vscode/extensions.json`, `.editorconfig` and `.clang-format` if using VS Code. Script `scripts/quick-flash.sh` runs a normal incremental IDF flash with no custom backup or checksum.