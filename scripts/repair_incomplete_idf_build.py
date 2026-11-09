#!/usr/bin/env python3
"""Safely quarantine an incomplete ESP-IDF CMake preset build directory.

A failed configure can leave build/default with logs and sdkconfig but without
CMakeCache.txt. 'idf.py set-target' runs fullclean first and refuses to remove
such a directory. This tool moves it aside without deleting files.
"""

import argparse
from datetime import datetime
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
FIRMWARE = ROOT / "firmware"
PRESETS = FIRMWARE / "CMakePresets.json"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preset", default="default", help="CMake configure preset name")
    args = parser.parse_args()

    try:
        presets = json.loads(PRESETS.read_text(encoding="utf-8"))["configurePresets"]
        preset = next(p for p in presets if p["name"] == args.preset)
    except (OSError, KeyError, ValueError, StopIteration) as exc:
        print(f"ERROR: cannot resolve preset {args.preset!r}: {exc}", file=sys.stderr)
        return 2

    raw = preset["binaryDir"]
    if not raw.startswith("${sourceDir}/"):
        print("ERROR: only source-relative build paths are supported", file=sys.stderr)
        return 2
    build_dir = FIRMWARE / raw[len("${sourceDir}/"):]
    root = (FIRMWARE / "build").resolve()
    if build_dir.is_symlink() or not build_dir.resolve().is_relative_to(root):
        print(f"ERROR: unsafe build directory: {build_dir}", file=sys.stderr)
        return 2

    if not build_dir.exists():
        print(f"OK: no incomplete directory: {build_dir}")
        return 0
    if not build_dir.is_dir():
        print(f"ERROR: not a directory: {build_dir}", file=sys.stderr)
        return 2
    if (build_dir / "CMakeCache.txt").is_file():
        print(f"OK: valid CMake cache exists; keeping {build_dir}")
        return 0

    stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    destination = build_dir.with_name(f"{build_dir.name}.incomplete-{stamp}")
    counter = 1
    while destination.exists():
        destination = build_dir.with_name(f"{build_dir.name}.incomplete-{stamp}-{counter}")
        counter += 1
    build_dir.rename(destination)
    print(f"Moved incomplete build (including logs/sdkconfig): {build_dir}")
    print(f"                                        to: {destination}")
    print("Next: run 'idf.py --preset " + args.preset + " build'; do not rerun set-target.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
