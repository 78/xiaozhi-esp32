# XiaoZhi AI Chatbot

（[中文](README.md) | English | [日本語](README_ja.md)）

This is Terrence's first hardware project.

👉 [Build your AI chat companion with ESP32+SenseVoice+Qwen72B! [bilibili]](https://www.bilibili.com/video/BV11msTenEH3/?share_source=copy_web&vd_source=ee1aafe19d6e60cf22e60a93881faeba)

👉 [DIY Your AI Companion - Beginner's Tutorial [bilibili]](https://www.bilibili.com/video/BV1XnmFYLEJN/)

## Project Purpose

This project is developed based on Espressif's ESP-IDF.

This is an open-source project primarily for educational purposes. Through this project, we aim to help more people get started with AI hardware development and understand how to integrate rapidly evolving large language models into actual hardware devices. Whether you're a student interested in AI or a developer looking to explore new technologies, this project offers valuable learning experiences.

Everyone is welcome to participate in the project's development and improvement. If you have any ideas or suggestions, please feel free to raise an Issue or join our chat group.

Learning & Discussion QQ Group: 946599635

## Implemented Features

- Wi-Fi / ML307 Cat.1 4G
- BOOT button wake-up and interrupt, supporting both click and long-press triggers
- Offline voice wake-up [ESP-SR](https://github.com/espressif/esp-sr)
- Streaming voice dialogue (WebSocket or UDP protocol)
- Support for 5 languages: Mandarin, Cantonese, English, Japanese, Korean [SenseVoice](https://github.com/FunAudioLLM/SenseVoice)
- Voice print recognition to identify who's calling AI's name [3D Speaker](https://github.com/modelscope/3D-Speaker)
- Large model TTS (Volcengine or CosyVoice)
- Large Language Model (Qwen2.5 72B or Doubao API)
- Configurable prompts and voice tones (custom characters)
- Short-term memory with self-summary after each conversation round
- OLED / LCD display showing signal strength or conversation content

## Hardware Section

### Breadboard Practice

For detailed tutorial, see the Feishu document:

👉 [XiaoZhi AI Chatbot Encyclopedia](https://ccnphfhqs21z.feishu.cn/wiki/F5krwD16viZoF0kKkvDcrZNYnhb?from=from_copylink)

Breadboard setup shown below:

![Breadboard Setup](docs/wiring2.jpg)

### Supported Open-Source Hardware

- <a href="https://oshwhub.com/li-chuang-kai-fa-ban/li-chuang-shi-zhan-pai-esp32-s3-kai-fa-ban" target="_blank" title="LiChuang ESP32-S3 Development Board">LiChuang ESP32-S3 Development Board</a>
- <a href="https://github.com/espressif/esp-box" target="_blank" title="Espressif ESP32-S3-BOX3">Espressif ESP32-S3-BOX3</a>
- <a href="https://docs.m5stack.com/zh_CN/core/CoreS3" target="_blank" title="M5Stack CoreS3">M5Stack CoreS3</a>
- <a href="https://docs.m5stack.com/en/atom/Atomic%20Echo%20Base" target="_blank" title="AtomS3R + Echo Base">AtomS3R + Echo Base</a>
- MagiClick 2.4
- <a href="https://oshwhub.com/tenclass01/xmini_c3" target="_blank" title="Xmini C3">Xmini C3</a>
- <a href="https://www.waveshare.com/esp32-s3-touch-amoled-1.8.htm" target="_blank" title="Waveshare ESP32-S3-Touch-AMOLED-1.8">Waveshare ESP32-S3-Touch-AMOLED-1.8</a>

<div style="display: flex; justify-content: space-between;">
  <a href="docs/lichuang-s3.jpg" target="_blank" title="LiChuang ESP32-S3 Development Board">
    <img src="docs/lichuang-s3.jpg" width="240" />
  </a>
  <a href="docs/esp32s3-box3.jpg" target="_blank" title="Espressif ESP32-S3-BOX3">
    <img src="docs/esp32s3-box3.jpg" width="240" />
  </a>
  <a href="docs/m5stack-cores3.jpg" target="_blank" title="M5Stack CoreS3">
    <img src="docs/m5stack-cores3.jpg" width="240" />
  </a>
  <a href="docs/atoms3r-echo-base.jpg" target="_blank" title="AtomS3R + Echo Base">
    <img src="docs/atoms3r-echo-base.jpg" width="240" />
  </a>
  <a href="docs/magiclick-2p4.jpg" target="_blank" title="MagiClick 2.4">
    <img src="docs/magiclick-2p4.jpg" width="240" />
  </a>
  <a href="docs/xmini-c3.jpg" target="_blank" title="Xmini C3">
    <img src="docs/xmini-c3.jpg" width="240" />
  </a>
</div>

## Firmware Section

### Flashing Without Development Environment

For beginners, it's recommended to first try flashing the firmware without setting up a development environment. The firmware uses a test server provided by the author, currently available for free use (not for commercial purposes).

👉 [Flash Firmware Guide (No IDF Environment Required)](https://ccnphfhqs21z.feishu.cn/wiki/Zpz4wXBtdimBrLk25WdcXzxcnNS)

### Development Environment

- Cursor or VSCode
- Install ESP-IDF plugin, select SDK version 5.3 or above
- Linux is preferred over Windows for faster compilation and fewer driver issues

## AI Character Configuration

If you already have a XiaoZhi AI chatbot, please refer to 👉 [Backend Operation Video Tutorial](https://www.bilibili.com/video/BV1jUCUY2EKM/)

For detailed usage instructions and test server notes, please refer to 👉 [XiaoZhi Test Server Help Guide](https://xiaozhi.me/help).

## Star History

<a href="https://star-history.com/#78/xiaozhi-esp32&Date">
 <picture>
   <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=78/xiaozhi-esp32&type=Date&theme=dark" />
   <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=78/xiaozhi-esp32&type=Date" />
   <img alt="Star History Chart" src="https://api.star-history.com/svg?repos=78/xiaozhi-esp32&type=Date" />
 </picture>
</a> 
