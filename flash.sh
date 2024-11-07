#!/bin/sh
esptool.py -p /dev/ttyACM0 -b 2000000 write_flash 0 releases/v0.6.2_ML307/merged-binary.bin
