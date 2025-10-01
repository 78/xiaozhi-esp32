# 自定义开发板指南

本指南介绍如何为小智AI语音聊天机器人项目定制一个新的开发板初始化程序。小智AI支持70多种ESP32系列开发板，每个开发板的初始化代码都放在对应的目录下。

## 重要提示

> **警告**: 对于自定义开发板，当IO配置与原有开发板不同时，切勿直接覆盖原有开发板的配置编译固件。必须创建新的开发板类型，或者通过config.json文件中的builds配置不同的name和sdkconfig宏定义来区分。使用 `python scripts/release.py [开发板目录名字]` 来编译打包固件。
>
> 如果直接覆盖原有配置，将来OTA升级时，您的自定义固件可能会被原有开发板的标准固件覆盖，导致您的设备无法正常工作。每个开发板有唯一的标识和对应的固件升级通道，保持开发板标识的唯一性非常重要。

## 目录结构

每个开发板的目录结构通常包含以下文件：

- `xxx_board.cc` - 主要的板级初始化代码，实现了板子相关的初始化和功能
- `config.h` - 板级配置文件，定义了硬件管脚映射和其他配置项
- `config.json` - 编译配置，指定目标芯片和特殊的编译选项
- `README.md` - 开发板相关的说明文档

## 定制开发板步骤

### 1. 创建新的开发板目录

首先在`boards/`目录下创建一个新的目录，命名方式应使用 `[品牌名]-[开发板类型]` 的形式，例如 `m5stack-tab5`：

```bash
mkdir main/boards/my-custom-board
```

### 2. 创建配置文件

#### config.h

在`config.h`中定义所有的硬件配置，包括:

- 音频采样率和I2S引脚配置
- 音频编解码芯片地址和I2C引脚配置
- 按钮和LED引脚配置
- 显示屏参数和引脚配置

参考示例（来自lichuang-c3-dev）：

```c
#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

// 音频配置
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

// 按钮配置
#define BOOT_BUTTON_GPIO        GPIO_NUM_9

// 显示屏配置
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

在`config.json`中定义编译配置，这个文件用于 `scripts/release.py` 脚本自动化编译：

```json
{
    "target": "esp32s3",  // 目标芯片型号: esp32, esp32s3, esp32c3, esp32c6, esp32p4等
    "builds": [
        {
            "name": "my-custom-board",  // 开发板名称，用于生成固件包
            "sdkconfig_append": [
                // 特别 Flash 大小配置
                "CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y",
                // 特别分区表配置
                "CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"partitions/v2/8m.csv\""
            ]
        }
    ]
}
```

**配置项说明：**
- `target`: 目标芯片型号，必须与硬件匹配
- `name`: 编译输出的固件包名称，建议与目录名一致
- `sdkconfig_append`: 额外的 sdkconfig 配置项数组，会追加到默认配置中

**常用的 sdkconfig_append 配置：**
```json
// Flash 大小
"CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y"   // 4MB Flash
"CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y"   // 8MB Flash
"CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y"  // 16MB Flash

// 分区表
"CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"partitions/v2/4m.csv\""  // 4MB 分区表
"CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"partitions/v2/8m.csv\""  // 8MB 分区表
"CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"partitions/v2/16m.csv\"" // 16MB 分区表

// 语言配置
"CONFIG_LANGUAGE_EN_US=y"  // 英语
"CONFIG_LANGUAGE_ZH_CN=y"  // 简体中文

// 唤醒词配置
"CONFIG_USE_DEVICE_AEC=y"          // 启用设备端 AEC
"CONFIG_WAKE_WORD_DISABLED=y"      // 禁用唤醒词
```

### 3. 编写板级初始化代码

创建一个`my_custom_board.cc`文件，实现开发板的所有初始化逻辑。

一个基本的开发板类定义包含以下几个部分：

1. **类定义**：继承自`WifiBoard`或`Ml307Board`
2. **初始化函数**：包括I2C、显示屏、按钮、IoT等组件的初始化
3. **虚函数重写**：如`GetAudioCodec()`、`GetDisplay()`、`GetBacklight()`等
4. **注册开发板**：使用`DECLARE_BOARD`宏注册开发板

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

    // I2C初始化
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

    // SPI初始化（用于显示屏）
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

    // 按钮初始化
    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState();
        });
    }

    // 显示屏初始化（以ST7789为例）
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
        
        // 创建显示屏对象
        display_ = new SpiLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, 
                                    DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, 
                                    DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    // MCP Tools 初始化
    void InitializeTools() {
        // 参考 MCP 文档
    }

public:
    // 构造函数
    MyCustomBoard() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();
        InitializeSpi();
        InitializeDisplay();
        InitializeButtons();
        InitializeTools();
        GetBacklight()->SetBrightness(100);
    }

    // 获取音频编解码器
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

    // 获取显示屏
    virtual Display* GetDisplay() override {
        return display_;
    }
    
    // 获取背光控制
    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }
};

// 注册开发板
DECLARE_BOARD(MyCustomBoard);
```

### 4. 添加构建系统配置

#### 在 Kconfig.projbuild 中添加开发板选项

打开 `main/Kconfig.projbuild` 文件，在 `choice BOARD_TYPE` 部分添加新的开发板配置项：

```kconfig
choice BOARD_TYPE
    prompt "Board Type"
    default BOARD_TYPE_BREAD_COMPACT_WIFI
    help
        Board type. 开发板类型
    
    # ... 其他开发板选项 ...
    
    config BOARD_TYPE_MY_CUSTOM_BOARD
        bool "My Custom Board (我的自定义开发板)"
        depends on IDF_TARGET_ESP32S3  # 根据你的目标芯片修改
endchoice
```

**注意事项：**
- `BOARD_TYPE_MY_CUSTOM_BOARD` 是配置项名称，需要全大写，使用下划线分隔
- `depends on` 指定了目标芯片类型（如 `IDF_TARGET_ESP32S3`、`IDF_TARGET_ESP32C3` 等）
- 描述文字可以使用中英文

#### 在 CMakeLists.txt 中添加开发板配置

打开 `main/CMakeLists.txt` 文件，在开发板类型判断部分添加新的配置：

```cmake
# 在 elseif 链中添加你的开发板配置
elseif(CONFIG_BOARD_TYPE_MY_CUSTOM_BOARD)
    set(BOARD_TYPE "my-custom-board")  # 与目录名一致
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)  # 根据屏幕大小选择合适的字体
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)  # 可选，如果需要表情显示
endif()
```

**字体和表情配置说明：**

根据屏幕分辨率选择合适的字体大小：
- 小屏幕（128x64 OLED）：`font_puhui_basic_14_1` / `font_awesome_14_1`
- 中小屏幕（240x240）：`font_puhui_basic_16_4` / `font_awesome_16_4`
- 中等屏幕（240x320）：`font_puhui_basic_20_4` / `font_awesome_20_4`
- 大屏幕（480x320+）：`font_puhui_basic_30_4` / `font_awesome_30_4`

表情集合选项：
- `twemoji_32` - 32x32 像素表情（小屏幕）
- `twemoji_64` - 64x64 像素表情（大屏幕）

### 5. 配置和编译

#### 方法一：使用 idf.py 手动配置

1. **设置目标芯片**（首次配置或更换芯片时）：
   ```bash
   # 对于 ESP32-S3
   idf.py set-target esp32s3
   
   # 对于 ESP32-C3
   idf.py set-target esp32c3
   
   # 对于 ESP32
   idf.py set-target esp32
   ```

2. **清理旧配置**：
   ```bash
   idf.py fullclean
   ```

3. **进入配置菜单**：
   ```bash
   idf.py menuconfig
   ```
   
   在菜单中导航到：`Xiaozhi Assistant` -> `Board Type`，选择你的自定义开发板。

4. **编译和烧录**：
   ```bash
   idf.py build
   idf.py flash monitor
   ```

#### 方法二：使用 release.py 脚本（推荐）

如果你的开发板目录下有 `config.json` 文件，可以使用此脚本自动完成配置和编译：

```bash
python scripts/release.py my-custom-board
```

此脚本会自动：
- 读取 `config.json` 中的 `target` 配置并设置目标芯片
- 应用 `sdkconfig_append` 中的编译选项
- 完成编译并打包固件

### 6. 创建README.md

在README.md中说明开发板的特性、硬件要求、编译和烧录步骤：


## 常见开发板组件

### 1. 显示屏

项目支持多种显示屏驱动，包括:
- ST7789 (SPI)
- ILI9341 (SPI)
- SH8601 (QSPI)
- 等...

### 2. 音频编解码器

支持的编解码器包括:
- ES8311 (常用)
- ES7210 (麦克风阵列)
- AW88298 (功放)
- 等...

### 3. 电源管理

一些开发板使用电源管理芯片:
- AXP2101
- 其他可用的PMIC

### 4. MCP设备控制

可以添加各种MCP工具，让AI能够使用:
- Speaker (扬声器控制)
- Screen (屏幕亮度调节)
- Battery (电池电量读取)
- Light (灯光控制)
- 等...

## 开发板类继承关系

- `Board` - 基础板级类
  - `WifiBoard` - Wi-Fi连接的开发板
  - `Ml307Board` - 使用4G模块的开发板
  - `DualNetworkBoard` - 支持Wi-Fi与4G网络切换的开发板

## 开发技巧

1. **参考相似的开发板**：如果您的新开发板与现有开发板有相似之处，可以参考现有实现
2. **分步调试**：先实现基础功能（如显示），再添加更复杂的功能（如音频）
3. **管脚映射**：确保在config.h中正确配置所有管脚映射
4. **检查硬件兼容性**：确认所有芯片和驱动程序的兼容性

## 可能遇到的问题

1. **显示屏不正常**：检查SPI配置、镜像设置和颜色反转设置
2. **音频无输出**：检查I2S配置、PA使能引脚和编解码器地址
3. **无法连接网络**：检查Wi-Fi凭据和网络配置
4. **无法与服务器通信**：检查MQTT或WebSocket配置

## 参考资料

- ESP-IDF 文档: https://docs.espressif.com/projects/esp-idf/
- LVGL 文档: https://docs.lvgl.io/
- ESP-SR 文档: https://github.com/espressif/esp-sr 