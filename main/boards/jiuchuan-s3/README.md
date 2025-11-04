# 九川科技小智AI音箱 (jiuchuan-s3)

基于 ESP32-S3 的智能 AI 音箱，集成语音识别、音频播放、电源管理等功能。

---

## 📋 硬件特性

### 主控芯片
- **MCU**: ESP32-S3-WROOM-1-N16R8
- **Flash**: 16MB
- **PSRAM**: 8MB (Octal SPI)

### 音频系统
- **音频编解码器**: ES8311 (输出) + ES7210 (输入，4路MIC)
- **功放芯片**: NS4150B (GPIO42 控制)
- **麦克风阵列**: 4 路麦克风，支持 TDM 模式

### 显示系统
- **屏幕**: 1.47" IPS LCD (172x320)
- **驱动芯片**: JD9853
- **接口**: QSPI (80MHz)

### 电源系统
- **电池**: 锂电池供电
- **充电管理**: USB-C 充电
- **电源控制**: GPIO15 (PWR_EN) + GPIO3 (PWR_BUTTON)
- **电池检测**: ADC1_CH4 (GPIO5) - 电池电压
- **充电检测**: ADC1_CH3 (GPIO4) - VBUS 检测
- **充电状态**: GPIO8 (CHG_DET)

---

## ⚡ 电源管理特性

### 开机/关机逻辑

#### **硬件开机**
- 短按电源键（GPIO3）→ 硬件电路拉高 GPIO15 → 系统上电

#### **软件关机**
1. **未充电模式**（电池供电）
   - 长按电源键 3 秒 → 显示"松开按键以关机"
   - 等待按键释放（最多 5 秒）
   - GPIO15=0 → 切断电池供电 → 完全断电
   - 下次开机：按电源键，硬件重新上电

2. **充电模式**（USB 供电）
   - 长按电源键 3 秒 → 显示"松开按键以关机"
   - 等待按键释放（最多 5 秒）
   - 进入深度睡眠，GPIO15 保持高电平
   - 下次开机：按电源键，ext0 唤醒系统

### 启动保护
- **保护期**: 开机后 3 秒内禁用关机功能
- **目的**: 防止开机时的长按被误判为关机操作

### 电池电量显示
- **采样方式**: ADC 采样 + OCV-SOC 曲线估算
- **满电阈值**: 95%（优化显示体验，避免拔充电器后立即掉电）
- **充电判断**: USB 连接 + 电量 < 95%

---

## 🛠️ 开发指南

### 环境要求
- **ESP-IDF**: v5.4.1
- **编译器**: xtensa-esp-elf-gcc 14.2.0
- **工具**: ESP-IDF VSCode Extension

### 编译步骤

> ⚠️ **网络提示**: 若在编译过程中访问在线库失败，可以尝试切换网络代理，或修改 `idf_component.yml` 替换为国内镜像源。

1. 使用 VSCode 打开项目根目录
2. 按 `Ctrl+Shift+P` → 选择 `ESP-IDF: Clean Project`
3. 设置 ESP-IDF 版本为 `v5.4.1`
4. 点击 VSCode 右下角提示，生成 `compile_commands.json` 文件
5. 选择目标设备: `ESP32-S3` → `JTAG`
6. 打开 SDK Configuration Editor (`sdkconfig`)
7. 搜索 `Board Type` → 选择 `jiuchuan-s3`
8. 保存配置 → 点击编译（Build）

### 烧录步骤

#### **方式一：UART 烧录（推荐）**
1. 使用 USB-C 数据线连接电脑与设备
2. **关闭设备电源**
3. **长按电源键不松手**（保持设备在下载模式）
4. 在烧录工具中选择对应的 COM 端口
5. 点击 Flash → 选择 UART 模式
6. **烧录完成前请勿松开电源键**
7. 烧录完成后，松开电源键，设备自动重启

#### **方式二：JTAG 烧录**
1. 连接 JTAG 调试器（如 ESP-Prog）
2. 在 VSCode 中选择 `ESP-IDF: Flash (JTAG)`
3. 等待烧录完成

---

## 🔧 开发注意事项

### GPIO 配置要点

#### **RTC GPIO**
- **GPIO3** (PWR_BUTTON): 用于 ext0 唤醒，充电模式下需配置为 RTC GPIO
- **GPIO15** (PWR_EN): 电源控制，需使用 `rtc_gpio_hold_en/dis` 管理状态

#### **关键 GPIO**
- **GPIO42** (SPK_EN): 功放使能，播放前需拉高
- **GPIO8** (CHG_DET): 充电检测，低电平表示充电中
- **GPIO5** (BAT_ADC): 电池电压采样
- **GPIO4** (VBUS_ADC): USB 电压采样

### 电源管理要点

1. **深度睡眠前**：
   - 确保 GPIO15 使用 `rtc_gpio_hold_en()` 保持状态
   - 配置 GPIO3 为 RTC GPIO 并启用 ext0 唤醒
   - 等待按键释放，避免立即唤醒

2. **深度睡眠后唤醒**：
   - 调用 `rtc_gpio_hold_dis(GPIO15)` 释放保持
   - 调用 `rtc_gpio_deinit(GPIO3)` 恢复为普通 GPIO

3. **未充电模式断电**：
   - 必须等待按键释放（GPIO3=LOW）
   - 原因：按键按下时硬件电路会持续拉高 GPIO15

### 音频系统初始化

```cpp
// 功放控制示例
gpio_set_level(GPIO_NUM_42, 1);  // 使能功放
// ... 播放音频 ...
gpio_set_level(GPIO_NUM_42, 0);  // 关闭功放（省电）
```

### 电池电量读取

```cpp
// 通过 PowerManager 获取
int battery_level = power_manager_->GetBatteryLevel();  // 0-100
bool is_charging = power_manager_->IsCharging();
bool usb_connected = power_manager_->IsUsbConnected();
```

---

## 🐛 故障排查

### 编译问题

**问题**: `fatal error: xxx.h: No such file or directory`  
**解决**: 
1. 清理工程: `idf.py fullclean`
2. 重新生成 `compile_commands.json`
3. 重新编译

**问题**: 在线组件下载失败  
**解决**: 
1. 检查网络连接
2. 使用国内镜像源（修改 `idf_component.yml`）
3. 手动下载组件到 `managed_components/`

### 烧录问题

**问题**: 无法识别串口  
**解决**:
1. 检查 USB 驱动是否安装（CH340/CP210x）
2. 尝试其他 USB 端口
3. 检查数据线是否支持数据传输

**问题**: 烧录失败或无法进入下载模式  
**解决**:
1. 确保长按电源键不松手
2. 尝试先短按一次电源键关机，再长按进入下载模式
3. 检查电池电量是否充足

### 运行时问题

**问题**: 长按开机不松手导致关机  
**解决**: 已优化，开机后 3 秒内禁用关机功能

**问题**: 充电状态下关机后无法开机  
**解决**: 已修复，松开按键后按一次电源键即可开机

**问题**: 未充电模式下长按不松手会重启  
**解决**: 已修复，系统会等待按键释放后再执行断电

**问题**: 音频播放无声音  
**检查**:
1. GPIO42 (SPK_EN) 是否拉高
2. 音量设置是否为 0
3. 音频编解码器是否正确初始化

---

## 📚 相关文档

- [ESP32-S3 技术参考手册](https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_cn.pdf)
- [ESP-IDF 编程指南](https://docs.espressif.com/projects/esp-idf/zh_CN/v5.4.1/)

---

## 📝 版本历史

### 最新优化 (2025-11)
- ✅ 优化电源管理：统一充电/未充电关机逻辑
- ✅ 添加启动保护：防止开机误触发关机
- ✅ 修复充电关机无法唤醒问题
- ✅ 修复未充电长按重启问题
- ✅ 优化电池电量显示体验

---

## 📧 技术支持

如有问题，请提交 Issue 或联系开发团队。