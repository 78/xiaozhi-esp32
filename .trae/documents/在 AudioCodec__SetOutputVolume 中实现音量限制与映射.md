## 背景与目标
- 在 `audio_codec.cc` 的 `AudioCodec::SetOutputVolume(int volume)` 中加入输入参数验证与范围限制。
- 将外部输入的 0–100 UI 音量映射到硬件安全范围 30–80（第2条“对应30–80”作为准则）。如需调整为 20–80，可在确认后改为常量映射。
- 保持其他音频处理流程不受影响，仅改动该函数；增加清晰日志与函数注释。

## 实施方案
- 输入校验：对传入的 `volume` 做边界检查，先将其钳制到 `[0, 100]`，并使用警告日志记录越界情况。
- 线性映射：将校验后的 `volume` 线性映射到 `[30, 80]`，公式：`mapped = 30 + (volume * 50) / 100`。
- 状态更新：`output_volume_` 保存为映射后的有效音量，并持久化到 `Settings("audio", true)`。
- 日志输出：记录 `requested`、`clamped`、`effective` 三个值，便于排查与追踪；正常设置使用信息日志，越界使用警告日志。
- 注释说明：在函数前添加注释，明确“输入 0–100，映射到 30–80，越界将被钳制”的规则与不影响音频质量的设计意图。

## 代码改动（拟）
```cpp
/**
 * 设置输出音量。
 * 输入为 UI 规模的 0–100；将其线性映射至硬件安全范围 30–80。
 * 超出 [0,100] 的输入将被钳制到边界，并记录日志；该限制不影响其他音频处理与音质。
 */
void AudioCodec::SetOutputVolume(int volume) {
    int requested = volume;

    // 1) 输入校验：先钳制到 0–100
    if (volume < 0) {
        ESP_LOGW(TAG, "Requested volume %d out of range (<0), clamped to 0", requested);
        volume = 0;
    } else if (volume > 100) {
        ESP_LOGW(TAG, "Requested volume %d out of range (>100), clamped to 100", requested);
        volume = 100;
    }

    // 2) 线性映射到 30–80
    int mapped = 30 + (volume * 50) / 100; // 0→30, 100→80

    // 3) 记录设置与调整情况
    ESP_LOGI(TAG, "Set output volume: requested=%d, clamped=%d, effective=%d", requested, volume, mapped);

    // 4) 更新状态并持久化
    output_volume_ = mapped;
    Settings settings("audio", true);
    settings.SetInt("output_volume", output_volume_);
}
```

## 日志与质量
- 使用 `ESP_LOGW` 记录越界钳制；使用 `ESP_LOGI` 记录最终有效音量，方便现场排查。
- 逻辑仅限于 setter，不改动采集/播放/编码/解码路径，确保其他功能与音质不受影响。

## 验证
- 单元级：调用 `SetOutputVolume` 分别传入 `-5, 0, 15, 50, 100, 130` 检查日志与 `output_volume_` 结果是否对应 `30, 30, 37, 55, 80, 80`。
- 运行级：启动后通过现有设置流程读取/存储音量，确认日志输出与音量行为符合预期。