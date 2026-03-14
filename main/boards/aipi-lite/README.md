# 编译命令

## 一键编译

```bash
python scripts/release.py aipi-lite
```

## 手动配置编译

```bash
idf.py set-target esp32s3
```

**配置**

```bash
idf.py menuconfig
```

选择板子

```
Xiaozhi Assistant -> Board Type -> AIPI-Lite
```

## 编译烧入

```bash
idf.py -DBOARD_NAME=aipi-lite build flash
```

注意: 如果当前设备出货之前是AiPi-Lite 固件(非小智版本),请特别小心处理闪存固件分区地址，以避免错误擦除 AiPi-Lite 的自身设备信息（EUI 等），否则设备即使恢复成Xorigin固件也无法正确连接到 服务器！所以在刷写固件之前，请务必记录设备的相关必要信息，以确保有恢复的方法！

您可以使用以下命令备份生产信息

```bash
# firstly backup the factory information partition which contains the credentials for connecting the SenseCraft server
esptool.py --chip esp32s3 --baud 2000000 --before default_reset --after hard_reset --no-stub read_flash 0x9000 16384 nvsfactory.bin

```