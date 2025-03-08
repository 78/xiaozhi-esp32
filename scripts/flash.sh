#!/bin/sh
esptool.py -p /dev/ttyACM0 -b 2000000 write_flash 0 ../releases/v0.9.9_bread-compact-wifi/merged-binary.bin
