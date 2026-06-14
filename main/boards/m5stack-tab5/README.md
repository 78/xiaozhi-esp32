# M5Stack Tab5

- **MCU**: ESP32-P4
- **Flash**: 16MB
- **Wi-Fi**: ESP32-C6, ESP-Hosted SDIO
- **Display**: 720x1280 MIPI LCD, Capacitive Touch Panel
- **Camera**: SC202CS

-----------
## 快速体验

在 [M5Burner](https://docs.m5stack.com/zh_CN/uiflow/m5burner/intro) 选择 Tab5, 搜索小智下载固件.

-----------
## 基础使用

* idf version: v5.5.2 or above (recommended: v6.0-dev)

* No dependency override needed — the project already specifies the correct `esp_video` and `esp_ipa` versions in `main/idf_component.yml`. Do NOT change the dependency versions unless you are also modifying the source code to match the older API.

针对 ESP32-P4 Rev <3.0 用户:
确保你的 sdkconfig.defaults 包含:

```text
CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y
```

否则烧写的时候会出现：`bootloader/bootloader.bin requires chip revision in range [v3.0 - v3.99] (this chip is revision v1.x)`

-----------
## 手动配置

配置编译目标:

```bash
idf.py set-target esp32p4
```

打开配置菜单:

```bash
idf.py menuconfig
```

选择板卡:

```text
Xiaozhi Assistant -> Board Type -> M5Stack Tab5
```

配置 Flash 大小:

```text
Serial flasher config -> Flash size -> 16 MB
```

配置分区表:

```text
Partition Table -> Custom partition CSV file -> partitions/v2/16m.csv
```

Tab5 使用 ESP32-C6 作为 Wi-Fi 协处理器, 并通过 ESP-Hosted SDIO 连接. 手动构建前需要确认 SDIO 引脚配置, 建议通过 `idf.py menuconfig` 配置, 不要直接修改生成后的 `sdkconfig`.

在 `menuconfig` 中配置 ESP-Hosted Wi-Fi, 并按 Tab5 连接关系设置 SDIO 引脚:

```text
Component config -> ESP-Hosted config -> Hosted Enable -> Enable
Component config -> ESP-Hosted config -> Transport layer -> SDIO
Component config -> ESP-Hosted config -> Slave chipset target -> ESP32-C6
Component config -> ESP-Hosted config -> SDIO GPIO reset active level -> Active High
Component config -> ESP-Hosted config -> SDIO slot -> Slot 1
Component config -> ESP-Hosted config -> SDIO bus width -> 4-bit
Component config -> ESP-Hosted config -> CMD GPIO number -> 13
Component config -> ESP-Hosted config -> CLK GPIO number -> 12
Component config -> ESP-Hosted config -> D0 GPIO number -> 11
Component config -> ESP-Hosted config -> D1 GPIO number -> 10
Component config -> ESP-Hosted config -> D2 GPIO number -> 9
Component config -> ESP-Hosted config -> D3 GPIO number -> 8
Component config -> ESP-Hosted config -> GPIO pin for Reseting slave ESP -> 15
```

编译:

```bash
idf.py build flash monitor
```

> [!NOTE]
> 进入下载模式: 长按复位按键(约 2 秒), 直至内部绿色 LED 指示灯开始快速闪烁, 松开按键.

-----------
## release.py 构建

`main/boards/m5stack-tab5/config.json` 已经包含 Tab5 需要的 board 选项和 ESP-Hosted SDIO 引脚配置, 可以使用:

```bash
python ./scripts/release.py m5stack-tab5
```

如需构建 P4X 版本:

```bash
python ./scripts/release.py m5stack-tab5-p4x
```

-----------
## 合并固件

手动构建后, 可使用以下命令合并烧录固件:

```bash
esptool.py --chip esp32p4 merge_bin \
    --flash_mode dio \
    --flash_freq 80m \
    --flash_size 16MB \
    0x2000 build/bootloader/bootloader.bin \
    0x8000 build/partition_table/partition-table.bin \
    0xd000 build/ota_data_initial.bin \
    0x20000 build/xiaozhi.bin \
    0x800000 build/generated_assets.bin \
    -o M5Stack-Tab5-XiaoZhi-v2.2.6_0x00.bin
```

烧录合并后的固件:

```bash
esptool.py --chip esp32p4 -b 1500000 write_flash -z 0 M5Stack-Tab5-XiaoZhi-v2.2.6_0x00.bin
```
