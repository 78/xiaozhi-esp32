## 目标
- 排查并修复旋转编码器导致的音量抖动、数值来回跳动
- 统一硬件与软件层面的抖动抑制与方向判定，保证每个物理档位对应稳定的音量步进

## 现状与风险定位
- `ProcessEncoder` 轮询判向，A 下降沿延时 `200 µs` 再读 B，去抖阈值 `2000 µs`，任务周期 `10 ms`（`main/boards/ai-martube-esp32s3/ai_martube_esp32s3.cc:621`、`ai_martube_esp32s3.cc:33`、`ai_martube_esp32s3.cc:1338`）
- 仅单次采样与较短去抖，易受机械抖动影响出现正反交替判向与重复计步
- 音量每触发变化即 ±5，缺少脉冲阈值与时间窗，易被短时噪声驱动

## 问题诊断（现场检查）
- 硬件连接：
  - 检查 `CLK/DT/SW` 引脚与地线焊点，确保无虚焊、无冷焊、走线无拉裂（本板 `A=GPIO12`、`B=GPIO8`，`SW` 独立在 `KEY_INPUT_GPIO=GPIO15`，`main/boards/ai-martube-esp32s3/config.h:46–64`）
- 电源稳定性：
  - 测量 `VCC` 对 `GND`，动态波动保持在额定值 ±5% 内（空闲、旋转、音量变化时段）
- 示波器验证：
  - 采样 `CLK` 与 `DT` 同步通道，观察 90° 相移的正交脉冲；
  - 旋转单个档位应出现有限的有效跃迁序列（如 `00→01→11→10→00`）；噪声表现为乱序/回跳或毛刺宽度远小于期望；
  - 记录毛刺宽度分布用于设定软件去抖时间窗（建议窗口 10–50 ms 起点）。

## 代码审查要点
- 去抖算法：当前阈值 `ENCODER_DEBOUNCE_US=2000` 明显短于机械抖动，且仅一次延时采样 `200 µs`（`ai_martube_esp32s3.cc:33`, `ai_martube_esp32s3.cc:634`）。
- 方向判定：以 `A` 下降沿取 `B` 单点比较，易受毛刺影响；更稳健的是基于 `AB` 状态机的合法跃迁序列判向。
- 中断与竞态：本板使用轮询，无 ISR 与共享写冲突；但 `ProcessEncoder` 直接改动 `current_volume_` 并回调（`ai_martube_esp32s3.cc:649–656`, `ai_martube_esp32s3.cc:668–675`，`ai_martube_esp32s3.cc:696–708`），建议统一入口减少重复与瞬时抖动影响。

## 解决方案（硬件）
- 在 `CLK` 与 `DT` 到地各并联 `0.1 µF` 陶瓷电容（高速路径靠近编码器端），降低高频毛刺；
- 检查编码器机械结构并必要时更换，确保档位锁止无异响与触点无明显磨损。

## 解决方案（软件）
- 双重校验：
  - 首次检测到边沿后仅“标记事件与时间戳”，在下一轮或延时 `10–20 ms` 后再次读取 `AB`，两次判向一致且处于稳定窗口才计步；避免 `esp_rom_delay_us` 的忙等（当前 `ai_martube_esp32s3.cc:634`）。
- `AB` 状态机判向：
  - 维护上一次 `AB` 二位状态（`00/01/11/10`），仅接受合法顺序的相邻跃迁，累计方向得分；非法跃迁直接丢弃。
- 脉冲阈值与时间窗：
  - 引入累加器与窗口：在 `T_window=50 ms` 内，`|pulses|<3` 视为噪声忽略；达到阈值后按方向一次性应用 `±5` 音量步进并清零累加。
- 音量平滑：
  - 在提交到 `codec->SetOutputVolume` 前，对近 `N=3–5` 次请求做移动平均或简单滞回，降低短时抖动导致的来回跳；接口位于 `main/audio/audio_codec.cc:51–72` 的调用前层。
- 轮询周期与资源：
  - 保持任务周期 `5–10 ms`，避免轮询过慢丢脉冲；移除忙等 `esp_rom_delay_us(200)`，用时间戳驱动二次校验。

## 具体改动点（建议）
- 调整去抖常量：
  - 将 `#define ENCODER_DEBOUNCE_US 2000` 提升至 `10000–50000 µs`（先试 `20000 µs`），位置：`main/boards/ai-martube-esp32s3/ai_martube_esp32s3.cc:33`。
- 重构 `ProcessEncoder`：
  - 增加结构：`last_ab_state`、`pulse_accum`、`last_event_time_us`、`verify_deadline_us`；
  - 边沿到来→记录 `verify_deadline_us=now+DEBOUNCE_MS`；到时→读取 `AB`、依据状态机更新 `pulse_accum`；达到 `|pulse_accum|>=3`→一次性调用统一的 `ApplyVolumeStep(±5)`；
  - 移除单次 `esp_rom_delay_us(200)` 与单点 `B` 比较（`ai_martube_esp32s3.cc:634–639`）。
- 统一音量入口：
  - 在 `ProcessEncoder` 内只计算目标值，通过 `OnVolumeChange(int)` 统一调用（`ai_martube_esp32s3.cc:696–708`），避免双处更新。
- 备选方案（快速稳定）：
  - 直接复用现有 `Knob` 封装（`main/boards/common/knob.h/.cc`）与其事件库，在本板 `InitializeEncoder` 位置替换为 `Knob`，参考 `sensecap-watcher` 的 `InitializeKnob` 与 `OnKnobRotate`（`main/boards/sensecap-watcher/sensecap_watcher.cc`）。

## 验证步骤
- 单档位旋转 20 次，记录音量变化应严格为每次 ±5 且无正反交替；
- 示波器观测在设定 `DEBOUNCE_MS` 与阈值后，噪声脉冲不触发计步；
- 压力测试：快速旋转 2×、5×速度，确认不丢步、不多步；
- 边界验证：`0` 与 `100` 边界钳制正确（`audio_codec.cc:51–72` 已做钳制）。

## 风险与回滚
- 若移动平均导致响应迟滞，可将 `N` 从 `5` 调回 `3` 或关闭；
- 若 `|pulses|>=3` 阈值对某些编码器过高，调至 `2`；
- 保留原 `ProcessEncoder` 的分支开关，便于线上快速回滚。

如确认以上方案，我将按上述文件位置与行号进行实现与验证，并提供切换为 `Knob` 封装的备选实现以便对比测试。