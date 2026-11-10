#!/usr/bin/env python3
"""Incremental WebGUI asset preparation for ESP-IDF's embedded web_api component.

Runs only during a developer-initiated build; does not erase, flash or touch NVS.
"""
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
WEB = ROOT / "web"
DIST = WEB / "dist" / "index.html"
NODE_MODULES = WEB / "node_modules"
INPUTS = ("src", "public", "index.html", "package.json", "package-lock.json",
          "vite.config.ts", "tsconfig.json", "tsconfig.app.json", "tsconfig.node.json")


def newest_input_mtime() -> float:
    mtimes = []
    for item in INPUTS:
        path = WEB / item
        if path.is_file():
            mtimes.append(path.stat().st_mtime)
        elif path.is_dir():
            mtimes.extend(p.stat().st_mtime for p in path.rglob("*") if p.is_file())
    return max(mtimes, default=0.0)


def main() -> int:
    npm = shutil.which("npm")
    if not npm:
        print("ERROR: Node.js/npm is required to generate the embedded WebGUI", file=sys.stderr)
        return 1
    if not NODE_MODULES.is_dir():
        print("Installing WebGUI dependencies (npm ci)")
        subprocess.run([npm, "ci"], cwd=WEB, check=True)
    if not DIST.is_file() or newest_input_mtime() > DIST.stat().st_mtime:
        print("Building embedded WebGUI (npm run build)")
        subprocess.run([npm, "run", "build"], cwd=WEB, check=True)
    else:
        print("WebGUI assets are up to date")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except subprocess.CalledProcessError as exc:
        print(f"ERROR: WebGUI build failed with status {exc.returncode}", file=sys.stderr)
        sys.exit(exc.returncode)
