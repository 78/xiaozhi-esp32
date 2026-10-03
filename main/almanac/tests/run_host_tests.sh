#!/usr/bin/env bash
set -euo pipefail
DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$DIR/../../.." && pwd)"
CJSON="$ROOT/managed_components/espressif__cjson/cJSON"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
cc -std=c11 -w -c "$CJSON/cJSON.c" -I"$CJSON" -o "$TMP/cJSON.o"
g++ -std=gnu++23 -Wall -Wextra \
    "$DIR/test_almanac_parsers.cc" \
    "$ROOT/main/almanac/almanac_parsers.cc" \
    "$ROOT/main/almanac/solar_terms.cc" \
    "$TMP/cJSON.o" \
    -I"$ROOT/main" -I"$ROOT/main/almanac" -I"$CJSON" \
    -o "$TMP/test"
"$TMP/test"
echo "almanac host tests passed"
