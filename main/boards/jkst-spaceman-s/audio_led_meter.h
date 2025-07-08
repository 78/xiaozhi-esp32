#pragma once
#include <cstdint>
#include <cstddef>
#include "led_strip.h"
#include "config.h"

#include <vector>
#include <tuple>

// 你需要在主控初始化时设置这个灯带句柄
void audio_led_meter_set_strip(void* led_strip);

// 传入PCM数据，自动分析音量并刷新灯带
void audio_led_meter_update(const int16_t* pcm, size_t samples);

// 新增：开关控制
void audio_led_meter_enable(int enable);

void audio_led_meter_set_brightness(int percent);
void audio_led_meter_set_colors(const std::vector<std::tuple<uint8_t, uint8_t, uint8_t>> &colors);
void audio_led_meter_init_colors();
void audio_led_meter_set_single_color(uint8_t r, uint8_t g, uint8_t b);