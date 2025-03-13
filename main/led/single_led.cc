#include "single_led.h"
#include "application.h"
#include <esp_log.h> 

#define TAG "SingleLed"  // 定义日志标签

#define DEFAULT_BRIGHTNESS 4  // 默认亮度值
#define HIGH_BRIGHTNESS 16    // 高亮度值
#define LOW_BRIGHTNESS 2      // 低亮度值

#define BLINK_INFINITE -1     // 无限闪烁标志

// SingleLed 构造函数，初始化单个 LED
SingleLed::SingleLed(gpio_num_t gpio) {
    // 如果 GPIO 未连接，应使用 NoLed 类
    assert(gpio != GPIO_NUM_NC);

    // 配置 LED 灯带
    led_strip_config_t strip_config = {};
    strip_config.strip_gpio_num = gpio;  // 设置 GPIO 引脚
    strip_config.max_leds = 1;           // 设置最大 LED 数量为 1
    strip_config.led_pixel_format = LED_PIXEL_FORMAT_GRB;  // 设置像素格式为 GRB
    strip_config.led_model = LED_MODEL_WS2812;  // 设置 LED 型号为 WS2812

    // 配置 RMT（Remote Control）设备
    led_strip_rmt_config_t rmt_config = {};
    rmt_config.resolution_hz = 10 * 1000 * 1000; // 设置 RMT 分辨率为 10MHz

    // 创建新的 RMT 设备并检查错误
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip_));
    led_strip_clear(led_strip_);  // 清除 LED 灯带

    // 配置定时器，用于周期性调用回调函数
    esp_timer_create_args_t blink_timer_args = {
        .callback = [](void *arg) {
            auto led = static_cast<SingleLed*>(arg);
            led->OnBlinkTimer();  // 调用闪烁定时器回调函数
        },
        .arg = this,  // 传递当前对象作为参数
        .dispatch_method = ESP_TIMER_TASK,  // 定时器任务分发方法
        .name = "blink_timer",  // 定时器名称
        .skip_unhandled_events = false,  // 不跳过未处理的事件
    };
    // 创建定时器并检查错误
    ESP_ERROR_CHECK(esp_timer_create(&blink_timer_args, &blink_timer_));
}

// SingleLed 析构函数，释放资源
SingleLed::~SingleLed() {
    esp_timer_stop(blink_timer_);  // 停止定时器
    if (led_strip_ != nullptr) {
        led_strip_del(led_strip_);  // 删除 LED 灯带
    }
}

// 设置 LED 颜色
void SingleLed::SetColor(uint8_t r, uint8_t g, uint8_t b) {
    r_ = r;  // 设置红色分量
    g_ = g;  // 设置绿色分量
    b_ = b;  // 设置蓝色分量
}

// 打开 LED
void SingleLed::TurnOn() {
    if (led_strip_ == nullptr) {
        return;  // 如果 LED 灯带未初始化，直接返回
    }
    
    std::lock_guard<std::mutex> lock(mutex_);  // 加锁
    esp_timer_stop(blink_timer_);  // 停止定时器
    led_strip_set_pixel(led_strip_, 0, r_, g_, b_);  // 设置 LED 颜色
    led_strip_refresh(led_strip_);  // 刷新 LED 灯带
}

// 关闭 LED
void SingleLed::TurnOff() {
    if (led_strip_ == nullptr) {
        return;  // 如果 LED 灯带未初始化，直接返回
    }

    std::lock_guard<std::mutex> lock(mutex_);  // 加锁
    esp_timer_stop(blink_timer_);  // 停止定时器
    led_strip_clear(led_strip_);  // 清除 LED 灯带
}

// 闪烁一次
void SingleLed::BlinkOnce() {
    Blink(1, 100);  // 闪烁一次，间隔 100ms
}

// 闪烁指定次数
void SingleLed::Blink(int times, int interval_ms) {
    StartBlinkTask(times, interval_ms);  // 启动闪烁任务
}

// 启动持续闪烁
void SingleLed::StartContinuousBlink(int interval_ms) {
    StartBlinkTask(BLINK_INFINITE, interval_ms);  // 启动无限闪烁任务
}

// 启动闪烁任务
void SingleLed::StartBlinkTask(int times, int interval_ms) {
    if (led_strip_ == nullptr) {
        return;  // 如果 LED 灯带未初始化，直接返回
    }

    std::lock_guard<std::mutex> lock(mutex_);  // 加锁
    esp_timer_stop(blink_timer_);  // 停止定时器
    
    blink_counter_ = times * 2;  // 设置闪烁计数器
    blink_interval_ms_ = interval_ms;  // 设置闪烁间隔
    esp_timer_start_periodic(blink_timer_, interval_ms * 1000);  // 启动周期性定时器
}

// 闪烁定时器回调函数
void SingleLed::OnBlinkTimer() {
    std::lock_guard<std::mutex> lock(mutex_);  // 加锁
    blink_counter_--;  // 减少闪烁计数器
    if (blink_counter_ & 1) {
        led_strip_set_pixel(led_strip_, 0, r_, g_, b_);  // 设置 LED 颜色
        led_strip_refresh(led_strip_);  // 刷新 LED 灯带
    } else {
        led_strip_clear(led_strip_);  // 清除 LED 灯带

        if (blink_counter_ == 0) {
            esp_timer_stop(blink_timer_);  // 如果闪烁完成，停止定时器
        }
    }
}

// 设备状态变化时的处理函数
void SingleLed::OnStateChanged() {
    auto& app = Application::GetInstance();
    auto device_state = app.GetDeviceState();
    switch (device_state) {
        case kDeviceStateStarting:
            SetColor(0, 0, DEFAULT_BRIGHTNESS);  // 设备启动时，设置颜色为蓝色
            StartContinuousBlink(100);  // 启动持续闪烁，间隔 100ms
            break;
        case kDeviceStateWifiConfiguring:
            SetColor(0, 0, DEFAULT_BRIGHTNESS);  // WiFi 配置时，设置颜色为蓝色
            StartContinuousBlink(500);  // 启动持续闪烁，间隔 500ms
            break;
        case kDeviceStateIdle:
            TurnOff();  // 设备空闲时，关闭 LED
            break;
        case kDeviceStateConnecting:
            SetColor(0, 0, DEFAULT_BRIGHTNESS);  // 设备连接时，设置颜色为蓝色
            TurnOn();  // 打开 LED
            break;
        case kDeviceStateListening:
            if (app.IsVoiceDetected()) {
                SetColor(HIGH_BRIGHTNESS, 0, 0);  // 检测到语音时，设置颜色为高亮红色
            } else {
                SetColor(LOW_BRIGHTNESS, 0, 0);  // 未检测到语音时，设置颜色为低亮红色
            }
            TurnOn();  // 打开 LED
            break;
        case kDeviceStateSpeaking:
            SetColor(0, DEFAULT_BRIGHTNESS, 0);  // 设备说话时，设置颜色为绿色
            TurnOn();  // 打开 LED
            break;
        case kDeviceStateUpgrading:
            SetColor(0, DEFAULT_BRIGHTNESS, 0);  // 设备升级时，设置颜色为绿色
            StartContinuousBlink(100);  // 启动持续闪烁，间隔 100ms
            break;
        case kDeviceStateActivating:
            SetColor(0, DEFAULT_BRIGHTNESS, 0);  // 设备激活时，设置颜色为绿色
            StartContinuousBlink(500);  // 启动持续闪烁，间隔 500ms
            break;
        default:
            ESP_LOGW(TAG, "Unknown led strip event: %d", device_state);  // 未知状态，记录警告日志
            return;
    }
}