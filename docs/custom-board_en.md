# Custom development board guide

This guide describes how to customize a new development board initialization program for the Xiaozhi AI voice chat robot project. Xiaozhi AI supports more than 70 ESP32 series development boards, and the initialization code of each development board is placed in the corresponding directory.

## IMPORTANT NOTE

> **Warning**: For custom development boards, when the IO configuration is different from the original development board, do not directly overwrite the configuration of the original development board to compile firmware. A new development board type must be created, or distinguished by different name and sdkconfig macro definitions through the builds configuration in the config.json file. Use `python scripts/release.py [development board directory name]` to compile and package the firmware.
>
> If you directly overwrite the original configuration, your custom firmware may be overwritten by the standard firmware of the original development board during future OTA upgrades, causing your device to not work properly. Each development board has a unique identification and corresponding firmware upgrade channel. It is very important to maintain the uniqueness of the development board identification.

## Directory structure

The directory structure of each development board usually contains the following files:

- `xxx_board.cc` - The main board-level initialization code, which implements board-related initialization and functions
- `config.h` - board-level configuration file, which defines hardware pin mapping and other configuration items
- `config.json` - Compilation configuration, specifying the target chip and special compilation options
- `README.md` - development board related documentation

## Customized development board steps

### 1. Create a new development board directory

First, create a new directory under the `boards/` directory. The naming method should be in the form of `[brand name]-[development board type]`, for example `m5stack-tab5`:

```bash
mkdir main/boards/my-custom-board
```

### 2. Create configuration file

#### config.h

Define all hardware configurations in `config.h`, including:

- Audio sample rate and I2S pin configuration
- Audio codec chip address and I2C pin configuration
- Button and LED pin configuration
- Display parameters and pin configuration

Reference example (from lichuang-c3-dev):

```c
#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

//Audio configuration
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

//Button configuration
#define BOOT_BUTTON_GPIO        GPIO_NUM_9

//Display configuration
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

Define compilation configuration in `config.json`. This file is used for `scripts/release.py` script automatic compilation:

```json
{
    "target": "esp32s3", // Target chip model: esp32, esp32s3, esp32c3, esp32c6, esp32p4, etc.
    "builds": [
        {
            "name": "my-custom-board", // Development board name, used to generate firmware packages
            "sdkconfig_append": [
                //Special Flash size configuration
                "CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y",
                //Special partition table configuration
                "CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"partitions/v2/8m.csv\""
            ]
        }
    ]
}
```

**Configuration item description:**
- `target`: target chip model, must match the hardware
- `name`: The name of the compiled firmware package, it is recommended to be consistent with the directory name
- `sdkconfig_append`: additional sdkconfig configuration item array, which will be appended to the default configuration

**Commonly used sdkconfig_append configuration:**
```json
// Flash size
"CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y"   // 4MB Flash
"CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y"   // 8MB Flash
"CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y"  // 16MB Flash

//Partition table
"CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"partitions/v2/4m.csv\"" // 4MB partition table
"CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"partitions/v2/8m.csv\"" // 8MB partition table
"CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"partitions/v2/16m.csv\"" // 16MB partition table

// language configuration
"CONFIG_LANGUAGE_EN_US=y" // English
"CONFIG_LANGUAGE_ZH_CN=y" // Simplified Chinese

// Wake word configuration
"CONFIG_USE_DEVICE_AEC=y" // Enable device-side AEC
"CONFIG_WAKE_WORD_DISABLED=y" // Disable wake words
```

### 3. Write board-level initialization code

Create a `my_custom_board.cc` file to implement all the initialization logic of the development board.

A basic development board class definition contains the following parts:

1. **Class definition**: Inherited from `WifiBoard` or `Ml307Board`
2. **Initialization function**: including initialization of I2C, display, buttons, IoT and other components
3. **Virtual function rewriting**: such as `GetAudioCodec()`, `GetDisplay()`, `GetBacklight()`, etc.
4. **Register Development Board**: Use `DECLARE_BOARD` macro to register development board

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

    //I2C initialization
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

    //Button initialization
    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState();
        });
    }

    //Display initialization (take ST7789 as an example)
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
        
        //Create display object
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
    //Constructor
    MyCustomBoard() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();
        InitializeSpi();
        InitializeDisplay();
        InitializeButtons();
        InitializeTools();
        GetBacklight()->SetBrightness(100);
    }

    // Get the audio codec
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

    // Get the display screen
    virtual Display* GetDisplay() override {
        return display_;
    }
    
    // Get backlight control
    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }
};

//Register development board
DECLARE_BOARD(MyCustomBoard);
```

### 4. Add build system configuration

#### Add development board options in Kconfig.projbuild

Open the `main/Kconfig.projbuild` file and add a new development board configuration item in the `choice BOARD_TYPE` section:

```kconfig
choice BOARD_TYPE
    prompt "Board Type"
    default BOARD_TYPE_BREAD_COMPACT_WIFI
    help
        Board type. Development board type
    
    # ...other board options ...
    
    config BOARD_TYPE_MY_CUSTOM_BOARD
        bool "My Custom Board (my custom development board)"
        depends on IDF_TARGET_ESP32S3 # Modify according to your target chip
endchoice
```

**Note:**
- `BOARD_TYPE_MY_CUSTOM_BOARD` is the name of the configuration item. It needs to be in all uppercase letters and separated by underscores.
- `depends on` specifies the target chip type (such as `IDF_TARGET_ESP32S3`, `IDF_TARGET_ESP32C3`, etc.)
- The description text can be in Chinese and English

#### Add development board configuration in CMakeLists.txt

Open the `main/CMakeLists.txt` file and add new configuration in the development board type judgment section:

```cmake
# Add your development board configuration in the elseif chain
elseif(CONFIG_BOARD_TYPE_MY_CUSTOM_BOARD)
    set(BOARD_TYPE "my-custom-board") # Same as the directory name
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4) #Choose the appropriate font according to the screen size
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64) # Optional, if emoticon display is required
endif()
```

**Font and emoticon configuration instructions:**

Choose the appropriate font size based on your screen resolution:
- Small screen (128x64 OLED): `font_puhui_basic_14_1` / `font_awesome_14_1`
- Small and medium screens (240x240): `font_puhui_basic_16_4` / `font_awesome_16_4`
- Medium screen (240x320): `font_puhui_basic_20_4` / `font_awesome_20_4`
- Large screen (480x320+): `font_puhui_basic_30_4` / `font_awesome_30_4`

Emoticon collection options:
- `twemoji_32` - 32x32 pixel emoticon (small screen)
- `twemoji_64` - 64x64 pixel emoji (large screen)

### 5. Configuration and compilation

#### Method 1: Manual configuration using idf.py

1. **Set the target chip** (when configuring or replacing the chip for the first time):
   ```bash
   # For ESP32-S3
   idf.py set-target esp32s3
   
   # For ESP32-C3
   idf.py set-target esp32c3
   
   # For ESP32
   idf.py set-target esp32
   ```

2. **Clean up old configuration**:
   ```bash
   idf.py fullclean
   ```

3. **Enter configuration menu**:
   ```bash
   idf.py menuconfig
   ```
   
   Navigate to: `Xiaozhi Assistant` -> `Board Type` in the menu and select your custom development board.

4. **Compile and Burn**:
   ```bash
   idf.py build
   idf.py flash monitor
   ```

#### Method 2: Use the release.py script (recommended)

If you have a `config.json` file in your development board directory, you can use this script to automatically complete configuration and compilation:

```bash
python scripts/release.py my-custom-board
```

This script automatically:
- Read the `target` configuration in `config.json` and set the target chip
- Apply compilation options in `sdkconfig_append`
- Compile and package firmware

### 6. Create README.md

The characteristics, hardware requirements, compilation and burning steps of the development board are described in README.md:


## Common development board components

### 1. Display

The project supports a variety of display drivers, including:
- ST7789 (SPI)
- ILI9341 (SPI)
- SH8601 (QSPI)
- wait...

### 2. Audio codec

Supported codecs include:
- ES8311 (commonly used)
- ES7210 (microphone array)
- AW88298 (power amplifier)
- wait...

### 3. Power management

Some development boards use power management chips:
- AXP2101
- Other PMICs available

### 4. MCP device control

Various MCP tools can be added to enable the AI ​​to use:
- Speaker (speaker control)
- Screen (screen brightness adjustment)
- Battery (battery level reading)
- Light (light control)
- wait...

## Development board class inheritance relationship

- `Board` - basic board level class
  - `WifiBoard` - Wi-Fi connected development board
  - `Ml307Board` - Development board using 4G module
  - `DualNetworkBoard` - Development board that supports Wi-Fi and 4G network switching

## Development skills

1. **Refer to similar development boards**: If your new development board is similar to the existing development board, you can refer to the existing implementation
2. **Step-by-step debugging**: First implement basic functions (such as display), and then add more complex functions (such as audio)
3. **Pin Mapping**: Make sure all pin mappings are configured correctly in config.h
4. **Check Hardware Compatibility**: Confirm compatibility of all chips and drivers

## Possible problems

1. **Display is abnormal**: Check SPI configuration, mirroring settings and color inversion settings
2. **No audio output**: Check I2S configuration, PA enable pin and codec address
3. **Unable to connect to network**: Check Wi-Fi credentials and network configuration
4. **Unable to communicate with server**: Check MQTT or WebSocket configuration

## References

- ESP-IDF documentation: https://docs.espressif.com/projects/esp-idf/
- LVGL documentation: https://docs.lvgl.io/
- ESP-SR documentation: https://github.com/espressif/esp-sr