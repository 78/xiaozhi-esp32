# Custom Board Guide

This guide describes how to add a new board to the XiaoZhi AI voice assistant project. XiaoZhi AI supports 70+ ESP32-series boards; each one lives in its own directory under `main/boards/`.

## Important

> **Warning**: for a custom board whose IO configuration differs from an existing board, never overwrite the original board's configuration. Always create a new board type - or use the `builds` array in `config.json` to produce a distinct firmware name with different `sdkconfig` macros. Use `python scripts/release.py [board-directory]` to build and package the firmware.
>
> Overwriting an existing board's configuration is dangerous because OTA updates may replace your custom firmware with the stock firmware for the original board. Every board must have a unique identity and its own firmware update channel.

## Directory Layout

A board directory typically contains:

- `xxx_board.cc` - board-level initialization and glue code.
- `config.h` - pin assignments and board-level settings.
- `config.json` - build configuration consumed by `scripts/release.py`.
- `README.md` - board-specific notes.

Boards can live directly under `main/boards/` or be grouped by manufacturer under `main/boards/<manufacturer>/<board>/` (see [Manufacturer Sub-directories](#manufacturer-sub-directories) below).

## Steps

### 1. Create the Board Directory

Create a new directory under `main/boards/` using the `[vendor]-[model]` naming style (e.g. `m5stack-tab5`):

```bash
mkdir main/boards/my-custom-board
```

### 2. Create the Configuration Files

#### config.h

Define all hardware settings in `config.h`:

- Audio sample rates and I2S pin mapping.
- Audio codec I2C address and pins.
- Button and LED pins.
- Display parameters and pins.

Example (from `lichuang-c3-dev`):

```c
#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

// Audio
#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_10
#define AUDIO_I2S_GPIO_WS   GPIO_NUM_12
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_8
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_7
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_11

#define AUDIO_CODEC_PA_PIN       GPIO_NUM_13
#define AUDIO_CODEC_I2C_SDA_PIN  GPIO_NUM_0
#define AUDIO_CODEC_I2C_SCL_PIN  GPIO_NUM_1
#define AUDIO_CODEC_ES8311_ADDR  ES8311_CODEC_DEFAULT_ADDR

// Buttons
#define BOOT_BUTTON_GPIO        GPIO_NUM_9

// Display
#define DISPLAY_SPI_SCK_PIN     GPIO_NUM_3
#define DISPLAY_SPI_MOSI_PIN    GPIO_NUM_5
#define DISPLAY_DC_PIN          GPIO_NUM_6
#define DISPLAY_SPI_CS_PIN      GPIO_NUM_4

#define DISPLAY_WIDTH   320
#define DISPLAY_HEIGHT  240
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY true

#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0

#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_2
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT true

#endif // _BOARD_CONFIG_H_
```

#### config.json

`config.json` drives `scripts/release.py`:

```json
{
    "target": "esp32s3",
    "builds": [
        {
            "name": "my-custom-board",
            "sdkconfig_append": [
                "CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y",
                "CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"partitions/v2/8m.csv\""
            ]
        }
    ]
}
```

**Fields**:
- `target`: target chip, must match the real hardware (`esp32`, `esp32s3`, `esp32c3`, `esp32c6`, `esp32p4`, ...).
- `name`: firmware package name; typically matches the directory name.
- `sdkconfig_append`: extra sdkconfig lines merged into the defaults.

**Common `sdkconfig_append` entries**:

```json
// Flash size
"CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y"
"CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y"
"CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y"

// Partition table
"CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"partitions/v2/4m.csv\""
"CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"partitions/v2/8m.csv\""
"CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"partitions/v2/16m.csv\""

// Language
"CONFIG_LANGUAGE_EN_US=y"
"CONFIG_LANGUAGE_ZH_CN=y"

// Wake word configuration
"CONFIG_USE_DEVICE_AEC=y"          // enable on-device AEC
"CONFIG_WAKE_WORD_DISABLED=y"      // disable wake word detection
```

### 3. Implement the Board Class

Create `my_custom_board.cc` containing the board-level implementation.

A basic board class has:

1. **Class declaration**: derive from `WifiBoard` or `Ml307Board`.
2. **Initialization helpers**: I2C, display, buttons, IoT/MCP tools, etc.
3. **Virtual overrides**: `GetAudioCodec()`, `GetDisplay()`, `GetBacklight()`, ...
4. **Board registration**: `DECLARE_BOARD(ClassName)`.

```cpp
#include "wifi_board.h"
#include "codecs/es8311_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>

#define TAG "MyCustomBoard"

class MyCustomBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;
    Button boot_button_;
    LcdDisplay* display_;

    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_SPI_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_SPI_SCK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
    }

    void InitializeDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_SPI_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = 2;
        io_config.pclk_hz = 80 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &panel_io));

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));

        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, true);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);

        display_ = new SpiLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                    DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
                                    DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeTools() {
        // Register MCP tools here; see docs/mcp-usage.md.
    }

public:
    MyCustomBoard() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();
        InitializeSpi();
        InitializeDisplay();
        InitializeButtons();
        InitializeTools();
        GetBacklight()->SetBrightness(100);
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(
            codec_i2c_bus_,
            I2C_NUM_0,
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK,
            AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN,
            AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }
};

DECLARE_BOARD(MyCustomBoard);
```

### 4. Hook Up the Build System

#### Add a Kconfig entry

In `main/Kconfig.projbuild`, add an entry to the `choice BOARD_TYPE` block:

```kconfig
choice BOARD_TYPE
    prompt "Board Type"
    default BOARD_TYPE_BREAD_COMPACT_WIFI
    help
        Board type.

    # ... other entries ...

    config BOARD_TYPE_MY_CUSTOM_BOARD
        bool "My Custom Board"
        depends on IDF_TARGET_ESP32S3  # pick the matching target
endchoice
```

Notes:
- The identifier must be uppercase and underscore-separated.
- `depends on` restricts the entry to the correct target (`IDF_TARGET_ESP32S3`, `IDF_TARGET_ESP32C3`, ...).
- The label can be localized.

#### Add a branch in CMakeLists.txt

Open `main/CMakeLists.txt` and extend the board-type chain:

```cmake
elseif(CONFIG_BOARD_TYPE_MY_CUSTOM_BOARD)
    set(BOARD_TYPE "my-custom-board")                # must match the directory name
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)     # pick a font for the display
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)         // optional, for emoji display
```

**Font and emoji guidance**:

Pick a font size that matches the display resolution:
- Small (128x64 OLED): `font_puhui_basic_14_1` / `font_awesome_14_1`
- Small-medium (240x240): `font_puhui_basic_16_4` / `font_awesome_16_4`
- Medium (240x320): `font_puhui_basic_20_4` / `font_awesome_20_4`
- Large (480x320+): `font_puhui_basic_30_4` / `font_awesome_30_4`

Emoji collections:
- `twemoji_32` - 32x32 pixels (small screens).
- `twemoji_64` - 64x64 pixels (large screens).

### 5. Build and Flash

#### Option A - use `idf.py` manually

1. Set the target chip (first time, or when switching targets):
   ```bash
   idf.py set-target esp32s3     # ESP32-S3
   idf.py set-target esp32c3     # ESP32-C3
   idf.py set-target esp32       # ESP32
   ```

2. Clean stale configuration:
   ```bash
   idf.py fullclean
   ```

3. Select the board via menuconfig:
   ```bash
   idf.py menuconfig
   ```
   Navigate to `Xiaozhi Assistant -> Board Type` and choose your board.

4. Build and flash:
   ```bash
   idf.py build
   idf.py flash monitor
   ```

#### Option B - use `release.py` (recommended)

If the board directory contains a `config.json`, you can build and package automatically:

```bash
python scripts/release.py my-custom-board
```

The script:
- Reads `target` from `config.json` and calls `idf.py set-target`.
- Appends the entries listed in `sdkconfig_append`.
- Builds and packages the firmware.

### 6. Write the README

In `README.md`, describe the board, hardware requirements, build instructions, and any special notes.

## Manufacturer Sub-directories

Boards can be grouped by manufacturer under `main/boards/<manufacturer>/<board>/`. This is the recommended layout when a single vendor ships several variants - for example `main/boards/waveshare/esp32-p4-nano/` or `main/boards/lceda-course-examples/eda-tv-pro/`.

To enable the layout, set the `MANUFACTURER` variable in `main/CMakeLists.txt` for your board:

```cmake
elseif(CONFIG_BOARD_TYPE_WAVESHARE_ESP32_P4_NANO)
    set(MANUFACTURER "waveshare")
    set(BOARD_TYPE "esp32-p4-nano")
    set(BUILTIN_TEXT_FONT font_puhui_basic_30_4)
    set(BUILTIN_ICON_FONT font_awesome_30_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
```

When `MANUFACTURER` is set, the build system globs source files from `main/boards/${MANUFACTURER}/${BOARD_TYPE}/`. When it is empty, it falls back to the flat `main/boards/${BOARD_TYPE}/` layout.

Rules of thumb:
- Use the manufacturer layout when you have two or more boards from the same vendor that share drivers, assets, or documentation.
- Use the flat layout for one-off boards and community examples.
- Directory names use lowercase with dashes (e.g. `waveshare`, `lceda-course-examples`).

## Common Board Components

Several reusable components live in `main/boards/common/`. You can include them directly from your board class:

### Display drivers

Supported LCD families include:
- ST7789 (SPI)
- ILI9341 (SPI)
- SH8601 (QSPI)
- and many more.

### Audio codecs

- `Es8311AudioCodec` (most common)
- `Es8374AudioCodec`
- `Es8388AudioCodec`
- `Es8389AudioCodec`
- `BoxAudioCodec` (ES7210 mic array + codec combo used on ESP-Box boards)
- `NoAudioCodec` (direct I2S without external codec)
- `DummyAudioCodec` (placeholder for boards without audio)

### Power management

- `Axp2101` power management IC helpers.
- `Sy6970` battery charger helpers.
- `AdcBatteryMonitor` - simple ADC-based battery voltage monitor.
- `PowerSaveTimer` / `SleepTimer` - helpers for light-sleep scheduling.

### Networking

- `WifiBoard` - WiFi-only base class.
- `Ml307Board` / `Nt26Board` - 4G modem base classes.
- `DualNetworkBoard` - switchable WiFi / 4G base class.
- `RndisBoard` - RNDIS-over-USB networking (ESP32-S3 / ESP32-P4).
- `EspVideo` helpers for ESP-Video on ESP32-S3 / ESP32-P4.

### Input helpers

- `Button` - standard push buttons (click, long-press, multi-click).
- `Knob` - rotary encoder wrapper.
- `PressToTalkMcpTool` - push-to-talk tool that registers itself through MCP.
- `AfskDemod` - AFSK demodulator used by some acoustic provisioning flows.
- `SystemReset` - helper that performs a safe factory reset when a button is held at boot.

### MCP integration

Any board can register custom tools - speaker control, screen brightness, battery readout, light control, etc. See [MCP IoT control usage](./mcp-usage.md).

## Board Class Hierarchy

- `Board` - base class
  - `WifiBoard` - WiFi-connected board
  - `Ml307Board` / `Nt26Board` - 4G modem boards
  - `DualNetworkBoard` - WiFi + 4G switchable board
  - `RndisBoard` - RNDIS-over-USB board

## Tips

1. **Start from a similar board** - copying and tweaking an existing board is usually faster than starting from scratch.
2. **Bring up incrementally** - get the display up first, then audio, then the full stack.
3. **Double check pin assignments** - every pin defined in `config.h` must match your schematic.
4. **Check hardware compatibility** - especially codec / PMIC / touch controller combinations.

## Troubleshooting

1. **Display looks wrong** - verify SPI configuration, mirroring, and color inversion.
2. **No audio** - check I2S wiring, PA enable pin, and codec I2C address.
3. **Cannot connect to WiFi** - re-check WiFi credentials and provisioning method.
4. **Cannot reach the server** - verify the WebSocket / MQTT endpoint configuration.

## References

- ESP-IDF documentation: https://docs.espressif.com/projects/esp-idf/
- LVGL documentation: https://docs.lvgl.io/
- ESP-SR documentation: https://github.com/espressif/esp-sr
