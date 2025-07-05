# ESP-Hi

## 简介

<div align="center">
    <a href="https://oshwhub.com/esp-college/esp-hi"><b> 立创开源平台 </b></a>
    |
    <a href="https://www.bilibili.com/video/BV1BHJtz6E2S"><b> Bilibili </b></a>
</div>

ESP-Hi 是 ESP Friends 开源的一款基于 ESP32C3 的超**低成本** AI 对话机器人。ESP-Hi 集成了一个0.96寸的彩屏，用于显示表情，**机器狗已实现数十种动作**。通过对 ESP32-C3 外设的充分挖掘，仅需最少的板级硬件即可实现拾音和发声，同步优化了软件，降低内存与 Flash 占用，在资源受限的情况下同时实现了**唤醒词检测**与多种外设驱动。硬件详情等可查看[立创开源项目](https://oshwhub.com/esp-college/esp-hi)。

## WebUI

ESP-Hi x 小智内置了一个控制身体运动的 WebUI，请将手机与 ESP-Hi 连接到同一个 Wi-Fi 下，手机访问 `http://esp-hi.local/` 以使用。

如需禁用，请取消 `ESP_HI_WEB_CONTROL_ENABLED`，即取消勾选 `Component config` → `Servo Dog Configuration` → `Web Control` → `Enable ESP-HI Web Control`。

## 配置、编译命令

由于 ESP-Hi 需要配置较多的 sdkconfig 选项，推荐使用编译脚本编译。

**编译**

```bash
python ./scripts/release.py esp-hi
```

如需手动编译，请参考 `esp-hi/config.json` 修改 menuconfig 对应选项。

**烧录**

```bash
idf.py flash
```


> [!TIP]
>
> **舵机控制会占用 ESP-Hi 的 USB Type-C 接口**，导致无法连接电脑（无法烧录/查看运行日志）。如遇此情况，请按以下提示操作：
>
> **烧录**
>
> 1. 断开 ESP-Hi 的电源，只留头部，不要连接身体。
> 2. 按住 ESP-Hi 的按钮并连接电脑。
> 
> 此时，ESP-Hi (ESP32C3) 应当处于烧录模式，可以使用电脑烧录程序。烧录完成后，可能需要重新插拔电源。
>
> **查看 log**
>
> 请设置 `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y`，即 `Component config` → `ESP System Settings` → `Channel for console output` 选择 `USB Serial/JTAG Controller`。这同时会禁用舵机控制功能。
