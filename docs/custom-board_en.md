# Custom Board Guide (English)

This guide explains how to create a custom board initialization for the XiaoZhi AI Voice Chatbot project. XiaoZhi AI supports over 70 ESP32-series boards, with each board's initialization code in its own directory.

## Important Notice

> **Warning**: For custom boards, when the IO configuration differs from existing boards, NEVER directly overwrite the original board configuration to compile firmware. You must create a new board type, or distinguish different variants through the `name` and sdkconfig macros in the `config.json` file's builds configuration. Use `python scripts/release.py [board-directory-name]` to compile and package firmware.
>
> If you directly overwrite existing configurations, future OTA upgrades may replace your custom firmware with the standard firmware for the original board, causing your device to malfunction. Each board has a unique identifier and corresponding firmware upgrade channel. Maintaining board identifier uniqueness is critical.

## Directory Structure

Each board directory typically contains:

- `xxx_board.cc` - Main board initialization code implementing board-specific initialization and features
- `config.h` - Board configuration file defining hardware pin mappings and other settings
- `config.json` - Build configuration specifying target chip and special compilation options
- `README.md` - Board-related documentation

## Custom Board Steps

### 1. Create New Board Directory

First, create a new directory under `boards/` using the naming convention `[brand-name]-[board-type]`, e.g., `m5stack-tab5`:

```bash
mkdir main/boards/my-custom-board
```

### 2. Create Configuration Files

#### config.h

Define all hardware configurations in `config.h`, including:

- Audio sample rates and I2S pin configuration
- Audio codec chip address and I2C pin configuration
- Button and LED pin configuration
- Display parameters and pin configuration

Example reference (from lichuang-c3-dev):

```c
#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

// Audio configuration
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

// Button configuration
#define BOOT_BUTTON_GPIO        GPIO_NUM_9

// Display configuration
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

Define build configuration in `config.json`. This file is used by the `scripts/release.py` script for automated compilation:

```json
{
    "target": "esp32s3",  // Target chip model: esp32, esp32s3, esp32c3, esp32c6, esp32p4, etc.
    "builds": [
        {
            "name": "my-custom-board",  // Board name for generating firmware package
            "sdkconfig_append": [
                // Special Flash size configuration
                "CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y",
                // Special partition table configuration
                "CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"partitions/v2/8m.csv\""
            ]
        }
    ]
}
```

**Configuration options:**
- `target`: Target chip model, must match hardware
- `name`: Firmware package output name, recommended to match directory name
- `sdkconfig_append`: Array of additional sdkconfig options, appended to default configuration

**Common sdkconfig_append configurations:**
```json
// Flash size
"CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y"   // 4MB Flash
"CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y"   // 8MB Flash
"CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y"  // 16MB Flash

// Partition tables
"CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"partitions/v2/4m.csv\""  // 4MB partition
"CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"partitions/v2/8m.csv\""  // 8MB partition
"CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"partitions/v2/16m.csv\"" // 16MB partition

// Language configuration
"CONFIG_LANGUAGE_EN_US=y"  // English
"CONFIG_LANGUAGE_ZH_CN=y"  // Simplified Chinese

// Wake word configuration
"CONFIG_USE_DEVICE_AEC=y"          // Enable device-side AEC
"CONFIG_WAKE_WORD_DISABLED=y"      // Disable wake word
```

### 3. Write Board Initialization Code

Create a `my_custom_board.cc` file implementing all board initialization logic.

A basic board class definition includes:

1. **Class definition**: Inherit from `WifiBoard` or `Ml307Board`
2. **Initialization functions**: Including I2C, display, buttons, IoT components
3. **Virtual function overrides**: Such as `GetAudioCodec()`, `GetDisplay()`, `GetBacklight()`, etc.
4. **Board registration**: Register board using `DECLARE_BOARD` macro

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

    // I2C initialization
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

    // SPI initialization (for display)
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

    // Button initialization
    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState();
        });
    }

    // Display initialization (ST7789 example)
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
        
        // Create display object
        display_ = new SpiLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, 
                                    DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, 
                                    DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    // MCP Tools initialization
    void InitializeTools() {
        // Refer to MCP documentation
    }

public:
    // Constructor
    MyCustomBoard() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();
        InitializeSpi();
        InitializeDisplay();
        InitializeButtons();
        InitializeTools();
        GetBacklight()->SetBrightness(100);
    }

    // Get audio codec
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

    // Get display
    virtual Display* GetDisplay() override {
        return display_;
    }
    
    // Get backlight control
    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }
};

// Register board
DECLARE_BOARD(MyCustomBoard);
```

### 4. Add Build System Configuration

#### Add Board Option in Kconfig.projbuild

Open the `main/Kconfig.projbuild` file and add a new board configuration in the `choice BOARD_TYPE` section:

```kconfig
choice BOARD_TYPE
    prompt "Board Type"
    default BOARD_TYPE_BREAD_COMPACT_WIFI
    help
        Board type
    
    # ... other board options ...
    
    config BOARD_TYPE_MY_CUSTOM_BOARD
        bool "My Custom Board"
        depends on IDF_TARGET_ESP32S3  # Modify based on your target chip
endchoice
```

**Notes:**
- `BOARD_TYPE_MY_CUSTOM_BOARD` is the configuration option name, must be uppercase with underscores
- `depends on` specifies the target chip type (e.g., `IDF_TARGET_ESP32S3`, `IDF_TARGET_ESP32C3`, etc.)
- Description text can be in English or Chinese

#### Add Board Configuration in CMakeLists.txt

Open the `main/CMakeLists.txt` file and add your configuration in the board type judgment section:

```cmake
# Add your board configuration in the elseif chain
elseif(CONFIG_BOARD_TYPE_MY_CUSTOM_BOARD)
    set(BOARD_TYPE "my-custom-board")  # Must match directory name
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)  # Choose appropriate font for screen size
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)  # Optional, if emoji display is needed
endif()
```

**Font and emoji configuration guide:**

Choose appropriate font size based on screen resolution:
- Small screens (128x64 OLED): `font_puhui_basic_14_1` / `font_awesome_14_1`
- Medium-small screens (240x240): `font_puhui_basic_16_4` / `font_awesome_16_4`
- Medium screens (240x320): `font_puhui_basic_20_4` / `font_awesome_20_4`
- Large screens (480x320+): `font_puhui_basic_30_4` / `font_awesome_30_4`

Emoji collection options:
- `twemoji_32` - 32x32 pixel emojis (small screens)
- `twemoji_64` - 64x64 pixel emojis (large screens)

### 5. Configuration and Compilation

#### Method 1: Manual Configuration with idf.py

1. **Set target chip** (first time configuration or when changing chips):
   ```bash
   # For ESP32-S3
   idf.py set-target esp32s3
   
   # For ESP32-C3
   idf.py set-target esp32c3
   
   # For ESP32
   idf.py set-target esp32
   ```

2. **Clean old configuration**:
   ```bash
   idf.py fullclean
   ```

3. **Enter configuration menu**:
   ```bash
   idf.py menuconfig
   ```
   
   Navigate to: `Xiaozhi Assistant` → `Board Type`, and select your custom board.

4. **Build and flash**:
   ```bash
   idf.py build
   idf.py flash monitor
   ```

#### Method 2: Using release.py Script (Recommended)

If your board directory has a `config.json` file, you can use this script to automatically complete configuration and compilation:

```bash
python scripts/release.py my-custom-board
```

This script will automatically:
- Read the `target` configuration from `config.json` and set the target chip
- Apply compilation options from `sdkconfig_append`
- Complete compilation and package firmware

### 6. Create README.md

In README.md, explain the board's features, hardware requirements, compilation and flashing steps.

## Common Board Components

### 1. Display Screens

The project supports multiple display drivers, including:
- ST7789 (SPI)
- ILI9341 (SPI)
- SH8601 (QSPI)
- etc.

### 2. Audio Codecs

Supported codecs include:
- ES8311 (commonly used)
- ES7210 (microphone array)
- AW88298 (power amplifier)
- etc.

### 3. Power Management

Some boards use power management chips:
- AXP2101
- Other available PMICs

### 4. MCP Device Control

You can add various MCP tools for AI to use:
- Speaker (speaker control)
- Screen (screen brightness adjustment)
- Battery (battery level reading)
- Light (light control)
- etc.

## Board Class Inheritance

- `Board` - Base board class
  - `WifiBoard` - Wi-Fi connected boards
  - `Ml307Board` - Boards using 4G modules
  - `DualNetworkBoard` - Boards supporting Wi-Fi and 4G network switching

## Development Tips

1. **Reference similar boards**: If your new board is similar to existing boards, reference existing implementations
2. **Step-by-step debugging**: First implement basic features (like display), then add more complex features (like audio)
3. **Pin mapping**: Ensure all pin mappings are correctly configured in config.h
4. **Check hardware compatibility**: Verify compatibility of all chips and drivers

## Common Issues

1. **Display not working properly**: Check SPI configuration, mirroring settings, and color inversion settings
2. **No audio output**: Check I2S configuration, PA enable pin, and codec address
3. **Cannot connect to network**: Check Wi-Fi credentials and network configuration
4. **Cannot communicate with server**: Check MQTT or WebSocket configuration

## References

- ESP-IDF Documentation: https://docs.espressif.com/projects/esp-idf/
- LVGL Documentation: https://docs.lvgl.io/
- ESP-SR Documentation: https://github.com/espressif/esp-sr 