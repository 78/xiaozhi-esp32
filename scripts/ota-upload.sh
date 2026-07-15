#!/bin/bash
# Upload built firmware to the OTA server for device updates.
# Usage: ./scripts/ota-upload.sh [firmware.bin] [version]
#
# Defaults:
#   firmware.bin = build/xiaozhi.bin
#   version      = auto-detected from PROJECT_VER in CMakeLists.txt

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
OTA_URL="${OTA_URL:-http://192.168.22.102:18792}"

FIRMWARE="${1:-$PROJECT_DIR/build/xiaozhi.bin}"
VERSION="${2:-$(grep -m1 '^set(PROJECT_VER' "$PROJECT_DIR/CMakeLists.txt" | sed 's/.*"\(.*\)".*/\1/')}"

if [ ! -f "$FIRMWARE" ]; then
    echo "ERROR: Firmware not found: $FIRMWARE"
    echo "Build the project first: idf.py build"
    exit 1
fi

if [ -z "$VERSION" ]; then
    echo "ERROR: Could not detect version. Supply it explicitly."
    exit 1
fi

echo "Uploading firmware v$VERSION from $FIRMWARE to $OTA_URL..."
HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" \
    -X POST "$OTA_URL/api/firmware/upload" \
    -F "file=@$FIRMWARE" \
    -F "version=$VERSION")

if [ "$HTTP_CODE" = "200" ]; then
    echo "OK: Firmware uploaded successfully."
elif [ "$HTTP_CODE" = "500" ]; then
    echo "WARN: Upload returned 500 (may still succeed). Checking..."
    # Verify by fetching firmware info
    STORED=$(curl -s "$OTA_URL/api/firmware")
    STORED_VER=$(echo "$STORED" | python3 -c "import json,sys; print(json.load(sys.stdin).get('version',''))" 2>/dev/null || echo "")
    if [ "$STORED_VER" = "$VERSION" ]; then
        echo "OK: Firmware v$VERSION confirmed on server."
    else
        echo "ERROR: Version mismatch (stored=$STORED_VER, expected=$VERSION)"
        exit 1
    fi
else
    echo "ERROR: Upload failed with HTTP $HTTP_CODE"
    exit 1
fi

echo "OTA URL: $OTA_URL/ota"
echo "Firmware URL: $OTA_URL/firmware/firmware_v${VERSION}.bin"
