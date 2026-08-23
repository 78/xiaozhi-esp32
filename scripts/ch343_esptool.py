#!/usr/bin/env python3
"""Run esptool with the conservative packet sizes required by CH343 adapters.

Kevin Box 2 units using the CH343 USB serial bridge can reject the default
ESP32-S3 stub packet size with checksum errors.  Keep this wrapper deliberately
small so every backup/flash command remains a normal, auditable esptool command
while using the verified 256-byte RAM and flash transfer sizes.
"""

from esptool import main
from esptool.targets.esp32s3 import ESP32S3ROM, ESP32S3StubLoader


ESP32S3ROM.ESP_RAM_BLOCK = 256
ESP32S3StubLoader.FLASH_WRITE_SIZE = 256


if __name__ == "__main__":
    main()
