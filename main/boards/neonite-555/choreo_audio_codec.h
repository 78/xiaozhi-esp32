#ifndef _CHOREO_AUDIO_CODEC_H_
#define _CHOREO_AUDIO_CODEC_H_

#include "codecs/no_audio_codec.h"
#include <atomic>

/* 板级派生 codec：choreo 独占输出期间拦截 power timer 的误判性 EnableOutput(false)。
 *
 * 背景：choreo 舞蹈播放绕过 AudioService::AudioOutputTask 直写 codec->OutputData()，
 * 导致 AudioService 的 last_output_time_ 不刷新，15s 后 CheckAndUpdateAudioPowerState()
 * 误判超时调用 codec_->EnableOutput(false) 关闭 I2S tx 通道，造成舞曲中断/爆音。
 *
 * 原方案修改官方 audio_service 添加 TouchOutputTime()，PR 不合规。
 * 本方案改为在板级派生 codec 子类，override EnableOutput 拦截误判关闭，
 * 全部改动限制在板级目录内，不改官方任何文件。
 *
 * 仅用于 SIMPLEX 分支（Duplex 是另一套硬件，不在 PR 范围）。 */
class ChoreoAudioCodecSimplex : public NoAudioCodecSimplex {
public:
    using NoAudioCodecSimplex::NoAudioCodecSimplex;

    /* choreo 开始/结束时由 choreo_audio_ctrl_bridge 调用，置位活动标志。
     * 必须在 EnableOutput() 调用之前设置，以保证时序正确：
     *   ctrl(true)  -> SetChoreoActive(true)  -> EnableOutput(true)   放行开启
     *   ctrl(false) -> SetChoreoActive(false) -> EnableOutput(false)  放行 choreo 合法关闭 */
    void SetChoreoActive(bool active) { choreo_active_.store(active); }

    /* override NoAudioCodec::EnableOutput：
     * - choreo 活动期间，吞掉 power timer 触发的误判性关闭（!enable && output_enabled_）
     * - 非活动期间或 EnableOutput(true) 一律放行，保留官方省电逻辑 */
    void EnableOutput(bool enable) override {
        if (choreo_active_.load() && !enable && output_enabled_) {
            return;
        }
        NoAudioCodecSimplex::EnableOutput(enable);
    }

private:
    std::atomic<bool> choreo_active_{false};
};

#endif // _CHOREO_AUDIO_CODEC_H_
