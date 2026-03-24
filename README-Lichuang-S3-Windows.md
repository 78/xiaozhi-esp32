# 立创实战派 ESP32-S3 + Windows 构建说明（基于 xiaozhi-esp32）

本文记录针对 **立创实战派 ESP32-S3 开发板** 与 **Windows** 环境所做的排查结论与工程修改，便于复现与向 [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) 提 PR 或自用 Fork。

---

## 1. 摄像头：资料写 GC0308，实际是 GC2145

- 立创实战派 ESP32-S3 部分公开说明/资料标注传感器为 **GC0308**。
- 实板与串口日志（及 `esp32-camera` 探测）表明 DVP 传感器为 **GC2145**。
- **xiaozhi** 中 `lichuang-dev` 板级使用 **`Esp32Camera(camera_config_t)` + `esp_camera_init()`**（`espressif/esp32-camera`），由驱动栈自动探测传感器；无需在板级 `config.json` 里写 `CONFIG_CAMERA_GC2145_*`（那是 **esp_video / esp_cam_sensor** 路径的选项）。
- 若按 GC0308 去改引脚或 menuconfig，容易走偏；以 **GC2145 + 现有 lichuang 引脚与 PCA9557 电源时序** 为准即可。

---

## 2. Windows 下构建/烧录问题与处理

### 2.1 `build_default_assets.py`：`UnicodeDecodeError: 'gbk' codec can't decode...`

- **原因**：Windows 默认文本编码常为 GBK，而 `sdkconfig` 中可能含 UTF-8 注释/字符。
- **处理**：`scripts/build_default_assets.py` 中增加 **`_read_sdkconfig_lines()`**，以 **二进制读取** `sdkconfig` 再 **`utf-8` + `errors="replace"`** 解码；所有读取 `sdkconfig` 的解析函数改为遍历上述行列表。
- 顺带：`generate_config_json` 后读回 `config.json`、生成 mmap 头文件等处使用 **`encoding='utf-8'`**。

### 2.2 `ccache`：`Permission denied` / Rust panic（错误 5）

- **现象**：ninja 调用 `ccache` 再启动 `xtensa-esp-elf-gcc` 时，子进程创建被拒绝。
- **处理**：在根目录 **`CMakeLists.txt`** 中，在 `include(project.cmake)` **之前** 增加：

  `set(CCACHE_ENABLE 0 CACHE BOOL "" FORCE)`

  使编译命令不再经 ccache 包装（若需缓存可自行改回并排除杀软）。向上游提 PR 时可改为 `if(WIN32)` 包裹以不影响 Linux CI。

### 2.3 `xtensa-esp-elf-gcc`：`cannot execute ... cc1.exe` / `CreateProcess: No such file or directory`

- **说明**：`cc1.exe` 在磁盘存在时仍可能因 **VC++ 运行库缺失** 或 **Defender/策略拦截** 报错。
- **建议**：安装/修复 **Microsoft Visual C++ 2015–2022 Redistributable (x64)**；为 `D:\Espressif\tools\` 加杀毒排除；必要时 `idf.py build -j 1` 降低并行。

### 2.4 烧录：`generated_assets.bin ... will not fit in ... flash`

- **原因**：默认 **`partitions/v2/16m.csv`** 中 **`assets` 分区为 8MB**（起始 `0x800000`），而完整 SR 模型 + 字体 + emoji 等生成的 **`generated_assets.bin` 可达约 11.5MB**。
- **处理**：在 menuconfig 中将 **Custom partition CSV** 改为 **`partitions/v2/16m_large_assets.csv`**（**单 `ota_0` + 约 12MB `assets`**，无 `ota_1`，失去 A/B 双槽回滚，请自行权衡）。
- **替代**：不改编分区表，在 **menuconfig** 中减少打包的 Multinet 语言/模型、换更小字体或 emoji，使 `generated_assets.bin` **≤ 8MB**。

---

## 3. 相对上游的改动文件清单

| 文件 | 说明 |
|------|------|
| `CMakeLists.txt` | 关闭 `CCACHE_ENABLE`，缓解 Windows 下 ccache 子进程失败 |
| `scripts/build_default_assets.py` | `sdkconfig`/JSON 等按 UTF-8 安全读取；修正自定义唤醒词解析块缩进 |
| `partitions/v2/16m_large_assets.csv` | **新增**：16MB Flash、单 OTA、大 assets |
| `partitions/v2/README.md` | 补充 **`16m_large_assets.csv`** 的说明条目 |
| `README-Lichuang-S3-Windows.md` | **新增**：本文档 |

---

## 4. 构建与烧录（简要）

```text
cd D:\esp32\xiaozhi-esp32
idf.py set-target esp32s3
idf.py menuconfig   # 选择板型 lichuang-dev；若 assets 过大则换 partitions/v2/16m_large_assets.csv
idf.py build flash
```

---

## 5. 许可证与上游

- 上游项目版权仍归 **xiaozhi-esp32** 原作者与许可协议；本文档仅描述补丁与硬件说明勘误。
- 官方仓库：<https://github.com/78/xiaozhi-esp32>

---

## 6. 提交到本仓库 / Fork 的建议

```bash
cd D:\esp32\xiaozhi-esp32
git add CMakeLists.txt scripts/build_default_assets.py partitions/v2/16m_large_assets.csv partitions/v2/README.md README-Lichuang-S3-Windows.md
git commit -m "fix(win): UTF-8 sdkconfig in build_default_assets, disable ccache; add 16m_large_assets; doc Lichuang S3 GC2145"
git push
```
