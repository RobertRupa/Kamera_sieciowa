# Copilot / VS Code repository instructions

Read [AGENTS.md](../AGENTS.md) first. Its rules override snippets.

- ESP-IDF stable compatible version, native C/C++, CMake, IDF Component Manager. Never invent GPIO, board peripherals or successful test results.
- Modular per-component domain/application/ports/adapters with MVC view edge. SOLID/DRY/KISS/YAGNI, C++ classes in matching include/src files, constructor DI and typed service contracts.
- Wi-Fi-capable builds: provisioning AP wizard, captive portal, reconnect, mDNS, responsive WebGUI. WebGUI: Vue/Vite/PrimeVue, `en` default and `pl`, system/light/dark themes.
- LCD only: LVGL v9 through Espressif `esp_lvgl_port`; GUI thread ownership mandatory.
- Wi-Fi credentials must be truly encrypted in NVS, including driver persistence path; document target-specific encryption and key scheme.
- `admin/admin` allowed exclusively when explicit dev mode; forbidden in release. No actual passwords or keys in logs or Git.
- `idf.py -p PORT flash` incremental without backup, routine erase or extra manual checksum/readback. Keep normal ESP-IDF image integrity.
- One owner per file in swarm. Define interfaces, task/event ownership and hardware facts before parallel implementation. Report build/tests actually executed and limitations.
- Required references: [Architecture](../docs/architecture.md), [Contracts](../docs/service-contracts.md), [Security](../docs/security-and-profiles.md), [Checklist](../docs/project-checklist.md).
`