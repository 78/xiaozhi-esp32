# 使用说明 

* [ESP32-S3-Touch-LCD-4.3C docs](https://www.waveshare.com/esp32-s3-touch-lcd-4.3c.htm)

## 快速体验

下载编译好的 [固件](https://files.waveshare.com/wiki/ESP32-S3-Touch-LCD-4.3C/ESP32-S3-Touch-LCD-4.3C-Xiaozhi.bin) 

```shell
esptool.py --chip esp32s3 -p /dev/ttyACM0 -b 460800 --before=default_reset --after=hard_reset write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB 0x00 ESP32-S3-Touch-LCD-4.3C-Xiaozhi.bin 
```

## 基础使用

* idf version: v5.5-dev

1. 设置编译目标为 esp32s3

```shell
idf.py set-target esp32s3
```

2. 修改配置 

```shell
cp main/boards/waveshare-s3-touch-lcd-4.3c/sdkconfig.4_3c sdkconfig
```

3. 编译烧录程序

```shell
idf.py build flash monitor
```

## log

@2025/05/17 测试问题

1. 返回应用界面时，需要存在此分区，否则无效
2. 
3. 
 
## TODO
