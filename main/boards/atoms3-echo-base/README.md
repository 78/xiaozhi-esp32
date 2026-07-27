# M5Stack AtomS3 + Echo Base

## 快速构建

推荐使用 release 脚本生成完整固件包：

```bash
python scripts/release.py atoms3-echo-base --name atoms3-echo-base
```

生成的固件压缩包位于：

```text
releases/v2.2.6_atoms3-echo-base.zip
```

AtomS3 不带 PSRAM，`config.json` 已通过 `CONFIG_SPIRAM=n` 关闭片外 PSRAM，并使用 8 MB Flash 分区配置。由于没有 PSRAM，AtomS3 + Echo Base 不支持语音唤醒功能，手动配置时需要关闭语音唤醒与音频处理。

## 手动配置

配置编译目标：

```bash
idf.py set-target esp32s3
```

打开配置菜单：

```bash
idf.py menuconfig
```

选择板卡：

```text
Xiaozhi Assistant -> Board Type -> AtomS3 + Echo Base
```

关闭语音唤醒与音频处理：

```text
Xiaozhi Assistant -> [ ] 启用语音唤醒与音频处理 -> Unselect
```

配置 Flash 大小：

```text
Serial flasher config -> Flash size -> 8 MB
```

配置分区表：

```text
Partition Table -> Custom partition CSV file -> partitions/v2/8m.csv
```

关闭片外 PSRAM：

```text
Component config -> ESP PSRAM -> [ ] Support for external, SPI-connected RAM -> Unselect
```

对应 `sdkconfig` 设置为：

```text
CONFIG_SPIRAM=n
CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions/v2/8m.csv"
```

编译：

```bash
idf.py build
```

## 合并固件

手动构建后，可使用以下命令合并烧录固件：

```bash
esptool.py --chip esp32s3 merge_bin \
    --flash_mode dio \
    --flash_freq 80m \
    --flash_size 8MB \
    0x0 build/bootloader/bootloader.bin \
    0x8000 build/partition_table/partition-table.bin \
    0xd000 build/ota_data_initial.bin \
    0x20000 build/xiaozhi.bin \
    0x600000 build/generated_assets.bin \
    -o AtomS3-EchoBase-XiaoZhi-v2.2.6_0x00.bin
```

烧录合并后的固件：

```bash
esptool.py -b 1500000 write_flash -z 0 AtomS3-EchoBase-XiaoZhi-v2.2.6_0x00.bin
```

## 使用说明

Echo Base 正常运行时请从 Echo Base 底座的 USB-C 口供电；AtomS3 的 USB-C 口主要用于烧录。
