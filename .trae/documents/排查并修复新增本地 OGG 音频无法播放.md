## 结论与假设
- 你的两个本地音频映射是正确的，`lang_config.h` 已包含：`OGG_LOW_BATTERY_OFF` 与 `OGG_LOW_BATTERY_REMIND`（main/assets/lang_config.h:169–181）。
- 更可能的原因是新增的 `.ogg` 没有被编译进固件：`file(GLOB ...)` 在 CMake 配置阶段展开，新增文件通常不会被自动纳入，需强制重新配置或清理重建；你把 `success.ogg` 复制改名后仍不响，符合“未嵌入”这一特征。

## 验证步骤
1. 重新配置并完整重建/烧录：执行 `idf.py fullclean build flash monitor` 或删除 `build` 目录后重建，确保 CMake 重新扫描 `assets/common/*.ogg` 并执行 `EMBED_FILES`（main/CMakeLists.txt:590–594）。
2. 检查符号是否存在：在构建产物 `*.elf` 中查找 `_binary_low_battery_remind_ogg_start` 与 `_binary_low_battery_off_ogg_start`，若不存在则说明未嵌入。
3. 运行时打印长度：在调用前打印 `Lang::Sounds::OGG_LOW_BATTERY_REMIND.size()` 与 `Lang::Sounds::OGG_SUCCESS.size()`，长度接近则说明嵌入成功；若为 0 或极小则异常。
4. 直接在可工作的路径试播：在已能播放成功音效的同一位置（main/application.cc:542 或 ai_martube_esp32s3.cc:958）紧邻插入一行播放 `OGG_LOW_BATTERY_REMIND`，排除触发条件或关机逻辑的影响。

## 代码层改进（待确认后执行）
- CMake 自动感知新增音频：把 `file(GLOB ...)` 改为 `file(GLOB CONFIGURE_DEPENDS ...)`，例如对 `COMMON_SOUNDS` 与 `LANG_SOUNDS` 应用该选项，让 CMake 在文件新增时自动重新配置。
- 播放健壮性：在 `AudioService::PlaySound` 放宽对 `OpusTags` 的依赖（仅 `OpusHead` 后即可开始处理音频包），提升对不同编码文件的兼容性。
- 诊断日志：在 `PlaySound` 中打印解析到的采样率、页计数与入队包数，便于快速判断是“未嵌入”还是“解析失败”。

## 预期结果
- 完整重建后应能在低电量分支正常播放新音频；若仍不响且符号存在、长度合理，再继续执行播放逻辑的健壮性改造与详细日志排查。