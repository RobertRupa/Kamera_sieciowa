#!/usr/bin/env python3
"""Stdlib-only structural validator. It does NOT certify schematic facts."""
import json
import pathlib
import sys

STATES = {"verified", "unverified", "absent"}
TARGETS = {"esp32", "esp32s2", "esp32s3", "esp32c3", "esp32c5",
           "esp32c6", "esp32h2", "esp32p4"}


def validate(data):
    errors = []
    if data.get("schema_version") != 1:
        errors.append("schema_version must be 1")
    if data.get("kind") != "esp-idf-hardware-manifest":
        errors.append("kind mismatch")
    if not isinstance(data.get("example_only"), bool):
        errors.append("example_only must be boolean")

    board = data.get("board") or {}
    mcu = data.get("primary_mcu") or {}
    wifi = (data.get("radios") or {}).get("wifi") or {}
    if not isinstance(board.get("id"), str) or not board.get("id"):
        errors.append("board.id required")
    if mcu.get("idf_target") not in TARGETS:
        errors.append("invalid primary_mcu.idf_target")
    if not isinstance(mcu.get("has_native_wifi"), bool):
        errors.append("primary_mcu.has_native_wifi must be boolean")
    if mcu.get("idf_target") == "esp32p4" and mcu.get("has_native_wifi"):
        errors.append("ESP32-P4 cannot claim native Wi-Fi")
    if wifi.get("topology") not in {"none", "native", "companion"}:
        errors.append("invalid wifi.topology")
    if wifi.get("verification") not in STATES:
        errors.append("invalid wifi.verification")
    if wifi.get("topology") == "companion" and not wifi.get("companion_soc"):
        errors.append("companion Wi-Fi requires companion_soc")
    if wifi.get("topology") == "native" and mcu.get("has_native_wifi") is False:
        errors.append("native topology conflicts with non-Wi-Fi MCU")

    for field in ("display", "touch"):
        obj = data.get(field) or {}
        if obj.get("verification") not in STATES:
            errors.append(f"{field}.verification must be verified/unverified/absent")
        if obj.get("verification") == "verified" and not obj.get("source"):
            errors.append(f"{field} verified without a source")
        if obj.get("verification") == "absent" and field == "touch":
            continue
    sec = ((data.get("security") or {}).get("nvs_encryption") or {})
    if sec.get("key_protection_verification") not in STATES:
        errors.append("security.nvs_encryption.key_protection_verification invalid")
    if sec.get("key_protection_verification") == "verified" and not sec.get("key_provider"):
        errors.append("key protection marked verified without a key_provider")
    if not isinstance(data.get("pins"), list):
        errors.append("pins must be a list")
    else:
        signals = set()
        for pin in data["pins"]:
            signal = pin.get("signal")
            if not signal or signal in signals:
                errors.append(f"duplicate/missing signal: {signal!r}")
            signals.add(signal)
            gpio = pin.get("gpio")
            if gpio is not None and (type(gpio) is not int or gpio < 0):
                errors.append(f"invalid GPIO number: {signal}")
            if gpio is not None and not pin.get("source"):
                errors.append(f"{signal}: GPIO assigned without schematic/BSP source")

    if data.get("example_only") is False:
        if not board.get("model") or not board.get("pcb_revision") or not board.get("schematic_url"):
            errors.append("non-example needs model, pcb_revision and schematic_url")
        if data.get("needs_verification"):
            errors.append("non-example has unresolved issues")

    return errors


def main():
    if len(sys.argv) != 2:
        print("Usage: validate_manifest.py MANIFEST.json", file=sys.stderr)
        return 2
    path = pathlib.Path(sys.argv[1])
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        print(f"Cannot load manifest: {exc}", file=sys.stderr)
        return 2
    errors = validate(data)
    if errors:
        for error in errors:
            print("ERROR:", error, file=sys.stderr)
        return 1
    print(f"OK: {path} (STRUCTURE ONLY; hardware unverified)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
