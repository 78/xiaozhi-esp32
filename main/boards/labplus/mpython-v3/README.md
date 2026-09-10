# labplus 掌控板V3

## 板载资源
    主控：ESP32-S3 外挂8MB psram 16MB flash	
    传感器:
        按钮(A B按键）	IO0 IO46
        光照传感器	IIC
        6轴	IIC
        磁力计	IIC
        声音触发	IO6
        触摸按键  P Y T H O N
        摄像头	IIC
    执行器:
        蜂鸣器	IO21
        RGB灯	IO16
        录音播放 es8388	IIC	
        TFT LCD	jd9853 SPI


## 编译配置

### 配置编译目标为 ESP32S3，USB JTAG下载

```bash
idf.py set-target esp32s3
```

### menuconfig配置

```bash
idf.py menuconfig
```

***选择板子：***

```
Xiaozhi Assistant -> Board Type -> labplus mpython_v3 board
```

***修改 psram 配置：***

```
Component config -> ESP PSRAM -> SPI RAM config -> Mode (QUAD/OCT) -> quad Mode PSRAM
```

**编译：**

```bash
idf.py build
```

**固件打包：**

```bash
esptool.py -p /dev/ttyACM0 -b 1500000 --before default_reset --after hard_reset --chip esp32s3 write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB 0x0 bootloader/bootloader.bin 0x100000 xiaozhi.bin 0x8000 partition_table/partition-table.bin 0xd000 ota_data_initial.bin 0x10000 srmodels/srmodels.bin 
```

## 使用

### 按键配置
* A：短按-打断/唤醒
