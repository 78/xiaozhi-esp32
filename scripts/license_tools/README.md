# ESP32生产固件烧录工具

这是一个基于tkinter的ESP32生产用途的固件烧录工具，支持自动检测USB设备、读取MAC地址、获取授权信息、下载并烧录固件。

## 功能特性

- **自动设备检测**: 自动监控USB设备插入（支持 `/dev/ttyACMx` 和 `/dev/ttyUSBx`）
- **设备信息读取**: 使用esptool读取ESP32设备的MAC地址
- **授权管理**: 通过API获取设备授权信息和固件下载链接
- **固件烧录**: 自动下载并烧录固件到ESP32设备
- **序列号烧录**: 将序列号烧录到BLOCK_USR_DATA
- **授权密钥烧录**: 将授权密钥烧录到BLOCK_KEY0
- **GUI界面**: 友好的图形用户界面，实时显示操作日志
- **可配置网格布局**: 支持自定义端口显示布局（默认4x4）
- **批量烧录**: 支持同时烧录多个设备
- **固件缓存**: 自动缓存下载的固件，避免重复下载

## 配置文件

工具使用 `config.json` 文件保存配置，支持以下配置项：

```json
{
  "license_url": "https://xiaozhi.me/api/developers/generate-license?token=YOUR_TOKEN&seed=00:00:00:00:00:00",
  "grid_rows": 4,
  "grid_cols": 4,
  "fullscreen_on_startup": true
}
```

### 网格布局配置

通过修改 `grid_rows` 和 `grid_cols` 可以调整端口显示布局：

- **4x4布局** (默认): `{"grid_rows": 4, "grid_cols": 4}` - 16个端口，4行4列
- **8x1布局**: `{"grid_rows": 8, "grid_cols": 1}` - 8个端口，8行1列
- **2x8布局**: `{"grid_rows": 2, "grid_cols": 8}` - 16个端口，2行8列
- **1x16布局**: `{"grid_rows": 1, "grid_cols": 16}` - 16个端口，1行16列

配置文件会在程序首次运行时自动创建，也可以手动创建。修改配置后重启程序生效。

### 配置项说明

- **license_url**: 授权API链接，`seed`参数会被自动替换为设备MAC地址
- **grid_rows**: 网格行数，控制端口显示的行数
- **grid_cols**: 网格列数，控制端口显示的列数
- **fullscreen_on_startup**: 启动时是否全屏，`true`为全屏，`false`为窗口模式

## 安装依赖

```bash
pip3 install -r requirements.txt
```

或者使用启动脚本自动安装：

```bash
./run_tool.sh
```

## 使用方法

### 1. 启动工具

```bash
# 方式1: 直接运行Python脚本
python3 esp32_production_tool.py

# 方式2: 使用启动脚本（推荐）
./run_tool.sh
```

### 2. 配置授权链接

#### 首次运行配置

首次运行工具时，如果配置文件不存在或授权链接为空，程序会自动进入配置模式，弹出配置对话框：

- **授权链接**: 输入完整的授权API链接
- **网格布局**: 选择端口显示的行列数（支持预设布局）
- **全屏设置**: 选择启动时是否全屏显示

配置完成后会自动保存到 `config.json` 文件。

#### 手动配置

也可以直接编辑 `config.json` 文件：

```json
{
  "license_url": "https://xiaozhi.me/api/developers/generate-license?token=YOUR_TOKEN&seed=00:00:00:00:00:00",
  "grid_rows": 4,
  "grid_cols": 4,
  "fullscreen_on_startup": true
}
```

注意：`seed`参数会被自动替换为设备的MAC地址。授权链接只能通过配置文件设置，不能在界面上修改。

### 3. 连接ESP32设备

将ESP32设备通过USB连接到计算机。工具会自动检测到新设备并显示在日志中。

### 4. 读取设备信息

点击"读取设备信息"按钮，工具会：
- 读取设备的MAC地址
- 获取芯片信息
- 检查是否已烧录序列号

### 5. 开始烧录

如果设备未烧录过，"开始烧录"按钮会变为可用状态。点击后工具会自动执行：

1. **获取授权信息**: 使用设备MAC地址替换seed参数，请求授权API
2. **下载固件**: 从授权响应中获取固件下载链接并下载
3. **烧录固件**: 使用esptool烧录固件到设备
4. **烧录序列号**: 将序列号烧录到BLOCK_USR_DATA
5. **烧录授权密钥**: 将授权密钥烧录到BLOCK_KEY0

## 授权API响应格式

工具期望的授权API响应格式：

```json
{
  "success": true,
  "message": "授权生成成功",
  "data": {
    "product_name": "小智AI语音盒子",
    "board_name": "kevin-box-2",
    "serial_number": "TCXZ_KEVINBOX2__38FBDDE984330E50",
    "license_key": "ioyI4KWmB41BXu0FgfqioLRncXinfttv",
    "license_algorithm": "hmac-sha256",
    "created_at": "2025-06-11T18:40:27.000Z",
    "firmware": {
      "version": "1.8.1",
      "image_url": "http://example.com/firmware.bin",
      "image_size": 3851712,
      "image_sha256": "ccf0c610643ec35819b360b081ed3bf16b53396c82558e8e050fa791947b03de"
    }
  }
}
```

## 注意事项

1. **权限要求**: 工具需要访问串口设备，在Linux系统上可能需要将用户添加到`dialout`组：
   ```bash
   sudo usermod -a -G dialout $USER
   ```

2. **设备驱动**: 确保已安装ESP32设备的USB驱动程序

3. **网络连接**: 烧录过程需要网络连接来获取授权信息和下载固件

4. **安全性**: 序列号和授权密钥一旦烧录无法更改，请确保数据正确

5. **固件缓存**: 下载的固件会保存在 `firmware_cache/` 目录中，相同文件名的固件会被复用以提高效率

## 故障排除

### 设备检测失败
- 检查USB连接
- 确认设备驱动已安装
- 检查串口权限

### 授权请求失败
- 检查网络连接
- 验证授权链接是否正确
- 确认API服务器可访问

### 烧录失败
- 确保设备未被其他程序占用
- 检查设备是否处于下载模式
- 验证固件文件完整性

## 开发说明

工具基于以下技术栈开发：
- **GUI框架**: tkinter
- **ESP32工具**: esptool, espefuse
- **网络请求**: requests
- **串口检测**: pyserial

主要模块：
- `ESP32ProductionTool`: 主应用类
- `monitor_devices()`: USB设备监控
- `read_device_info()`: 设备信息读取
- `get_license_info()`: 授权信息获取
- `download_firmware()`: 固件下载
- `flash_firmware()`: 固件烧录
- `flash_license_info()`: 授权信息烧录 