// beat_sync.h — 随音乐摇摆：麦克风采音 → BPM 检测 → 对齐节拍摇摆
//
// 功能：
//   - 从板载 I2S MEMS 麦克风采集环境音频
//   - 自相关算法检测 BPM（60~180 范围）
//   - 随机选取 4 个基础摇摆动作（FB/LR/Twist/UpDown）
//   - 每个动作对齐 2 拍，自动计算 half_ms
//   - 全本地计算，无需网络/小程序依赖（但触发可通过小程序按钮）

#ifndef BEAT_SYNC_H
#define BEAT_SYNC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 执行完整流程：采音 → 测 BPM → 随机四动作摇摆
// 返回 0 成功，-1 失败（如环境太安静）
int beat_sync_run(void);

// 仅检测 BPM（不执行动作），返回 BPM 值，失败返回 -1
int beat_sync_detect_bpm(void);

#ifdef __cplusplus
}
#endif

#endif // BEAT_SYNC_H
