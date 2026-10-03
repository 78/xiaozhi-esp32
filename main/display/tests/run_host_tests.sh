#!/usr/bin/env bash
set -euo pipefail
DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$DIR/../../.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
g++ -std=gnu++23 -Wall -Wextra \
    "$DIR/test_solar_times.cc" \
    "$ROOT/main/display/solar_times.cc" \
    -I"$ROOT/main" -I"$ROOT/main/display" \
    -o "$TMP/test"
"$TMP/test"
echo "display host tests passed"
