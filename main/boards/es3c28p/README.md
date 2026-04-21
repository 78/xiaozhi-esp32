# ES3C28P — 2.8" ESP32-S3 Display Board (XiaoZhi Board Support)

Manufacturer: **LCDWIKI / QDtech (ShenZhen QDtech Electronic Technology Co., Ltd.)**  
Product page: https://www.lcdwiki.com/2.8inch_ESP32-S3_Display

## Hardware Summary

| Component | Details |
|-----------|---------|
| MCU | ESP32-S3-WROOM-1 (N16R8) — 16 MB Flash, 8 MB OPI PSRAM |
| Display | 2.8" IPS TFT, 240×320, ILI9341V driver, 4-wire SPI |
| Touch | FT6336G capacitive, I2C |
| Audio codec | ES8311 (I2C control + I2S data) |
| Microphone | Single MEMS mic, I2S input |
| Speaker amp | FM8002E (enable active LOW on GPIO1) |
| RGB LED | Single WS2812 on GPIO42 |
| SD card | SDIO (not configured in this board support) |
| USB | GPIO19 (D−) / GPIO20 (D+) |
| Power | USB Type-C + external LiPo via TP4054 charger |

## Pin Mapping

### TFT Display (ILI9341V)
| Signal | GPIO |
|--------|------|
| SCK    | 12   |
| MOSI   | 11   |
| MISO   | 13   |
| CS     | 10   |
| DC/RS  | 46   |
| BL     | 45   |
| RST    | shared with CHIP_PU — not driven by firmware |

### Capacitive Touch (FT6336G)
| Signal | GPIO |
|--------|------|
| SDA    | 16 (shared with ES8311 I2C) |
| SCL    | 15 (shared with ES8311 I2C) |
| INT    | 17   |
| RST    | 18   |

### Audio (ES8311 + FM8002E)
| Signal | GPIO |
|--------|------|
| PA EN  | 1 (LOW = enabled) |
| MCLK   | 4    |
| BCLK   | 5    |
| DOUT   | 6 (to speaker) |
| WS/LR  | 7    |
| DIN    | 8 (from mic) |
| I2C SDA| 16   |
| I2C SCL| 15   |

### Other
| Component | GPIO |
|-----------|------|
| RGB LED   | 42   |
| BOOT btn  | 0    |

## Build Instructions

### Prerequisites
- ESP-IDF v5.3 or later
- Python 3 with `idf_component_manager`

### Steps

```bash
# 1. Clone the XiaoZhi repo
git clone https://github.com/78/xiaozhi-esp32.git
cd xiaozhi-esp32

# 2. Copy this board directory into the boards folder
cp -r /path/to/es3c28p main/boards/es3c28p

# 3. Add the board entry to main/Kconfig.projbuild
#    (see "Kconfig entry" section below)

# 4. Set up ESP-IDF environment
. $IDF_PATH/export.sh   # Linux/macOS
# or: %IDF_PATH%\export.bat  (Windows)

# 5. Configure — select "ES3C28P" as Board Type
idf.py menuconfig

# 6. Build
idf.py build

# 7. Flash (replace /dev/ttyUSB0 with your port)
idf.py -p /dev/ttyUSB0 flash monitor
```

Alternatively, use the release script to produce a flashable zip:
```bash
python scripts/release.py main/boards/es3c28p
```

## Kconfig Entry

Add the following block inside the `choice BOARD_TYPE` section in
`main/Kconfig.projbuild`:

```kconfig
config BOARD_TYPE_ES3C28P
    bool "ES3C28P (LCDWIKI 2.8\" ESP32-S3 Display Board)"
    help
        LCDWIKI / QDtech ES3C28P — 2.8-inch IPS TFT (ILI9341V),
        capacitive touch (FT6336G), ES8311 audio codec, single MEMS
        microphone, FM8002E speaker amplifier, WS2812 RGB LED.
```

And in `main/CMakeLists.txt`, inside the board-type conditional block, add:
```cmake
elseif(CONFIG_BOARD_TYPE_ES3C28P)
    list(APPEND BOARD_SRCS "boards/es3c28p/es3c28p_board.cc")
    set(BOARD_NAME "es3c28p")
```

## First Boot

1. Flash the firmware.
2. On first boot the board starts a Wi-Fi AP named **XiaoZhi-XXXXXX**.
3. Connect your phone/PC to that AP and enter your Wi-Fi credentials.
4. After connecting, a 6-digit device code appears on screen.
5. Register at [xiaozhi.me](https://xiaozhi.me) and add the device using that code.
6. Default wake word: **"你好小智"** (configurable).

## Known Limitations / Notes

- **Display RST** is tied to CHIP_PU (hardware reset). The firmware skips
  driving a software reset pin (`GPIO_NUM_NC`). This is intentional and safe.
- **PA active-LOW**: The FM8002E amplifier on this board enables on a LOW
  signal (GPIO1). The `Es8311AudioCodec` constructor's last boolean argument
  (`false`) sets this polarity.
- **Shared I2C bus**: GPIO15/16 serve both ES8311 and FT6336G. Both devices
  have different I2C addresses so they coexist without conflict.
- **SD card**: SDIO pins (DAT0-DAT3, CMD, CLK) are not configured in this
  board support. Adding SD support requires additional driver code.
- **OPI PSRAM pins** (GPIO26, 28, 30, 31, 32) are reserved internally by the
  ESP-IDF and must not be used by application code.

## References

- [LCDWIKI product page](https://www.lcdwiki.com/2.8inch_ESP32-S3_Display)
- [XiaoZhi custom board guide](https://github.com/78/xiaozhi-esp32/blob/main/docs/custom-board.md)
- [ngttai BSP for ES3C28P](https://github.com/ngttai/esp32_s3_es3c28p)
