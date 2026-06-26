# ESP32-C3 Custom Board

This board configuration is for an ESP32-C3 Super Mini with an SSD1306 OLED display and separate I2S audio devices:
- MAX98357A speaker
- INMP441 microphone

## Supported features

- SSD1306 OLED via I2C
- I2S audio output to MAX98357A
- I2S microphone input from INMP441
- Separate simplex I2S audio pins for speaker and mic

## Wiring

### OLED display
- `SDA` -> `GPIO8`
- `SCL` -> `GPIO9`
- `VCC` -> `3.3V`
- `GND` -> `GND`
- OLED I2C address: `0x3C`

### MAX98357A speaker
- `BCLK` -> `GPIO7`
- `LRCK` -> `GPIO5`
- `DOUT` -> `GPIO6`
- `DIN` not used
- `GND` -> `GND`
- `VCC` -> `3.3V`

### INMP441 microphone
- `SCK` -> `GPIO3`
- `WS` -> `GPIO2`
- `DIN` -> `GPIO4`
- `VCC` -> `3.3V`
- `GND` -> `GND`

## Important pin configuration

The custom board uses `main/boards/esp32-c3-custom/config.h` to define the pins:

```c
#define DISPLAY_SDA_PIN GPIO_NUM_8
#define DISPLAY_SCL_PIN GPIO_NUM_9

#define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_2
#define AUDIO_I2S_MIC_GPIO_SCK  GPIO_NUM_3
#define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_4

#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_6
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_7
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_5
```

## Build instructions

1. Set the ESP32 target:

```bash
idf.py set-target esp32c3
```

2. Configure the project if needed:

```bash
idf.py menuconfig
```

3. Build the project:

```bash
idf.py build
```

If you have issues building, verify the `main/CMakeLists.txt` custom asset generation command uses the correct repository root paths and that `generated_assets.bin` is produced in `build/`.

## Flashing

Flash normally with:

```bash
idf.py flash
```

If you need to write a full image manually, include the generated assets file:

```bash
esptool.py --chip esp32c3 write_flash -z 0x1000 build/bootloader/bootloader.bin \
    0x10000 build/xiaozhi.bin \
    0x600000 build/generated_assets.bin
```

> Note: `generated_assets.bin` is required when the build is configured to embed assets in flash.

## Known problems and fixes

### 1. Incorrect I2C pins

The board initially used `GPIO13`/`GPIO15` for OLED I2C, which is not valid on this board and causes I2C initialization failure.

Fix: update `main/boards/esp32-c3-custom/config.h` to use:

```c
#define DISPLAY_SDA_PIN GPIO_NUM_8
#define DISPLAY_SCL_PIN GPIO_NUM_9
```

### 2. Build error from missing `generated_assets.bin`

If the flash or merge script fails because `generated_assets.bin` is missing, check `main/CMakeLists.txt` and the custom asset generation command. It must use the correct project root path so the build can find `scripts/` and create the file.

### 3. ESP-IDF pin conflicts

Some GPIO pins on ESP32-C3 are reserved or have special functions. Always choose pins that are usable for I2C and I2S.

### 4. Display initialization failures

If the SSD1306 driver fails to initialize:
- verify the display address is `0x3C`
- confirm the OLED has power and correct pull-ups
- confirm `DISPLAY_WIDTH` and `DISPLAY_HEIGHT` match the OLED module

## Implementation details

The custom board class is implemented in `main/boards/esp32-c3-custom/esp32_c3_custom_board.cc`.

- `InitializeDisplayI2c()` creates an I2C master bus using `DISPLAY_SDA_PIN` and `DISPLAY_SCL_PIN`.
- `InitializeSsd1306Display()` installs the SSD1306 panel driver and powers on the display.
- `GetAudioCodec()` returns a simplex audio codec configured with separate speaker and microphone I2S pins.

## What worked

- Firmware built and flashed successfully for ESP32-C3.
- OLED display works on `GPIO8`/`GPIO9`.
- Speaker output works with MAX98357A on `GPIO7`/`GPIO5`/`GPIO6`.

## Tips for future debugging

- Use `idf.py monitor` to inspect boot logs and driver initialization output.
- If the device resets, look for `ESP_ERROR_CHECK` failures on I2C or I2S setup.
- Confirm the board uses `esp32c3` target and not another ESP32 family member.

## Troubleshooting checklist

1. Confirm `idf.py set-target esp32c3`.
2. Confirm `main/boards/esp32-c3-custom/config.h` pin mapping.
3. Confirm `main/boards/esp32-c3-custom/esp32_c3_custom_board.cc` uses those constants.
4. Rebuild with `idf.py build`.
5. Flash with `idf.py flash`.
6. Monitor output with `idf.py monitor`.

---

This README is intended to help anyone rebuild the ESP32-C3 custom board firmware and resolve the common I2C / I2S wiring issues encountered during development.