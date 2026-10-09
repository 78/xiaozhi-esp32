#!/usr/bin/env bash
set -euo pipefail
DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$DIR/../../../.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
g++ -std=gnu++23 -Wall -Wextra \
    "$DIR/test_dashboard_mappings.cc" \
    "$ROOT/main/display/dashboard/dashboard_mappings.cc" \
    -I"$ROOT/main/display/dashboard" \
    -o "$TMP/test"
"$TMP/test"
echo "dashboard mapping host tests passed"
