# 小智 ESP32 二次开发 · LCDWiki ES3C28P 待机仪表盘

本项目基于开源项目 [Xiaozhi（小智 AI 聊天机器人）](https://github.com/78/xiaozhi-esp32) 二次开发，
在保留小智完整语音对话能力的基础上，为 **LCDWiki ES3C28P 2.8 寸 ESP32-S3 智能显示模块**
新增了信息丰富的**待机仪表盘**（时钟、天气、空气、农历黄历等）。

本仓库为二次开发后的独立维护版本，仅推送到自有仓库：
<https://github.com/drb789/xiaozhi-esp32>

## 功能特性

### 待机仪表盘

设备空闲时自动显示 240×320 仪表盘，从上到下依次为：

- **顶部状态栏**：WiFi 图标、节假日黄色徽章 + 放假安排通知（文字过长时跑马灯滚动）
- **天气区**：城市名、空气质量等级徽章（优/良/轻度污染等，按等级变色）、空气指数、
  和风天气彩色天气图标和天气状态徽章（如“雾”）
- **大时钟**：Impact 字体 HH:MM（分钟橙色），右侧红色秒数
- **日期区**：公历日期、星期；黑色农历徽章（如“农历八月十五日”），
  右侧徽章显示**节气**（秋分等，琥珀金）或**传统节日**（中秋节等，紫色）
- **每日宜忌**：绿色“宜”/红色“忌”圆形徽章 + 内容；内容不超出时静态显示，
  超出时自动切换为水平跑马灯
- **底部环境区**：温度、湿度图标及数值

### 继承的小智能力

- 离线语音唤醒（ESP-SR，可自定义唤醒词）
- WebSocket、MQTT + UDP 两种通信协议；Opus 音频流
- 接入大模型（通义千问 / DeepSeek 等），支持设备端 MCP 工具调用
- 摄像头、表情、电池管理等小智原有全部能力

## 硬件规格

目标硬件：LCDWiki ES3C28P（带触摸屏）/ ES3N28P（无触摸屏）。

| 项目 | 参数 |
|------|------|
| 主控 | ESP32-S3 双核 240MHz |
| 存储 | 16MB Flash + 8MB PSRAM (N16R8) |
| 屏幕 | 2.8" IPS TFT，240×320，ILI9341V，4-Line SPI |
| 触摸 | FT6336G 电容触摸 (I2C 0x38) |
| 音频 | ES8311 编解码 + SC8002B 功放 + MEMS 麦克风 |
| 无线 | Wi-Fi 2.4GHz、蓝牙 5.0 |
| 供电 | USB Type-C 5V，支持 3.7V 锂电池（TP4054 充电管理） |
| 其他 | RGB LED (IO42)、MicroSD（SDIO）、电池 ADC |

完整引脚分配见板子文档：[`main/boards/lcdwiki-es3c28p/README.md`](main/boards/lcdwiki-es3c28p/README.md)
硬件资料：<https://www.lcdwiki.com/zh/2.8inch_ESP32-S3_Display>

## 编译与烧录

需要 ESP-IDF v6.0.1 或更高版本（推荐 v6.1，本项目在 v6.0.2 下验证）。

```bash
# 配置 ESP-IDF 环境
source /path/to/esp-idf/export.sh

# 编译（自动选择 lcdwiki-es3c28p 板子）
python3 scripts/build.py lcdwiki-es3c28p

# 烧录
idf.py -p /dev/cu.usbmodem11101 flash

# 生成发布包（merged-binary.bin 合并固件，整片烧录到偏移 0x0）
python3 scripts/build.py lcdwiki-es3c28p --zip
```

发布包输出到 `releases/v<版本>_lcdwiki-es3c28p.zip`。

## 首次使用：配置两个 API Key

仪表盘的天气和黄历数据分别需要两个免费 API 的 Key。**推荐直接用语音配置**，
也可以用 NVS 在线工具写入。

### 方式一：语音配置（推荐）

设备联网并进入对话后，直接对小智说：

- “配置天气密钥，Key 是 xxxx”
- “配置老黄历密钥，Key 是 xxxx”
- “把城市改成北京”

设备通过 MCP 工具将 Key 保存到 NVS 并立即刷新数据。

- **和风天气 Key**：<https://dev.qweather.com/> 注册后在控制台创建项目和凭证获取
- **聚合数据（万年历）Key**：<https://www.juhe.cn/docs/api/id/177> 注册、实名认证并添加
  “万年历”API 后获取；免费会员每天 50 次请求，设备已做每日缓存，正常使用每天仅请求一次

### 方式二：NVS 在线工具

打开 [ESP NVS 在线读取与修改](https://www.16302.com/nvsreadwrite)，连接设备并读取 NVS，
编辑以下键值后写回设备并重启：

| 命名空间 | 键 | 内容 |
|----------|----|------|
| almanac | juhe_key | 聚合数据 AppKey |
| weather | qweather_key | 和风天气 AppKey |

## 小智官方文档

- [自定义板子指南](docs/custom-board.md)
- [WebSocket 通信协议](docs/websocket.md)
- [MQTT + UDP 通信协议](docs/mqtt-udp.md)
- [MCP 协议与设备控制](docs/mcp-protocol.md)

## 致谢与协议

本项目基于 [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32)，遵循 MIT 开源协议，
感谢小智项目全体作者的开源贡献。
