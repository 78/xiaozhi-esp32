# ESP32-S31-Korvo-1

本板型适配乐鑫 ESP32-S31-Korvo-1 开发板（V1.1），需要 ESP-IDF `release/v6.1`
或更新版本（`esp32s31` target）。

## 支持的板载硬件

- ESP32-S31-WROOM-3 模组（16MB Flash + 16MB 八线 PSRAM，Wi-Fi 6 / BT 5.4）
- ES8389 双声道音频编解码（I2C 地址 0x20，从 BCLK 恢复时钟，use_mclk=false）
- 双模拟麦克风阵列（双通道输入）+ 双 NS4150B 功放（立体声输出）
- RGB 接口 LCD 子板（ST7262E43，800x480，16MHz PCLK，背光常开）
- GT1151 触摸（与 codec 共享 I2C 总线）
- OV3660 摄像头（DVP 8bit，SCCB 共享 I2C 总线，地址 0x78，180° 旋转安装）
- WS2812 RGB 状态灯（GPIO37）
- BOOT 键（GPIO61）+ 4 个 ADC 按键（GPIO42，电阻分压）
- microSD 卡槽、USB-A OTG（暂未启用）

## ADC 按键方案

采用官方 factory demo BSP 的方案（移植自 esp-dev-kits）：

- **S31 SAR ADC 软件校准**（`esp32_s31_adc_calibration.c`）：通过内部校准
  寄存器计算 17 位权重，使无硬件校准方案的 ESP32-S31 获得准确电压读数；
- **中心电压取分压理论值**：SET=1870mV，MODE=1340mV，VOL-=820mV，VOL+=380mV；
- **按键窗口取相邻键中心的中点**自动划分，无需手工调参。

> 注意：S31 的 SAR ADC 原始值是反相的（电压越高 raw 越小），未经校准的
> 直读公式会得出相反的键序——键位判断必须以校准后的电压为准。

## 音频引脚

| 信号 | GPIO |
| --- | ---: |
| I2C SDA（codec + 触摸 + 摄像头 SCCB 共享） | 0 |
| I2C SCL | 1 |
| I2S MCLK（仅驱动该脚，codec 不使用） | 2 |
| I2S BCLK / SCLK | 3 |
| I2S WS / LRCK | 4 |
| I2S DOUT（接 codec DSIN，播放） | 5 |
| I2S DIN（接 codec ASDOUT，录音） | 6 |
| NS4150B 功放使能（两路共用） | 7 |

> 原理图/文档中的 I2S 网络名是 codec 视角，与 SoC 视角相反，移植时注意。

## 摄像头引脚

| 信号 | GPIO |
| --- | ---: |
| D0 ~ D7 | 46 ~ 53 |
| PCLK | 54 |
| XCLK（20MHz） | 55 |
| VSYNC | 56 |
| HREF | 57 |
| RESET / PWDN | 未连接 |

## 构建

```sh
export IDF_CCACHE_ENABLE=1
idf.py set-target esp32s31
idf.py menuconfig   # Xiaozhi Assistant -> Board Type -> Espressif ESP32-S31-Korvo-1
idf.py build flash monitor
```

摄像头（OV3660）默认随固件启用（`config.json` 的 `sdkconfig_append` 已包含
`CONFIG_CAMERA_OV3660=y`）；若手工配置 sdkconfig，请确认该项开启。

硬件验证建议覆盖：麦克风采集、扬声器播放（立体声）、唤醒词、ADC 四键、
BOOT 键、RGB 灯、屏幕显示、触摸、拍照（方向应为正，偏色则检查 RGB565
字节序）、Wi-Fi 配网与重连。

## 参考资料

- [ESP32-S31-Korvo-1 用户指南](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32s31/esp32-s31-korvo-1/user_guide.html)
- [原理图](https://dl.espressif.com/schematics/esp32-s31-korvo-1-schematics.pdf)
- [官方 factory demo（esp-dev-kits examples/esp32-s31-korvo）](https://github.com/espressif/esp-dev-kits/tree/master/examples/esp32-s31-korvo/examples/factory_demo)
