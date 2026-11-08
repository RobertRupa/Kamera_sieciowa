#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

c++ -std=c++17 -Wall -Wextra -Werror -pedantic \
    -I"$ROOT/include" \
    "$ROOT/src/BoardProfile.cpp" \
    "$ROOT/src/CapabilityRegistry.cpp" \
    "$ROOT/tests/capability_registry_host_test.cpp" \
    -o "$TMP/capability_registry_host_test"

"$TMP/capability_registry_host_test"
