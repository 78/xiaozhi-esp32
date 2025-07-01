#pragma once
#include <cstdint>
#include <cstddef>
#include "led_strip.h"
#include "config.h"
// 你需要在主控初始化时设置这个灯带句柄
void audio_led_meter_set_strip(void* led_strip);

// 传入PCM数据，自动分析音量并刷新灯带
void audio_led_meter_update(const int16_t* pcm, size_t samples);

// 新增：开关控制
void audio_led_meter_enable(int enable);