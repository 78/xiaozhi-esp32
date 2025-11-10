# Dark Theme Configuration

[English](#english) | [中文](#中文)

---

## English

### Overview

The Xiaozhi ESP32 project includes a built-in theme system with two available themes:
- **Light Theme** (default)
- **Dark Theme**

The theme setting is stored in the device's NVS (Non-Volatile Storage) and persists across reboots.

### Available Themes

#### Light Theme
- Background: White (`#FFFFFF`)
- Text: Black (`#000000`)
- Chat Background: Light Gray (`#E0E0E0`)
- User Bubble: Green (`#00FF00`)
- Assistant Bubble: Light Gray (`#DDDDDD`)
- Border: Black (`#000000`)

#### Dark Theme
- Background: Black (`#000000`)
- Text: White (`#FFFFFF`)
- Chat Background: Dark Gray (`#1F1F1F`)
- User Bubble: Green (`#00FF00`)
- Assistant Bubble: Dark Gray (`#222222`)
- Border: White (`#FFFFFF`)
- Low Battery: Red (`#FF0000`)

### How Themes Work

The theme system is implemented in the display layer:
- Theme configuration is defined in `main/display/lcd_display.cc`
- Theme preference is stored in NVS under the namespace `"display"` with key `"theme"`
- Default theme is `"light"` if no NVS value exists
- Themes are registered with the `LvglThemeManager` at initialization

### Setting Dark Theme as Default

There are several ways to set the dark theme as default:

#### Method 1: Via MCP Protocol (Recommended)

If your device supports MCP (Model Context Protocol), you can change the theme using the `self.screen.set_theme` tool:

```json
{
  "theme": "dark"
}
```

This will immediately switch to dark theme and save the preference to NVS.

#### Method 2: Modify NVS Partition Before Flashing

1. Create or modify an NVS partition CSV file with the theme setting:

```csv
key,type,encoding,value
display,namespace,,
theme,data,string,dark
```

2. Generate the NVS binary:

```bash
python $IDF_PATH/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py generate nvs.csv nvs.bin 0x4000
```

3. Flash the NVS partition to your device:

```bash
esptool.py --chip esp32s3 --port /dev/ttyUSB0 write_flash 0x9000 nvs.bin
```

**Note:** The NVS partition address (`0x9000`) may vary depending on your board's partition table. Check your `partitions.csv` file for the correct address.

#### Method 3: Modify NVS at Runtime

You can modify the NVS storage programmatically by adding code to set the theme preference during initialization:

```cpp
#include "settings.h"

// In your initialization code
Settings settings("display", true);
settings.SetString("theme", "dark");
```

#### Method 4: Modify Default in Source Code

If you want to change the default theme in the source code, modify `main/display/lcd_display.cc`:

```cpp
// Find this line (around line 74):
std::string theme_name = settings.GetString("theme", "light");

// Change the default from "light" to "dark":
std::string theme_name = settings.GetString("theme", "dark");
```

Then rebuild and flash the firmware.

### Why No Menuconfig Option?

The theme setting is implemented as a runtime configuration stored in NVS rather than a compile-time configuration. This design choice allows:

1. **Runtime Changes**: Users can switch themes without reflashing firmware
2. **User Preferences**: Each device can maintain its own theme preference
3. **Remote Control**: Themes can be changed via MCP protocol or other remote interfaces
4. **Persistence**: Theme preference survives firmware updates (when NVS partition is preserved)

If a menuconfig option is desired, it could be added to set the **default** theme when NVS is empty, but the current architecture favors runtime configurability.

### Implementation Details

The theme system consists of several components:

1. **Theme Base Class** (`main/display/display.h`)
   - Abstract base class for all themes
   - Stores theme name

2. **LvglTheme Class** (`main/display/lvgl_display/lvgl_theme.h`)
   - Implements theme for LVGL-based displays
   - Stores colors, fonts, and styling information

3. **LvglThemeManager** (`main/display/lvgl_display/lvgl_theme.h`)
   - Singleton manager for registered themes
   - Provides theme lookup by name

4. **Settings Class** (`main/settings.h`)
   - Wrapper for NVS storage operations
   - Handles reading/writing theme preferences

5. **Display Initialization** (`main/display/lcd_display.cc`)
   - Registers available themes
   - Loads theme preference from NVS
   - Applies theme to display

### Troubleshooting

#### Theme doesn't persist after reboot
- Ensure NVS partition is properly initialized
- Check that the NVS partition is not being erased during flash operations
- Verify Settings class is opened with `read_write = true` when setting values

#### Dark theme not available
- Verify your board uses LCD display (themes are not implemented for OLED displays)
- Check that `InitializeLcdThemes()` is being called during display initialization
- Ensure you're using a recent version of the firmware

#### Cannot change theme via MCP
- Verify MCP protocol is enabled and working
- Check that your display supports the theme system (should have `GetTheme() != nullptr`)
- Review MCP server logs for errors

### Future Enhancements

Possible future improvements to the theme system:
- Custom themes loaded from assets partition
- Additional built-in themes
- Per-element theme customization via MCP
- Theme preview before applying
- Automatic theme switching based on time of day

---

## 中文

### 概述

小智 ESP32 项目内置了主题系统，提供两种可用主题：
- **浅色主题**（默认）
- **深色主题**

主题设置存储在设备的 NVS（非易失性存储）中，重启后保持不变。

### 可用主题

#### 浅色主题
- 背景：白色 (`#FFFFFF`)
- 文本：黑色 (`#000000`)
- 聊天背景：浅灰色 (`#E0E0E0`)
- 用户气泡：绿色 (`#00FF00`)
- 助手气泡：浅灰色 (`#DDDDDD`)
- 边框：黑色 (`#000000`)

#### 深色主题
- 背景：黑色 (`#000000`)
- 文本：白色 (`#FFFFFF`)
- 聊天背景：深灰色 (`#1F1F1F`)
- 用户气泡：绿色 (`#00FF00`)
- 助手气泡：深灰色 (`#222222`)
- 边框：白色 (`#FFFFFF`)
- 低电量：红色 (`#FF0000`)

### 主题工作原理

主题系统在显示层实现：
- 主题配置定义在 `main/display/lcd_display.cc`
- 主题偏好存储在 NVS 中，命名空间为 `"display"`，键为 `"theme"`
- 如果 NVS 中没有值，默认主题为 `"light"`
- 主题在初始化时注册到 `LvglThemeManager`

### 将深色主题设置为默认

有几种方法可以将深色主题设置为默认：

#### 方法 1：通过 MCP 协议（推荐）

如果您的设备支持 MCP（模型上下文协议），可以使用 `self.screen.set_theme` 工具更改主题：

```json
{
  "theme": "dark"
}
```

这将立即切换到深色主题并将偏好保存到 NVS。

#### 方法 2：烧录前修改 NVS 分区

1. 创建或修改带有主题设置的 NVS 分区 CSV 文件：

```csv
key,type,encoding,value
display,namespace,,
theme,data,string,dark
```

2. 生成 NVS 二进制文件：

```bash
python $IDF_PATH/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py generate nvs.csv nvs.bin 0x4000
```

3. 将 NVS 分区烧录到设备：

```bash
esptool.py --chip esp32s3 --port /dev/ttyUSB0 write_flash 0x9000 nvs.bin
```

**注意：** NVS 分区地址（`0x9000`）可能因开发板的分区表而异。请检查您的 `partitions.csv` 文件以获取正确的地址。

#### 方法 3：运行时修改 NVS

您可以通过编程方式修改 NVS 存储，在初始化期间添加代码来设置主题偏好：

```cpp
#include "settings.h"

// 在初始化代码中
Settings settings("display", true);
settings.SetString("theme", "dark");
```

#### 方法 4：修改源代码中的默认值

如果您想在源代码中更改默认主题，请修改 `main/display/lcd_display.cc`：

```cpp
// 找到这一行（大约在第 74 行）：
std::string theme_name = settings.GetString("theme", "light");

// 将默认值从 "light" 改为 "dark"：
std::string theme_name = settings.GetString("theme", "dark");
```

然后重新构建并烧录固件。

### 为什么没有 Menuconfig 选项？

主题设置作为运行时配置存储在 NVS 中，而不是编译时配置。这种设计选择的优点：

1. **运行时更改**：用户可以在不重新烧录固件的情况下切换主题
2. **用户偏好**：每个设备可以保持自己的主题偏好
3. **远程控制**：可以通过 MCP 协议或其他远程接口更改主题
4. **持久性**：主题偏好在固件更新时保留（当 NVS 分区被保留时）

如果需要 menuconfig 选项，可以添加一个选项来设置 NVS 为空时的**默认**主题，但当前架构更倾向于运行时可配置性。

### 实现细节

主题系统由几个组件组成：

1. **主题基类** (`main/display/display.h`)
   - 所有主题的抽象基类
   - 存储主题名称

2. **LvglTheme 类** (`main/display/lvgl_display/lvgl_theme.h`)
   - 为基于 LVGL 的显示器实现主题
   - 存储颜色、字体和样式信息

3. **LvglThemeManager** (`main/display/lvgl_display/lvgl_theme.h`)
   - 已注册主题的单例管理器
   - 按名称提供主题查找

4. **Settings 类** (`main/settings.h`)
   - NVS 存储操作的包装器
   - 处理主题偏好的读写

5. **显示初始化** (`main/display/lcd_display.cc`)
   - 注册可用主题
   - 从 NVS 加载主题偏好
   - 将主题应用到显示器

### 故障排除

#### 重启后主题不保持
- 确保 NVS 分区已正确初始化
- 检查在烧录操作期间 NVS 分区未被擦除
- 验证设置值时 Settings 类以 `read_write = true` 打开

#### 深色主题不可用
- 验证您的开发板使用 LCD 显示器（主题未针对 OLED 显示器实现）
- 检查在显示初始化期间是否调用了 `InitializeLcdThemes()`
- 确保您使用的是最新版本的固件

#### 无法通过 MCP 更改主题
- 验证 MCP 协议已启用并正常工作
- 检查您的显示器支持主题系统（应该有 `GetTheme() != nullptr`）
- 查看 MCP 服务器日志中的错误

### 未来增强

主题系统可能的未来改进：
- 从资源分区加载自定义主题
- 额外的内置主题
- 通过 MCP 进行逐元素主题自定义
- 应用前的主题预览
- 基于时间的自动主题切换
