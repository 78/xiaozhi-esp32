# Waveshare ESP32-C6-Touch-LCD-2.8

微雪 ESP32-C6-Touch-LCD-2.8 开发板，配备 240 × 320 ST7789 触摸屏、
CST3530 触摸控制器、ES8311 音频编解码器和 CH32V003 IO 扩展芯片，
支持语音对话、触摸操作及语音调节音量和屏幕亮度。

## 硬件配置

主控为 ESP32-C6，16 MB Flash，无 PSRAM，使用 2.4 GHz Wi-Fi。
引脚分配、复位时序、屏幕时钟、功放使能和背光 PWM 均依据本产品的出厂 BSP。

| 功能 | 连接 |
| --- | --- |
| 共用 I2C SDA / SCL | GPIO6 / GPIO7 |
| LCD SCLK / MOSI | GPIO0 / GPIO1，与 SD 卡共用 SPI |
| LCD DC / CS | GPIO10 / GPIO11 |
| LCD 复位 | CH32V003 EXIO1 |
| LCD 时钟 | 20 MHz |
| SD MISO / CS | GPIO8 / GPIO23；未使用 SD 卡时保持 CS 为高电平 |
| 触摸 | CST3530，地址 0x58；INT 为 GPIO18，复位为 CH32V003 EXIO0 |
| 音频 BCLK / WS / DOUT / DIN | GPIO15 / GPIO3 / GPIO2 / GPIO20 |
| 音频 MCLK | 未连接；ES8311 使用 BCLK |
| 功放使能 | CH32V003 EXIO3，高电平使能 |
| 背光 | CH32V003 PWM，0～255，非反相 |
| BOOT | GPIO9，低电平有效 |

## 编译与烧录

使用 ESP-IDF v6.0.1 或更新版本，推荐 `release/v6.1` 分支。加载 ESP-IDF 开发环境后，
在小智项目根目录执行：

```sh
python scripts/build.py waveshare/esp32-c6-touch-lcd-2.8 --name esp32-c6-touch-lcd-2.8 --language zh-CN
idf.py flash monitor
```

如需指定串口，将最后一条命令改为 `idf.py -p <串口> flash monitor`。
构建脚本会选择 ESP32-C6 板型、分区和语音模型；中文构建默认使用“你好小智”唤醒词。
语言和唤醒词可通过项目构建选项调整。手动使用 `idf.py` 构建时，请选择
`Xiaozhi Assistant -> Board Type -> Waveshare ESP32-C6-Touch-LCD-2.8`，
并配置语言和唤醒词。ESP32-C6 不支持 PSRAM，请勿沿用 C5 的 PSRAM 配置。

使用前，请确认 CH32V003 已烧录本产品的出厂 IO 扩展固件。

## 配网与使用

首次启动时，根据屏幕提示完成 Wi-Fi 配置和设备激活。设备仅支持 2.4 GHz Wi-Fi。
联网后，说“你好小智”唤醒设备；触摸输入已接入聊天界面的 LVGL 输入设备。

支持以下语音操作：

- “音量调到 50”：调节扬声器音量，范围 0～100。
- “屏幕亮度调到 80”：调节背光亮度，范围 0～100。

上述控制使用小智标准工具 `self.audio_speaker.set_volume` 和
`self.screen.set_brightness`，需要所连接的服务支持设备工具调用。
音量和永久亮度设置使用小智现有的设置存储机制。

## BOOT 按键

| 当前状态 | 单击功能 |
| --- | --- |
| 开机连接 Wi-Fi | 进入配网 |
| 待机 | 开始语音对话 |
| 正在播报 | 打断播报 |
| 正在聆听 | 结束当前对话 |
| 配网界面 | 进入音频测试，再按一次返回配网 |

出厂测试应用、SD 卡文件系统、陀螺仪、RTC 和温湿度传感器不在本适配的初始化范围内。
