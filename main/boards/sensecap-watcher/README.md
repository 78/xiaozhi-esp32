# 编译配置命令

**配置编译目标为 ESP32S3：**

```bash
idf.py set-target esp32s3
```

**打开 menuconfig：**

```bash
idf.py menuconfig
```

**选择板子：**

```
Xiaozhi Assistant -> Board Type -> SenseCAP Watcher
```

watcher 中一些额外的配置项如下，需要menuconfig 选择， 或者拷贝放入sdkconfig.defaults中.

```
CONFIG_BOARD_TYPE_SENSECAP_WATCHER=y
CONFIG_ESPTOOLPY_FLASHSIZE_32MB=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions/v1/32m.csv"
CONFIG_BOOTLOADER_CACHE_32BIT_ADDR_QUAD_FLASH=y
CONFIG_ESPTOOLPY_FLASH_MODE_AUTO_DETECT=n
CONFIG_IDF_EXPERIMENTAL_FEATURES=y
```

**编译烧入：**

```bash
idf.py build flash
```
注意: 请特别小心处理闪存固件分区地址，以避免错误擦除 SenseCAP Watcher 的自身设备信息（EUI 等），否则设备可能无法正确连接到 SenseCraft 服务器！在刷写固件之前，请务必记录设备的相关必要信息，以确保有恢复的方法！

您可以使用以下命令备份生产信息

```bash
# firstly backup the factory information partition which contains the credentials for connecting the SenseCraft server
esptool.py --chip esp32s3 --baud 2000000 --before default_reset --after hard_reset --no-stub read_flash 0x9000 204800 nvsfactory.bin

```