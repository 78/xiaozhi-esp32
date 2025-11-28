## 转换目标与命名

* 将 `main\assets\common\batteryremind.mp3` 转为 `main\assets\common\batteryremind.ogg`

* 将 `main\assets\common\batteryoff.mp3` 转为 `main\assets\common\batteryoff.ogg`

* 使用 Opus 编码、`bitrate=16k`、`mono(ac=1)`、`ar=16000`，与工程音频规范一致（scripts/ogg\_converter/xiaozhi\_ogg\_converter.py）

## 转换方法

* 命令行直接用 ffmpeg（适合快速批量）：

  * `ffmpeg -i main/assets/common/batteryremind.mp3 -acodec libopus -b:a 16k -ac 1 -ar 16000 main/assets/common/batteryremind.ogg`

  * `ffmpeg -i main/assets/common/batteryoff.mp3 -acodec libopus -b:a 16k -ac 1 -ar 16000 main/assets/common/batteryoff.ogg`

## 构建与烧录

* 完整清理并重建嵌入：`idf.py fullclean build flash monitor`

* 原因：`main/CMakeLists.txt:578-594` 用 `file(GLOB ...)` 收集 `.ogg`，新增文件需要重新配置/清理以确保 `EMBED_FILES` 生效

#

<br />

