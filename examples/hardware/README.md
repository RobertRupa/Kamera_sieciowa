# Example Hardware Manifest

[esp32-p4-c6.reference.json](esp32-p4-c6.reference.json) illustrates an **ESP32-P4 + ESP32-C6** design. It is **not** a verified PCB description. Never copy its unknown GPIOs and peripheral assumptions into firmware as established facts. Only P4 chip-level lack of native Wi-Fi and the documented expansion options have evidence.

## Instructions for agents

1. Copy the JSON as `docs/hardware/board.json` in an application repo.
2. Identify board model/PCB revision; verify schematic, BOM, BSP example, pin mapping, Flash/PSRAM, transport and encryption key scheme. Attach a source to each verified pin.
3. Mark `example_only=false` **only after** resolving required board facts, sources, and remaining issues.
4. Validate using `python3 examples/hardware/validate_manifest.py examples/hardware/esp32-p4-c6.reference.json`.
5. Map *verified static facts* into a compiled `BoardProfile`; do not parse a large JSON file just to start the MCU. The C++ capability registry then combines board profile + feature flags + live runtime status.
6. Update the manifest, compiled profile and host tests together whenever the PCB/feature set changes.

Null means unknown; `verification=unverified` must NOT be treated as a driver or function being ready. Never publish passwords, NVS keys, tokens or other secrets in a manifest.

## Capability statuses

| Status | Meaning | UI behavior |
| --- | --- | --- |
| `unsupported` | Hardware absent | hide action |
| `unverified` | Board wiring/key mechanism not confirmed | show warning; do not activate |
| `disabled` | Hardware verified but feature not compiled | disabled with explanation |
| `not_ready` | Dependency or service not initialized | disabled or progress indicator |
| `ready` | Runtime prerequisites satisfied | action available, subject to auth |

See [Hardware and Capability Registry](../../docs/hardware-and-capabilities.md) and [C++ component example](../../templates/esp-idf/components/board_capabilities/).

## Host checks

```bash
python3 examples/hardware/validate_manifest.py examples/hardware/esp32-p4-c6.reference.json
bash templates/esp-idf/components/board_capabilities/tests/run_host_tests.sh
```

These checks validate structure/logic, **not** actual board GPIOs, encryption, LVGL or Wi-Fi performance.

Official Espressif P4 references:
- https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/get-started/index.html
- https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-guides/wifi-expansion.html
