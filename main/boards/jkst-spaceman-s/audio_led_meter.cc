#include "audio_led_meter.h"
#include <mutex>
#include <cstdint>
#include "led_strip.h"
#include "esp_log.h"

// #define WS2812_LED_NUM_USED 5

static led_strip_handle_t g_led_strip = nullptr;
static std::mutex g_led_mutex;

static int g_led_meter_enabled = 0; // 开启关闭控制，默认关闭

extern uint8_t g_color_r, g_color_g, g_color_b; // 在 ws2812_controller.cc 里定义并赋值

void audio_led_meter_enable(int enable)
{
    g_led_meter_enabled = enable;
}

void audio_led_meter_set_strip(void* led_strip) {
    g_led_strip = reinterpret_cast<led_strip_handle_t>(led_strip);
}

void audio_led_meter_update(const int16_t* pcm, size_t samples) {
    if (!g_led_meter_enabled)
    return;

    if (!g_led_strip)
    return;
    // ESP_LOGI("AudioLedMeter", "Updating LED meter with %zu samples", samples);

    int64_t sum = 0;
    for (size_t i = 0; i < samples; ++i) {
        sum += abs(pcm[i]);
    }
    int avg = sum / samples;
    int level = avg / 1000;
    if (level > WS2812_LED_NUM_USED) level = WS2812_LED_NUM_USED;

    std::lock_guard<std::mutex> lock(g_led_mutex);
    for (int i = 0; i < WS2812_LED_NUM_USED; i++) {
        if (i < level)
            led_strip_set_pixel(g_led_strip, i, g_color_r, g_color_g, g_color_b); // 用全局色
        else
            led_strip_set_pixel(g_led_strip, i, 0, 0, 0);
    }

    for (int i = WS2812_LED_NUM_USED; i < WS2812_LED_NUM; i++)
    {
        led_strip_set_pixel(g_led_strip, i, 0, 0, 0);
    }
    led_strip_refresh(g_led_strip);
}