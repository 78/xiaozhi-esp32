# OTA API Contract

## 1. Firmware Upload

**`POST /api/firmware/upload`**

Upload a new firmware binary. Accepts `multipart/form-data` with field `file`.

- Returns `200` on success
- Currently returns `500` (file IS saved, but response is wrong)

Fix: Return proper HTTP 200 with JSON body `{"status": "ok", "version": "<version>"}`.

## 2. Version Check

**`GET/POST /ota`**

Device sends `POST` with system info JSON. Server must respond with:

```json
{
  "server_time": {
    "timestamp": 1784193260013,
    "timezone_offset": 180
  },
  "protocol": "websocket",
  "websocket": {
    "url": "ws://192.168.22.102:18792/",
    "access_token": "token"
  },
  "firmware": {
    "has_update": true,
    "version": "2.3.0",
    "url": "http://192.168.22.102:18792/firmware/firmware_v2.3.0.bin"
  }
}
```

**Critical:** `firmware` must be an **object** (not a string). Device parses:
- `firmware.version` — semver string, compared against current version
- `firmware.url` — direct download URL for the firmware binary
- `firmware.has_update` (optional) — if `1` or `true`, forces update even if same version

## 3. Firmware Download

**`GET <firmware.url>`**

Must return raw binary with `Content-Length` header. Device downloads in 4KB chunks and writes to OTA partition.

## 4. Current Setup

| Component | Value |
|-----------|-------|
| OTA URL | `http://192.168.22.102:18792/ota` |
| Firmware URL | `http://192.168.22.102:18792/firmware/firmware_v2.3.0.bin` |
| Upload | `http://192.168.22.102:18792/api/firmware/upload` |
| Device version | `2.3.0` (PROJECT_VER in root CMakeLists.txt) |
| Upload script | `scripts/ota-upload.sh` (uploads `build/xiaozhi.bin`) |

## 5. Test Commands

```bash
# Check current firmware response
curl -s http://192.168.22.102:18792/ota | python3 -m json.tool

# Upload new firmware
curl -X POST http://192.168.22.102:18792/api/firmware/upload \
  -F "file=@build/xiaozhi.bin"

# Direct firmware download
curl -I http://192.168.22.102:18792/firmware/firmware_v2.3.0.bin
```
