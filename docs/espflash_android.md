# 使用 Android ESPFlash 刷写（详细步骤）

本文是补充指南，面向使用 Android 手机与 ESPFlash 应用刷写本仓库在 feature/c6-ai-terminal 分支通过 CI 生成的固件。

准备工作
- 手机支持 USB OTG。
- OTG 线、USB-串口适配器（若开发板没有直接 USB），或直接用带 USB 接口的开发板。
- 已下载的固件文件（从 GitHub Actions Artifacts）。

步骤
1. 下载构建产物（Artifact）：仓库 → Actions → 选择最近的 workflow run → Artifacts → 下载.
2. 在手机上安装并打开 ESPFlash（或你选择的刷机工具）。
3. 用 OTG 连接手机与开发板的 USB（或 USB‑TTL），确保接线正确（TX↔RX、GND↔GND）。
4. 如果需要手动进入 bootloader 模式，请按 BOOT/EN 的组合（参见 README）。
5. ESPFlash 中选择串口（手机识别到的设备）和波特率（建议 115200）。
6. 添加需要烧写的 bin 文件并填写偏移：
   - bootloader.bin -> 0x1000
   - partition_table.bin -> 0x8000
   - app.bin -> 0x10000
   或者直接选择单个 flashable firmware.bin 并刷写。
7. （可选）先擦除 Flash（Erase），再开始 Flash。
8. 刷写完成后重启板子并用串口监视器查看启动日志（115200）。

故障排查
- 若刷写显示找不到设备：检查 OTG、驱动、是否进入下载模式。
- 若 flash 成功但启动异常：检查 partition 表是否匹配，或使用 idf.py 在本机尝试复现编译/刷写步骤以获得更详细日志。


