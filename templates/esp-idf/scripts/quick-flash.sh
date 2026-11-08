#!/usr/bin/env bash
set -eo pipefail

# Use from the copied firmware project root or anywhere within that project.
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

# No backups, full erase or redundant manual checksum/readback checks.
PORT="$1"
if [[ -z "$PORT" ]]; then PORT="$(printenv ESPPORT || true)"; fi
if [[ -z "$PORT" ]]; then PORT="/dev/ttyUSB0"; fi

echo "Flash target on $PORT using standard ESP-IDF integrity safeguards"
idf.py -p "$PORT" flash
