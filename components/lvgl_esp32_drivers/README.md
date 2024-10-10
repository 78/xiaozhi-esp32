# Display and touchpad drivers for ESP32 using LVGL

For a ready to use ESP32 project take look at the [lv_port_esp32](https://github.com/lvgl/lv_port_esp32) repository.

#### Table of content
- [Supported display controllers](#supported-display-controllers)
- [Supported indev controllers](#supported-indev-controllers)
- [Support for predefined development kits](#support-for-predefined-development-kits)
- [Thread-safe I2C with I2C Manager](#thread-safe-i2c-with-i2c-manager)

**NOTE:** You need to set the display horizontal and vertical size, color depth and
swap of RGB565 color on the LVGL configuration menuconfig (it's not handled automatically).


## Supported display controllers


| Display Controller                          | Type       | Interface              | Color depth (LV_COLOR_DEPTH) | Swap RGB565 color (LV_COLOR_16_SWAP)   |
|---------------------------------------------|------------|------------------------|------------------------------|----------------------------------------|
| ILI9341                                     | TFT        | SPI                    | 16: RGB565                   | Yes                                    |
| ILI9163C                                    | TFT        | SPI                    | 16: RGB565                   | Yes                                    |
| ILI9486                                     | TFT        | SPI                    | 16: RGB565                   | Yes                                    |
| ILI9488                                     | TFT        | SPI                    | 16: RGB565                   | No                                     |
| HX8357B/HX8357D                             | TFT        | SPI                    | 16: RGB565                   | Yes                                    |
| ST7789                                      | TFT        | SPI                    | 16: RGB565                   | Yes                                    |
| ST7735S                                     | TFT        | SPI                    | 16: RGB565                   | Yes                                    |
| FT81x                                       | TFT        | Single, Dual, Quad SPI | 16: RGB565                   | No                                     |
| GC9A01                                      | TFT        | SPI                    | 16: RGB565                   | Yes                                    |
| RA8875                                      | TFT        | SPI                    | 16: RGB565                   | Yes                                    |
| SH1107                                      | Monochrome | SPI                    | 1: 1byte per pixel           | No                                     |
| SSD1306                                     | Monochrome | I2C                    | 1: 1byte per pixel           | No                                     |
| IL3820                                      | e-Paper    | SPI                    | 1: 1byte per pixel           | No                                     |
| UC8151D/ GoodDisplay GDEW0154M10 DES        | e-Paper    | SPI                    | 1: 1byte per pixel           | No                                     |
| FitiPower JD79653A/ GoodDisplay GDEW0154M09 | e-Paper    | SPI                    | 1: 1byte per pixel           | No                                     |

## Supported indev controllers

- XPT2046
- FT3236, FT6X36
- FT6206 controllers should work as well (not tested)
- STMPE610
- FT81x (Single, Dual, and Quad SPI)

If your display or input device (touch) controller is not supported consider contributing to this repo by
adding support to it! [Contribute controller support](CONTRIBUTE_CONTROLLER_SUPPORT.md)

## Support for predefined development kits

You can also use the predefined kits, which selects the correct display controllers on the kit,
and sets the gpio numbers for the interface.

| Kit name                  | Display controller    | Interface | Hor. Res. | Ver. Res. |
|---------------------------|-----------------------|-----------|-----------|-----------|
| ESP Wrover Kit v4.1       | ILI9341               | SPI       | 240       | 320       |
| M5Stack                   | ILI9341               | SPI       | 240       | 320       |
| M5Stack Core2             | ILI9341               | SPI       | 240       | 320       |
| M5Stick                   | SH1107                | SPI       | -         | -         |
| M5StickC                  | ST7735S               | SPI       | 80        | 160       |
| Adafruit 3.5 Featherwing  | HX8357                | SPI       | 480       | 320       |
| RPi MPI3501               | ILI9486               | SPI       | -         | -         |
| Wemos Lolin OLED          | SSD1306               | SPI       | 64        | 128       |
| ER-TFT035-6               | ILI9488               | SPI       | 480       | 320       |
| AIRcable ATAGv3           | IL3820                | SPI       | 128       | 296       |
| TTGO T-Display            | ST7789                | SPI       | 135       | 240       |
| TTGO Camera Plus          | ST7789                | SPI       | 240       | 240       |

**NOTE:** See [Supported display controllers](#supported-display-controllers) for more information on display configuration.
**NOTE:** See [Supported indev controllers](#supported-indev-controllers) for more information about indev configuration.


## Thread-safe I2C with I2C Manager

LVGL can use I2C to read from a touch sensor or write to a display, possibly
many times a second. Meanwhile, other tasks may also want to read from i2c
devices on the same bus. I2C using the ESP-IDF is not thread-safe.

I2C Manager (`i2c_manager`) is a component that will let code in multiple threads
talk to devices on the I2C ports without getting in each other's way. These drivers
use a built-in copy of I2C Manager to talk to the I2C port, but you can also use 
the I2C Manager component itself and have others play nice with LVGL and vice-versa.
[Click here](i2c_manager/README.md) for details.
