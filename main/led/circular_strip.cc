#include "circular_strip.h"
#include "application.h"
#include <esp_log.h>

#define TAG "CircularStrip"  // 定义日志标签

#define DEFAULT_BRIGHTNESS 4  // 默认亮度值
#define HIGH_BRIGHTNESS 16    // 高亮度值
#define LOW_BRIGHTNESS 1      // 低亮度值

#define BLINK_INFINITE -1     // 无限闪烁标志

// CircularStrip 构造函数，初始化 LED 灯带
CircularStrip::CircularStrip(gpio_num_t gpio, uint8_t max_leds) : max_leds_(max_leds) {
    // 如果 GPIO 未连接，应使用 NoLed 类
    assert(gpio != GPIO_NUM_NC);

    // 根据最大 LED 数量调整颜色数组大小
    colors_.resize(max_leds_);

    // 配置 LED 灯带
    led_strip_config_t strip_config = {};
    strip_config.strip_gpio_num = gpio;  // 设置 GPIO 引脚
    strip_config.max_leds = max_leds_;   // 设置最大 LED 数量
    strip_config.led_pixel_format = LED_PIXEL_FORMAT_GRB;  // 设置像素格式为 GRB
    strip_config.led_model = LED_MODEL_WS2812;  // 设置 LED 型号为 WS2812

    // 配置 RMT（Remote Control）设备
    led_strip_rmt_config_t rmt_config = {};
    rmt_config.resolution_hz = 10 * 1000 * 1000; // 设置 RMT 分辨率为 10MHz

    // 创建新的 RMT 设备并检查错误
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip_));
    led_strip_clear(led_strip_);  // 清除 LED 灯带

    // 配置定时器，用于周期性调用回调函数
    esp_timer_create_args_t strip_timer_args = {
        .callback = [](void *arg) {
            auto strip = static_cast<CircularStrip*>(arg);
            std::lock_guard<std::mutex> lock(strip->mutex_);
            if (strip->strip_callback_ != nullptr) {
                strip->strip_callback_();  // 调用回调函数
            }
        },
        .arg = this,  // 传递当前对象作为参数
        .dispatch_method = ESP_TIMER_TASK,  // 定时器任务分发方法
        .name = "strip_timer",  // 定时器名称
        .skip_unhandled_events = false,  // 不跳过未处理的事件
    };
    // 创建定时器并检查错误
    ESP_ERROR_CHECK(esp_timer_create(&strip_timer_args, &strip_timer_));
}

// CircularStrip 析构函数，释放资源
CircularStrip::~CircularStrip() {
    esp_timer_stop(strip_timer_);  // 停止定时器
    if (led_strip_ != nullptr) {
        led_strip_del(led_strip_);  // 删除 LED 灯带
    }
}

// 设置静态颜色
void CircularStrip::StaticColor(StripColor color) {
    std::lock_guard<std::mutex> lock(mutex_);  // 加锁
    esp_timer_stop(strip_timer_);  // 停止定时器
    for (int i = 0; i < max_leds_; i++) {
        colors_[i] = color;  // 设置每个 LED 的颜色
        led_strip_set_pixel(led_strip_, i, color.red, color.green, color.blue);  // 设置像素颜色
    }
    led_strip_refresh(led_strip_);  // 刷新 LED 灯带
}

// 设置闪烁效果
void CircularStrip::Blink(StripColor color, int interval_ms) {
    for (int i = 0; i < max_leds_; i++) {
        colors_[i] = color;  // 设置每个 LED 的颜色
    }
    StartStripTask(interval_ms, [this]() {
        static bool on = true;
        if (on) {
            for (int i = 0; i < max_leds_; i++) {
                led_strip_set_pixel(led_strip_, i, colors_[i].red, colors_[i].green, colors_[i].blue);  // 设置像素颜色
            }
            led_strip_refresh(led_strip_);  // 刷新 LED 灯带
        } else {
            led_strip_clear(led_strip_);  // 清除 LED 灯带
        }
        on = !on;  // 切换状态
    });
}

// 设置淡出效果
void CircularStrip::FadeOut(int interval_ms) {
    StartStripTask(interval_ms, [this]() {
        bool all_off = true;
        for (int i = 0; i < max_leds_; i++) {
            colors_[i].red /= 2;  // 红色分量减半
            colors_[i].green /= 2;  // 绿色分量减半
            colors_[i].blue /= 2;  // 蓝色分量减半
            if (colors_[i].red != 0 || colors_[i].green != 0 || colors_[i].blue != 0) {
                all_off = false;  // 如果有 LED 未关闭，标记为未全部关闭
            }
            led_strip_set_pixel(led_strip_, i, colors_[i].red, colors_[i].green, colors_[i].blue);  // 设置像素颜色
        }
        if (all_off) {
            led_strip_clear(led_strip_);  // 如果全部关闭，清除 LED 灯带
            esp_timer_stop(strip_timer_);  // 停止定时器
        } else {
            led_strip_refresh(led_strip_);  // 刷新 LED 灯带
        }
    });
}

// 设置呼吸效果
void CircularStrip::Breathe(StripColor low, StripColor high, int interval_ms) {
    StartStripTask(interval_ms, [this, low, high]() {
        static bool increase = true;
        static StripColor color = low;
        if (increase) {
            if (color.red < high.red) {
                color.red++;  // 红色分量增加
            }
            if (color.green < high.green) {
                color.green++;  // 绿色分量增加
            }
            if (color.blue < high.blue) {
                color.blue++;  // 蓝色分量增加
            }
            if (color.red == high.red && color.green == high.green && color.blue == high.blue) {
                increase = false;  // 达到高亮度后，开始减少
            }
        } else {
            if (color.red > low.red) {
                color.red--;  // 红色分量减少
            }
            if (color.green > low.green) {
                color.green--;  // 绿色分量减少
            }
            if (color.blue > low.blue) {
                color.blue--;  // 蓝色分量减少
            }
            if (color.red == low.red && color.green == low.green && color.blue == low.blue) {
                increase = true;  // 达到低亮度后，开始增加
            }
        }
        for (int i = 0; i < max_leds_; i++) {
            led_strip_set_pixel(led_strip_, i, color.red, color.green, color.blue);  // 设置像素颜色
        }
        led_strip_refresh(led_strip_);  // 刷新 LED 灯带
    });
}

// 设置滚动效果
void CircularStrip::Scroll(StripColor low, StripColor high, int length, int interval_ms) {
    for (int i = 0; i < max_leds_; i++) {
        colors_[i] = low;  // 设置每个 LED 的初始颜色
    }
    StartStripTask(interval_ms, [this, low, high, length]() {
        static int offset = 0;
        for (int i = 0; i < max_leds_; i++) {
            colors_[i] = low;  // 重置每个 LED 的颜色
        }
        for (int j = 0; j < length; j++) {
            int i = (offset + j) % max_leds_;
            colors_[i] = high;  // 设置滚动的高亮部分
        }
        for (int i = 0; i < max_leds_; i++) {
            led_strip_set_pixel(led_strip_, i, colors_[i].red, colors_[i].green, colors_[i].blue);  // 设置像素颜色
        }
        led_strip_refresh(led_strip_);  // 刷新 LED 灯带
        offset = (offset + 1) % max_leds_;  // 更新偏移量
    });
}

// 启动 LED 灯带任务
void CircularStrip::StartStripTask(int interval_ms, std::function<void()> cb) {
    if (led_strip_ == nullptr) {
        return;  // 如果 LED 灯带未初始化，直接返回
    }

    std::lock_guard<std::mutex> lock(mutex_);  // 加锁
    esp_timer_stop(strip_timer_);  // 停止定时器
    
    strip_callback_ = cb;  // 设置回调函数
    esp_timer_start_periodic(strip_timer_, interval_ms * 1000);  // 启动周期性定时器
}

// 设备状态变化时的处理函数
void CircularStrip::OnStateChanged() {
    auto& app = Application::GetInstance();
    auto device_state = app.GetDeviceState();
    switch (device_state) {
        case kDeviceStateStarting: {
            StripColor low = { 0, 0, 0 };
            StripColor high = { LOW_BRIGHTNESS, LOW_BRIGHTNESS, DEFAULT_BRIGHTNESS };
            Scroll(low, high, 3, 100);  // 设备启动时，显示滚动效果
            break;
        }
        case kDeviceStateWifiConfiguring: {
            StripColor color = { LOW_BRIGHTNESS, LOW_BRIGHTNESS, DEFAULT_BRIGHTNESS };
            Blink(color, 500);  // WiFi 配置时，显示闪烁效果
            break;
        }
        case kDeviceStateIdle:
            FadeOut(50);  // 设备空闲时，显示淡出效果
            break;
        case kDeviceStateConnecting: {
            StripColor color = { LOW_BRIGHTNESS, LOW_BRIGHTNESS, DEFAULT_BRIGHTNESS };
            StaticColor(color);  // 设备连接时，显示静态颜色
            break;
        }
        case kDeviceStateListening: {
            StripColor color = { DEFAULT_BRIGHTNESS, LOW_BRIGHTNESS, LOW_BRIGHTNESS };
            StaticColor(color);  // 设备监听时，显示静态颜色
            break;
        }
        case kDeviceStateSpeaking: {
            StripColor color = { LOW_BRIGHTNESS, DEFAULT_BRIGHTNESS, LOW_BRIGHTNESS };
            StaticColor(color);  // 设备说话时，显示静态颜色
            break;
        }
        case kDeviceStateUpgrading: {
            StripColor color = { LOW_BRIGHTNESS, DEFAULT_BRIGHTNESS, LOW_BRIGHTNESS };
            Blink(color, 100);  // 设备升级时，显示快速闪烁效果
            break;
        }
        case kDeviceStateActivating: {
            StripColor color = { LOW_BRIGHTNESS, DEFAULT_BRIGHTNESS, LOW_BRIGHTNESS };
            Blink(color, 500);  // 设备激活时，显示闪烁效果
            break;
        }
        default:
            ESP_LOGW(TAG, "Unknown led strip event: %d", device_state);  // 未知状态，记录警告日志
            return;
    }
}