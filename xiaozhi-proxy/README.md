# XiaoZhi Transparent Proxy

A transparent proxy between XiaoZhi ESP32 devices and the official server,
enabling push notifications and OpenClaw integration while preserving all
official server functionality.

## Architecture

```
Device ──MQTT──> Mosquitto ──> Proxy Python Service ──WebSocket──> api.tenclass.net
Device ──UDP───> Proxy UDP Server ──> (decrypt) ──WebSocket binary──> api.tenclass.net
```

## Quick Start

### 1. Install Dependencies

```bash
cd xiaozhi-proxy
pip install -r requirements.txt
```

### 2. Install and Configure Mosquitto

```bash
# Ubuntu/Debian
sudo apt install mosquitto

# macOS
brew install mosquitto

# Windows: download from https://mosquitto.org/download/

# Generate password file
bash mosquitto/setup_passwd.sh

# Start Mosquitto
mosquitto -c mosquitto/mosquitto.conf
```

### 3. Configure

Edit `config.yaml`:
- Set `proxy.public_host` to your server's public IP or domain
- Set `openclaw.base_url` and `openclaw.api_key` for OpenClaw integration
- For production: configure TLS in mosquitto.conf and change mqtt.broker_port to 8883

### 4. Run the Proxy

```bash
python -m proxy.main
```

### 5. Point the Device to the Proxy

**Option A (no firmware recompile):** Write the proxy OTA URL to the device's NVS:
- Use ESP-IDF `nvs_set` tool or the device's web config interface
- Key: namespace=`wifi`, key=`ota_url`, value=`http://<proxy_host>:8080/xiaozhi/ota/`

**Option B (firmware recompile):** Change `CONFIG_OTA_URL` in
`xiaozhi-esp32/Kconfig.projbuild` line 5 to your proxy URL, then rebuild.

### 6. Push a Test Reminder

```bash
curl -X POST http://localhost:8081/push/alert \
  -H "Content-Type: application/json" \
  -d '{"status": "提醒", "message": "该开会了！", "emotion": "happy"}'
```

## Components

| Module | Port | Description |
|--------|------|-------------|
| `ota_server` | 8080 | Intercepts OTA, steers device to proxy MQTT |
| Mosquitto | 1883/8883 | MQTT broker for persistent device connection |
| `mqtt_bridge` | - | Routes messages between device and upstream |
| `udp_audio` | 8889 | Handles AES-CTR encrypted Opus audio |
| `ws_upstream` | - | WebSocket client to official server |
| `push_manager` | 8081 | Webhook for reminders and push alerts |
| `openclaw_client` | - | HTTP client for OpenClaw API |

## Push API

### POST /push/alert
Send a text alert to the device (displays on screen + plays notification sound).

```json
{"status": "提醒", "message": "消息内容", "emotion": "happy"}
```

### POST /webhook/reminder
OpenClaw cron webhook callback for timed reminders.

```json
{"message": "提醒内容"}
```
