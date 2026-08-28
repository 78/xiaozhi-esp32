# DF-K10 (行客 K10) 板型配置说明

## 硬件信息

| 项目 | 型号/规格 |
|-----|----------|
| 开发板 | 行客 K10 (DF-K10) |
| 芯片 | ESP32-S3 (QFN56, v0.2) |
| Flash | 16MB |
| PSRAM | 8MB (Octal, 80MHz) |
| 相机 | GC2145 |
| 屏幕 | 支持 LCD 显示 |
| 音频 | 支持麦克风和扬声器 |

## 烧录成功配置

### 关键配置项

在 `sdkconfig.defaults.esp32s3` 中添加了以下配置：

```ini
# Board Type: DF-K10 (行客 K10)
CONFIG_BOARD_TYPE_DF_K10=y

# Camera: GC2145
CONFIG_CAMERA_GC2145=y

# Camera Endianness Swap (fix for DF-K10 hardware)
CONFIG_XIAOZHI_ENABLE_CAMERA_ENDIANNESS_SWAP=y
```

### 编译和烧录命令

```bash
# 进入项目目录
cd ~/xiaozhi-esp32-one/xiaozhi-esp32

# 激活 ESP-IDF 环境
. ~/.espressif/v5.5.2/esp-idf/export.sh

# 编译项目
idf.py build

# 烧录到设备（自动重启）
idf.py -p /dev/tty.usbmodem143101 flash

# 查看设备日志（可选）
idf.py -p /dev/tty.usbmodem143101 monitor
```

## 修复的问题

### 1. 屏幕无显示
**问题**: 重新插拔设备后，屏幕没有反应

**原因**: 使用了错误的板型配置 (BREAD_COMPACT_WIFI) 与实际硬件 (DF-K10) 不匹配

**解决**: 修改配置为 `CONFIG_BOARD_TYPE_DF_K10=y`

### 2. WiFi 热点未创建
**问题**: 设备没有进入创建 WiFi 热点的页面

**原因**: 错误的板型初始化导致硬件外设（WiFi、屏幕等）无法正确初始化

**解决**: 使用正确的板型配置后，硬件初始化正确，WiFi 热点可以正常创建

### 3. 相机图像问题
**问题**: 相机显示可能有图像倒置或颜色异常

**原因**: GC2145 相机的字节序与 ESP32-S3 不匹配

**解决**: 启用 `CONFIG_XIAOZHI_ENABLE_CAMERA_ENDIANNESS_SWAP=y` 进行端序转换

## 预编译固件对比

| 配置项 | 预编译固件 | 正确配置 |
|-------|----------|---------|
| 板型 | DF-K10 | ✅ DF-K10 |
| 相机 | GC2145 | ✅ GC2145 |
| 端序转换 | 启用 | ✅ 启用 |

## 测试结果

✅ 烧录成功
✅ 设备重启
✅ 系统日志正常
✅ 音频编解码器工作正常
✅ State machine 状态转换正常

## 后续步骤

1. 连接设备创建的 WiFi 热点
2. 访问配置页面进行 WiFi 设置
3. 在 https://xiaozhi.me 绑定设备
4. 测试语音唤醒功能
5. 测试相机和显示功能

## 文件变更

- `sdkconfig.defaults.esp32s3`: 添加 DF-K10 板型和 GC2145 相机配置

## Git 提交

```
commit 6f9de81101f5e4c4e53035b7b1eab4a2b18ea13f
Author: Developer <developer@xiaozhi.local>
Date:   Fri Feb 27 15:28:12 2026 +0800

    配置DF-K10板型和GC2145相机支持
```

---

**配置完成时间**: 2026-02-27 15:28
**测试设备**: 行客 K10 (ESP32-S3)
**固件版本**: v2.2.3
