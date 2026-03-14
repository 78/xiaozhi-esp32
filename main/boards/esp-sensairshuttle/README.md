# ESP-SensairShuttle

## 简介

<div align="center">
    <a href="https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32c5/esp-sensairshuttle/index.html">
        <b> 开发版文档 </b>
    </a>
    |
    <a href="#传感器--shuttleboard-子板支持">
        <b> 传感器 & <i>ShuttleBoard</i> 文档 </b>
    </a>
</div>

ESP-SensairShuttle 是乐鑫携手 Bosch Sensortec 面向**动作感知**与**大模型人机交互**场景联合推出的开发板。

ESP-SensairShuttle 主控采用乐鑫 ESP32-C5-WROOM-1-N16R8 模组，具有 2.4 & 5 GHz 双频 Wi-Fi 6 (802.11ax)、Bluetooth® 5 (LE)、Zigbee 及 Thread (802.15.4) 无线通信能力。

## 传感器 & _ShuttleBoard_ 子板支持

即将推出，敬请期待。

## 配置、编译命令

由于 ESP-SensairShuttle 需要配置较多的 sdkconfig 选项，推荐使用编译脚本编译。

**编译**

```bash
python ./scripts/release.py esp-sensairshuttle
```

如需手动编译，请参考 `main/boards/esp-sensairshuttle/config.json` 修改 menuconfig 对应选项。

**烧录**

```bash
idf.py flash
```
