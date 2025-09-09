# Build Instructions

## One-click Build

```bash
python scripts/release.py sensecap-watcher -c config_en.json
```

## Manual Configuration and Build

```bash
idf.py set-target esp32s3
```

**Configuration**

```bash
idf.py menuconfig
```

Select the board:

```
Xiaozhi Assistant -> Board Type -> SenseCAP Watcher
```

There are some additional configuration options for the watcher. Please select them in menuconfig:

```
CONFIG_BOARD_TYPE_SENSECAP_WATCHER=y
CONFIG_ESPTOOLPY_FLASHSIZE_32MB=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions/v2/32m.csv"
CONFIG_BOOTLOADER_CACHE_32BIT_ADDR_QUAD_FLASH=y
CONFIG_ESPTOOLPY_FLASH_MODE_AUTO_DETECT=n
CONFIG_IDF_EXPERIMENTAL_FEATURES=y
CONFIG_LANGUAGE_EN_US=y
CONFIG_SR_WN_WN9_JARVIS_TTS=y
```

## Build and Flash

```bash
idf.py -DBOARD_NAME=sensecap-watcher-en build flash
```

Note: If your device was previously shipped with the SenseCAP firmware (not the Xiaozhi version), please be very careful with the flash partition addresses to avoid accidentally erasing the device information (such as EUI) of the SenseCAP Watcher. Otherwise, even if you restore the SenseCAP firmware, the device may not be able to connect to the SenseCraft server correctly! Therefore, before flashing the firmware, be sure to record the necessary device information to ensure you have a way to recover it!

You can use the following command to back up the factory information:

```bash
# Firstly backup the factory information partition which contains the credentials for connecting the SenseCraft server
esptool.py --chip esp32s3 --baud 2000000 --before default_reset --after hard_reset --no-stub read_flash 0x9000 204800 nvsfactory.bin
```
