# An MCP-based Chatbot

（中文 | [English](README.md) | [日本語](README_ja.md)）

## 介绍

👉 [人类：给 AI 装摄像头 vs AI：当场发现主人三天没洗头【bilibili】](https://www.bilibili.com/video/BV1bpjgzKEhd/)

👉 [手工打造你的 AI 女友，新手入门教程【bilibili】](https://www.bilibili.com/video/BV1XnmFYLEJN/)

小智 AI 聊天机器人作为一个语音交互入口，利用 Qwen / DeepSeek 等大模型的 AI 能力，通过 MCP 协议实现多端控制。

<img src="docs/mcp-based-graph.jpg" alt="通过MCP控制万物" width="320">

## 近期更新

- 项目主线现已迁移到 ESP-IDF v6.0 或以上版本，首选稳定版为 v6.0.2；完整的 157 个发布变体已在 ESP-IDF v6.0.1 上通过构建验证。
- MQTT 和 BluFi 加密已迁移到 PSA Crypto，同时完成了 IDF 6 组件拆分及第三方依赖兼容处理。
- 加固了音频流水线并发、MQTT/UDP 数据包校验和发布矩阵选择逻辑。
- ESP-IDF v5.5 仅保留用于旧版硬件兼容，包括 ESP32-P4 v3 之前的芯片版本。完整兼容性和板卡验证状态请参阅 [ESP-IDF 6.0 迁移文档](docs/esp-idf-6-migration.md)。

### 已实现功能

- 支持 Wi-Fi、有线以太网、USB RNDIS，以及 ML307/EC801E 或 NT26 Cat.1 4G 网络；部分硬件支持 Wi-Fi 与 4G 切换
- 基于 [ESP-SR](https://github.com/espressif/esp-sr) 的离线语音唤醒，支持自定义唤醒词
- 支持两种通信传输方式：[WebSocket](docs/websocket_zh.md) 和 [MQTT + UDP](docs/mqtt-udp_zh.md)
- 采用 Opus 音频流，既支持传统的流式 ASR + LLM + TTS 方案，也支持 Realtime 端到端语音模型；具备 AEC 的硬件可实现实时全双工交互
- 声纹识别，识别当前说话人的身份 [3D Speaker](https://github.com/modelscope/3D-Speaker)
- OLED / LCD 显示屏，支持表情和丰富的情绪呈现；部分硬件支持摄像头视觉输入
- 电量显示与电源管理
- 提供 38 种界面语言；语音提示优先使用本地化资源，缺失时自动回退到英文
- 支持 ESP32、ESP32-C3、ESP32-C5、ESP32-C6、ESP32-S3、ESP32-P4 芯片平台
- 支持热点、声波和 BluFi 三种 Wi-Fi 配网方式
- 通过设备端 MCP 实现设备控制（音量、灯光、电机、GPIO 等）
- 通过云端 MCP 扩展大模型能力（智能家居控制、PC桌面操作、知识搜索、邮件收发等）
- 自定义唤醒词、字体、表情与聊天背景，支持网页端在线修改 ([自定义Assets生成器](https://github.com/78/xiaozhi-assets-generator))

## 硬件

### 面包板手工制作实践

详见飞书文档教程：

👉 [《小智 AI 聊天机器人百科全书》](https://ccnphfhqs21z.feishu.cn/wiki/F5krwD16viZoF0kKkvDcrZNYnhb?from=from_copylink)

面包板效果图如下：

![面包板效果图](docs/v1/wiring2.jpg)

### 支持 137 个板卡目录、157 个固件发布变体（仅展示部分）

- <a href="https://oshwhub.com/li-chuang-kai-fa-ban/li-chuang-shi-zhan-pai-esp32-s3-kai-fa-ban" target="_blank" title="立创·实战派 ESP32-S3 开发板">立创·实战派 ESP32-S3 开发板</a>
- <a href="https://github.com/espressif/esp-box" target="_blank" title="乐鑫 ESP32-S3-BOX3">乐鑫 ESP32-S3-BOX3</a>
- <a href="https://docs.m5stack.com/zh_CN/core/CoreS3" target="_blank" title="M5Stack CoreS3">M5Stack CoreS3</a>
- <a href="https://docs.m5stack.com/en/atom/Atomic%20Echo%20Base" target="_blank" title="AtomS3R + Echo Base">M5Stack AtomS3R + Echo Base</a>
- <a href="https://gf.bilibili.com/item/detail/1108782064" target="_blank" title="神奇按钮 2.4">神奇按钮 2.4</a>
- <a href="https://www.waveshare.net/shop/ESP32-S3-Touch-AMOLED-1.8.htm" target="_blank" title="微雪电子 ESP32-S3-Touch-AMOLED-1.8">微雪电子 ESP32-S3-Touch-AMOLED-1.8</a>
- <a href="https://github.com/Xinyuan-LilyGO/T-Circle-S3" target="_blank" title="LILYGO T-Circle-S3">LILYGO T-Circle-S3</a>
- <a href="https://oshwhub.com/tenclass01/xmini_c3" target="_blank" title="虾哥 Mini C3">虾哥 Mini C3</a>
- <a href="https://oshwhub.com/movecall/cuican-ai-pendant-lights-up-y" target="_blank" title="Movecall CuiCan ESP32S3">璀璨·AI 吊坠</a>
- <a href="https://github.com/WMnologo/xingzhi-ai" target="_blank" title="无名科技Nologo-星智-1.54">无名科技 Nologo-星智-1.54TFT</a>
- <a href="https://www.seeedstudio.com/SenseCAP-Watcher-W1-A-p-5979.html" target="_blank" title="SenseCAP Watcher">SenseCAP Watcher</a>
- <a href="https://www.bilibili.com/video/BV1BHJtz6E2S/" target="_blank" title="ESP-HI 超低成本机器狗">ESP-HI 超低成本机器狗</a>

<div style="display: flex; justify-content: space-between;">
  <a href="docs/v1/lichuang-s3.jpg" target="_blank" title="立创·实战派 ESP32-S3 开发板">
    <img src="docs/v1/lichuang-s3.jpg" width="240" />
  </a>
  <a href="docs/v1/espbox3.jpg" target="_blank" title="乐鑫 ESP32-S3-BOX3">
    <img src="docs/v1/espbox3.jpg" width="240" />
  </a>
  <a href="docs/v1/m5cores3.jpg" target="_blank" title="M5Stack CoreS3">
    <img src="docs/v1/m5cores3.jpg" width="240" />
  </a>
  <a href="docs/v1/atoms3r.jpg" target="_blank" title="AtomS3R + Echo Base">
    <img src="docs/v1/atoms3r.jpg" width="240" />
  </a>
  <a href="docs/v1/magiclick.jpg" target="_blank" title="神奇按钮 2.4">
    <img src="docs/v1/magiclick.jpg" width="240" />
  </a>
  <a href="docs/v1/waveshare.jpg" target="_blank" title="微雪电子 ESP32-S3-Touch-AMOLED-1.8">
    <img src="docs/v1/waveshare.jpg" width="240" />
  </a>
  <a href="docs/v1/lilygo-t-circle-s3.jpg" target="_blank" title="LILYGO T-Circle-S3">
    <img src="docs/v1/lilygo-t-circle-s3.jpg" width="240" />
  </a>
  <a href="docs/v1/xmini-c3.jpg" target="_blank" title="虾哥 Mini C3">
    <img src="docs/v1/xmini-c3.jpg" width="240" />
  </a>
  <a href="docs/v1/movecall-cuican-esp32s3.jpg" target="_blank" title="CuiCan">
    <img src="docs/v1/movecall-cuican-esp32s3.jpg" width="240" />
  </a>
  <a href="docs/v1/wmnologo_xingzhi_1.54.jpg" target="_blank" title="无名科技Nologo-星智-1.54">
    <img src="docs/v1/wmnologo_xingzhi_1.54.jpg" width="240" />
  </a>
  <a href="docs/v1/sensecap_watcher.jpg" target="_blank" title="SenseCAP Watcher">
    <img src="docs/v1/sensecap_watcher.jpg" width="240" />
  </a>
  <a href="docs/v1/esp-hi.jpg" target="_blank" title="ESP-HI 超低成本机器狗">
    <img src="docs/v1/esp-hi.jpg" width="240" />
  </a>
</div>

## 软件

### 固件烧录

新手第一次操作建议先不要搭建开发环境，直接使用免开发环境烧录的固件。

固件默认接入 [xiaozhi.me](https://xiaozhi.me) 官方服务器，个人用户注册账号可以免费使用 Qwen 实时模型。

👉 [新手烧录固件教程](https://ccnphfhqs21z.feishu.cn/wiki/Zpz4wXBtdimBrLk25WdcXzxcnNS)

### 开发环境

- Cursor 或 VSCode
- 安装 ESP-IDF 插件，首选 [ESP-IDF v6.0.2](https://github.com/espressif/esp-idf/releases/tag/v6.0.2)，建议使用 v6.0 或以上的稳定版本；ESP-IDF v5.5.2 仅保留用于旧版硬件兼容
- Linux 比 Windows 更好，编译速度快，也免去驱动问题的困扰
- 本项目使用 Google C++ 代码风格，提交代码时请确保符合规范

### 开发者文档

- [ESP-IDF 6.0 迁移文档](docs/esp-idf-6-migration.md) - SDK 兼容性、组件变更、旧版硬件支持和板卡验证状态
- [自定义开发板指南](docs/custom-board_zh.md) - 学习如何为小智 AI 创建自定义开发板
- [MCP 协议物联网控制用法说明](docs/mcp-usage_zh.md) - 了解如何通过 MCP 协议控制物联网设备
- [MCP 协议交互流程](docs/mcp-protocol_zh.md) - 设备端 MCP 协议的实现方式
- [MQTT + UDP 混合通信协议文档](docs/mqtt-udp_zh.md)
- [一份详细的 WebSocket 通信协议文档](docs/websocket_zh.md)

## 大模型配置

如果你已经拥有一个小智 AI 聊天机器人设备，并且已接入官方服务器，可以登录 [xiaozhi.me](https://xiaozhi.me) 控制台进行配置。

👉 [后台操作视频教程（旧版界面）](https://www.bilibili.com/video/BV1jUCUY2EKM/)

## 相关开源项目

在个人电脑上部署服务器，可以参考以下第三方开源的项目：

- [xinnan-tech/xiaozhi-esp32-server](https://github.com/xinnan-tech/xiaozhi-esp32-server) Python 服务器
- [joey-zhou/xiaozhi-esp32-server-java](https://github.com/joey-zhou/xiaozhi-esp32-server-java) Java 服务器
- [AnimeAIChat/xiaozhi-server-go](https://github.com/AnimeAIChat/xiaozhi-server-go) Golang 服务器
- [hackers365/xiaozhi-esp32-server-golang](https://github.com/hackers365/xiaozhi-esp32-server-golang) Golang 服务器

使用小智通信协议的第三方客户端项目：

- [huangjunsen0406/py-xiaozhi](https://github.com/huangjunsen0406/py-xiaozhi) Python 客户端
- [TOM88812/xiaozhi-android-client](https://github.com/TOM88812/xiaozhi-android-client) Android 客户端
- [100askTeam/xiaozhi-linux](http://github.com/100askTeam/xiaozhi-linux) 百问科技提供的 Linux 客户端
- [78/xiaozhi-sf32](https://github.com/78/xiaozhi-sf32) 思澈科技的蓝牙芯片固件
- [QuecPython/solution-xiaozhiAI](https://github.com/QuecPython/solution-xiaozhiAI) 移远提供的 QuecPython 固件

## 关于项目

这是一个由虾哥开源的 ESP32 项目，以 MIT 许可证发布，允许任何人免费使用，修改或用于商业用途。

我们希望通过这个项目，能够帮助大家了解 AI 硬件开发，将当下飞速发展的大语言模型应用到实际的硬件设备中。

如果你有任何想法或建议，请随时提出 Issues 或加入 [Discord](https://discord.gg/C759fGMBcZ) 或 QQ 群：1011329060

## Star History

<a href="https://star-history.com/#78/xiaozhi-esp32&Date">
 <picture>
   <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=78/xiaozhi-esp32&type=Date&theme=dark" />
   <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=78/xiaozhi-esp32&type=Date" />
   <img alt="Star History Chart" src="https://api.star-history.com/svg?repos=78/xiaozhi-esp32&type=Date" />
 </picture>
</a>
