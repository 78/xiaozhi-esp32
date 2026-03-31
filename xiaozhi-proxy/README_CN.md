# 小智透明代理

在小智 ESP32 设备和官方服务器之间的透明代理，
在保留所有官方服务器功能的同时，支持推送通知和 OpenClaw 集成。

## 架构

```
设备 ──MQTT──> Mosquitto ──> 代理 Python 服务 ──WebSocket──> api.tenclass.net
设备 ──UDP───> 代理 UDP 服务器 ──> (解密) ──WebSocket 二进制流──> api.tenclass.net
```

## 快速开始

### 1. 安装依赖

```bash
cd xiaozhi-proxy
pip install -r requirements.txt
```

### 2. 安装和配置 Mosquitto

```bash
# Ubuntu/Debian
sudo apt install mosquitto

# macOS
brew install mosquitto

# Windows：从 https://mosquitto.org/download/ 下载

# 生成密码文件
bash mosquitto/setup_passwd.sh

# 启动 Mosquitto
mosquitto -c mosquitto/mosquitto.conf
```

### 3. 配置

编辑 `config.yaml`：
- 将 `proxy.public_host` 设置为你服务器的公网 IP 或域名
- 将 `openclaw.base_url` 和 `openclaw.api_key` 设置为 OpenClaw 集成所需值
- 生产环境：在 mosquitto.conf 中配置 TLS，并将 mqtt.broker_port 改为 8883

### 4. 运行代理服务

```bash
python -m proxy.main
```

### 5. 将设备指向代理

**选项 A（无需重新编译固件）：** 将代理 OTA URL 写入设备的 NVS：
- 使用 ESP-IDF 的 `nvs_set` 工具或设备的 Web 配置界面
- 键：namespace=`wifi`, key=`ota_url`, value=`http://<proxy_host>:8080/xiaozhi/ota/`

**选项 B（重新编译固件）：** 修改 `xiaozhi-esp32/Kconfig.projbuild` 第 5 行的 `CONFIG_OTA_URL` 为你的代理 URL，然后重新编译。

### 6. 推送测试提醒

```bash
curl -X POST http://localhost:8081/push/alert \
  -H "Content-Type: application/json" \
  -d '{"status": "提醒", "message": "该开会了！", "emotion": "happy"}'
```

## 组件

| 模块 | 端口 | 描述 |
|------|------|------|
| `ota_server` | 8080 | 拦截 OTA，引导设备连接到代理 MQTT |
| Mosquitto | 1883/8883 | 用于设备持久连接的 MQTT 代理 |
| `mqtt_bridge` | - | 在设备和上游之间路由消息 |
| `udp_audio` | 8889 | 处理 AES-CTR 加密的 Opus 音频 |
| `ws_upstream` | - | 到官方服务器的 WebSocket 客户端 |
| `push_manager` | 8081 | 用于提醒和推送警报的 Webhook |
| `openclaw_client` | - | 用于 OpenClaw API 的 HTTP 客户端 |

## 推送 API

### POST /push/alert
向设备发送文本提醒（在屏幕上显示 + 播放提示音）。

```json
{"status": "提醒", "message": "消息内容", "emotion": "happy"}
```

### POST /webhook/reminder
OpenClaw 定时提醒的 cron webhook 回调。

```json
{"message": "提醒内容"}
```
