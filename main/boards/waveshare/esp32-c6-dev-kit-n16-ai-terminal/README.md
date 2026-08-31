# Waveshare ESP32-C6-DEV-KIT-N16 AI Terminal — 使用说明

本文档包含该板的注意事项、接线说明、快速测试步骤，以及如何使用 Android 手机上的 ESPFlash 软件刷写由 CI 生成的固件（方案 A）。

## 注意事项

- 所有 GPIO 均基于 3.3V 逻辑。若你的外设（例如某些 IR 模块）使用 5V 电源，请务必使用电平移位或光耦隔离，避免直接把 5V 信号接到 MCU 引脚。
- 推荐给板子与外设供电使用稳定的 3.3V（逻辑）与外设独立电源（例如 MAX98357 的供电若为 5V，请确保逻辑线做电平兼容）。
- 在进行烧录前，请拔掉高电流负载或喇叭以减少 USB 供电干扰。若使用外部供电，请先接地。
- 如果使用 OTG 手机刷写，请保证 OTG 转接线与手机支持 USB OTG，并授予应用所需权限。

## 引脚对照（本仓库中定义）

- I2S（麦克风 INMP441）
  - MCLK: GPIO19
  - BCLK: GPIO21
  - LRCLK(WS): GPIO22
  - DIN (MIC SD): GPIO20
  - DOUT (DAC DIN): GPIO23

- 音频 I2C（可选，用于 codec）
  - I2C SDA: GPIO18
  - I2C SCL: GPIO8

- 红外（IR）
  - IR_RX (接收): GPIO4
  - IR_TX (发射/调制): GPIO5

- 按键（可选）
  - BOOT: GPIO9
  - PWR/RESET: GPIO2

（注：如需不同引脚，请以具体硬件原厂资料为准并对应修改仓库的 config.h）

## 接线示意（ASCII）

INMP441 (I2S MIC)        ESP32-C6
-----------------        ---------
VCC ------------------> 3.3V
GND ------------------> GND
SD  ------------------> GPIO20 (AUDIO_I2S_GPIO_DIN)
SCK ------------------> GPIO21 (AUDIO_I2S_GPIO_BCLK)
LRCK------------------> GPIO22 (AUDIO_I2S_GPIO_WS)

MAX98357 (I2S DAC)      ESP32-C6
-----------------        ---------
VIN ------------------> 5V or 3.3V (按模块要求)
GND ------------------> GND
DIN ------------------> GPIO23 (AUDIO_I2S_GPIO_DOUT)
BCLK------------------> GPIO21 (AUDIO_I2S_GPIO_BCLK)
LRCLK------------------> GPIO22 (AUDIO_I2S_GPIO_WS)
SPK+/SPK- -----------> 喇叭

IR 接收模块             ESP32-C6
-----------------        ---------
VCC ------------------> 3.3V (建议)
GND ------------------> GND
OUT(RXT) ------------> GPIO4 (IR_RX_GPIO)

IR 发射模块（或TTL输入） ESP32-C6
-----------------        ---------
VCC ------------------> 3.3V（或模块接受3.3V输出）
GND ------------------> GND
IN/XTD ---------------> GPIO5 (IR_TX_GPIO)

## 快速自检（串口日志）

1. 在串口工具中打开设备（波特率 115200）
2. 复位设备，观察启动日志，确认 I2S、AudioCodec 初始化无错误
3. 用遥控器对准 IR 接收器按键，查看串口是否出现类似：

   NEC received addr=0xXX cmd=0xYY

4. 在 MCP（远程控制面板）调用工具：
   - self.ir.get_last 获取最近接收到的 NEC 码
   - self.ir.send 发送 NEC 码（address, command）用于测试发射


## 使用 Android ESPFlash 刷写 CI 生成的固件（步骤）

下面以“使用 GitHub Actions 构建并下载 artifact，然后用 ESPFlash 刷写”为流程说明。

1. 在 GitHub 上获取固件：
   - 打开仓库页面 → 点击 "Actions" → 选择最近一次由 feature/c6-ai-terminal 触发的 workflow run。
   - 在该 workflow 页面向下滚动到 "Artifacts" 部分，下载构建产物（通常为 firmware.zip 或 firmware.bin）。

2. 准备手机与线材：
   - Android 手机需支持 USB OTG。准备一根 OTG 转接线（USB-C/USB-A to microUSB/USB-C，视手机接口而定）。
   - 若使用 USB‑串口（USB‑TTL），需要一个 USB‑串口适配器（例如 CP2102/FTDI/CH340），并用 OTG 将其接到手机上；也可以使用直接支持 USB 串口的开发板（通过 OTG 连接）。

3. 在手机上安装 ESPFlash 或类似刷机工具：
   - 在 Google Play 或可信 APK 源查找并安装“ESPFlash”（或“ESP32 Flash Tool”之类的工具）。我无法保证所有第三方应用的名称和 UI 一致，请根据你安装的应用界面做相应操作。

4. 连接设备：
   - 用 OTG 将手机与开发板连接（通常先将 USB‑TTL 的 USB 插入 OTG，再把 TX/RX/GND 线接到开发板）。
   - 确保接线正确：手机‑USB‑TTL TX -> 开发板 RX，手机‑USB‑TTL RX -> 开发板 TX，共地（GND）。若使用直接 USB（开发板有 USB 接口），直接用 OTG 线连接。

5. 进入下载模式（若需要）：
   - 常见方法：按住 BOOT（GPIO0，如有）→ 按下 RESET（EN）→ 先释放 RESET → 再释放 BOOT。不同板子可能有自动进入下载模式的电路，若有疑问请参考板子说明。

6. 在 ESPFlash 中选择串口和波特率：
   - 串口：选择手机识别到的 USB 串口设备
   - 波特率：建议 115200（稳定），可用 921600 加速

7. 填写待写入的 bin 文件与偏移（常用偏移）：
   - 如果你的 CI 产物是一个单独的 flashable image（例如 firmware.bin），可以直接选择文件并点击 Flash。
   - 如果 CI 产物是多个 bin（bootloader、partition_table、app），请在应用中添加多个文件并填写偏移：
     - bootloader.bin -> 0x1000
     - partition_table.bin -> 0x8000
     - app.bin -> 0x10000

   （以上偏移为 IDF 默认偏移，若你的项目有自定义 partition 表或地址请以实际构建输出为准。）

8. 开始刷写：
   - 先可选择擦除 Flash（Wipe/Erase）以防旧数据影响（可选，但推荐在第一次刷写时执行）。
   - 点击 Flash / Start 并等待进度完成。

9. 完成后重启并查看串口：
   - 刷写完成后重启开发板（按 RESET 或断电重启），在串口工具中打开 115200 波特并查看启动日志。

## 构建产物与下载（CI / Actions）

- 我会把构建配置为在 GitHub Actions 完成后把生成的固件添加为 Artifact（如果仓库已有 workflow，这会自动发生；若没有，我可以为你添加 workflow）。
- 下载位置：仓库 → Actions → 选择对应 run → Artifacts → 点击下载。

## 常见问题与排查

- 手机无法识别设备：确认 OTG 支持、线缆与适配器完好、给手机授权 USB 权限。尝试换一台手机或更换 OTG 线。  
- 刷写失败或提示时序问题/EN/BOOT：尝试手动进入下载模式的操作（参考第 5 步）。  
- 串口看不到日志：确认波特率与串口设备选择正确，板子已正确上电且 RX/TX 线未接反。  

---

如果你愿意，我接下来会：
1) 把上述 README（放在 board 目录）和一份更详细的闪存指南文件（docs/espflash_android.md）提交到 feature/c6-ai-terminal 分支；
2) 在 CI 构建成功后将构建产物保留为 Action artifact（已有 workflow 的会自动上传），并在 README 中补充 artifact 下载的直接链接步骤。

请回复“现在提交 README 并开启发布流程”，我就把这两个文档提交到分支并继续监控 CI 构建产物。