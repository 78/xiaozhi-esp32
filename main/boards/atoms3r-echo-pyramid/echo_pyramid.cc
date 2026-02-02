#include "echo_pyramid.h"
#include "i2c_device.h"
#include <esp_log.h>
#include <math.h>

#define TAG "EchoPyramid"

// STM32 寄存器地址
#define REG_TOUCH1_STATUS       (0x00)
#define REG_TOUCH2_STATUS       (0x01)
#define REG_TOUCH3_STATUS       (0x02)
#define REG_TOUCH4_STATUS       (0x03)

#define REG_RGB1_BRIGHTNESS     (0x10)
#define REG_RGB2_BRIGHTNESS     (0x11)
#define REG_RGB_CH1_I1_COLOR    (0x20)
#define REG_RGB_CH2_I1_COLOR    (0x3C)
#define REG_RGB_CH3_I1_COLOR    (0x60)
#define REG_RGB_CH4_I1_COLOR    (0x7C)

#define NUM_RGB_STRIPS          (2)
#define NUM_LEDS_PER_STRIP      (14)
#define NUM_GROUPS_PER_STRIP    (2)
#define NUM_LEDS_PER_GROUP      (7)
#define NUM_RGB_CHANNELS        (4)

/**
 * STM32 RGB灯带控制器实现
 */
class EchoPyramid::Stm32Impl : public I2cDevice {
public:
    Stm32Impl(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
        uint8_t firmware_version = ReadReg(0xFE);
        ESP_LOGI(TAG, "Init Stm32 firmware version: 0x%02X", firmware_version);

        ESP_LOGI(TAG, "AW87559 reset");
        WriteReg(0xA0, 0x01);
        vTaskDelay(pdMS_TO_TICKS(100));
        WriteReg(REG_RGB1_BRIGHTNESS, 30);
        WriteReg(REG_RGB2_BRIGHTNESS, 30);
        clearAllRgb();
        setEffectMode(LightMode::BREATHE);
        startEffectTask();
    }

    ~Stm32Impl() {
        stopEffectTask();
    }

    void setRgbColor(uint8_t channel, uint8_t index, uint32_t color) {
        if (channel >= NUM_RGB_CHANNELS || index >= NUM_LEDS_PER_GROUP) return;
        setRgbColorInternal(channel, index, color);
    }

    void setRgbChannelColor(uint8_t channel, uint32_t color) {
        if (channel >= NUM_RGB_CHANNELS) return;
        for (uint8_t i = 0; i < NUM_LEDS_PER_GROUP; i++) {
            setRgbColorInternal(channel, i, color);
        }
    }

    void setAllRgbColor(uint32_t color) {
        for (uint8_t c = 0; c < NUM_RGB_CHANNELS; c++) {
            setRgbChannelColor(c, color);
        }
    }

    void clearAllRgb() {
        setAllRgbColor(0x00000000);
    }

    void setRgbStripBrightness(uint8_t strip, uint8_t brightness) {
        if (strip >= NUM_RGB_STRIPS) return;
        if (brightness > 100) brightness = 100;
        uint8_t reg_addr = (strip == 0) ? REG_RGB1_BRIGHTNESS : REG_RGB2_BRIGHTNESS;
        WriteReg(reg_addr, brightness);
    }

    void setEffectMode(LightMode mode) {
        effect_mode_ = mode;
    }

    LightMode getEffectMode() const {
        return effect_mode_.load();
    }

    void setEffectColor(uint32_t color) {
        effect_color_ = color;
    }

    bool readTouchStatus(uint8_t touch_num) {
        if (touch_num < 1 || touch_num > 4) return false;
        uint8_t reg_addr;
        switch (touch_num) {
            case 1: reg_addr = REG_TOUCH1_STATUS; break;
            case 2: reg_addr = REG_TOUCH2_STATUS; break;
            case 3: reg_addr = REG_TOUCH3_STATUS; break;
            case 4: reg_addr = REG_TOUCH4_STATUS; break;
            default: return false;
        }
        uint8_t buffer[1];
        esp_err_t ret = i2c_master_transmit_receive(i2c_device_, &reg_addr, 1, buffer, 1, 300);
        if (ret != ESP_OK) return false;
        return (buffer[0] & 0x01) != 0;
    }

    void startEffectTask() {
        if (effect_task_handle_ == nullptr) {
            xTaskCreate(RgbEffectTask, "rgb_effect", 8192, this, 4, &effect_task_handle_);
            ESP_LOGI(TAG, "RGB effect task started");
        }
    }

    void stopEffectTask() {
        if (effect_task_handle_ != nullptr) {
            effect_running_ = false;
            vTaskDelay(pdMS_TO_TICKS(100));
            if (effect_task_handle_ != nullptr) {
                vTaskDelete(effect_task_handle_);
                effect_task_handle_ = nullptr;
            }
            ESP_LOGI(TAG, "RGB effect task stopped");
        }
    }

private:
    TaskHandle_t effect_task_handle_ = nullptr;
    std::atomic<bool> effect_running_{true};
    std::atomic<LightMode> effect_mode_{LightMode::BREATHE};
    uint32_t effect_color_ = 0x000000FF;
    int effect_speed_ = 10;

    void setRgbColorInternal(uint8_t channel, uint8_t index, uint32_t color) {
        uint8_t hardware_index = index;
        if (channel == 0 || channel == 1) {
            hardware_index = NUM_LEDS_PER_GROUP - 1 - index;
        }

        uint8_t reg_addr;
        switch (channel) {
            case 0: reg_addr = REG_RGB_CH1_I1_COLOR + hardware_index * 4; break;
            case 1: reg_addr = REG_RGB_CH2_I1_COLOR + hardware_index * 4; break;
            case 2: reg_addr = REG_RGB_CH4_I1_COLOR + hardware_index * 4; break;
            case 3: reg_addr = REG_RGB_CH3_I1_COLOR + hardware_index * 4; break;
            default: return;
        }
        WriteRegs(reg_addr, (uint8_t*)&color, sizeof(color));
    }

    static void RgbEffectTask(void* arg) {
        Stm32Impl* stm32 = static_cast<Stm32Impl*>(arg);
        stm32->EffectTaskLoop();
        vTaskDelete(NULL);
    }

    void EffectTaskLoop() {
        int position = 0;
        int breathe_step = 0;
        float hue = 0.0f;

        while (effect_running_) {
            switch (effect_mode_.load()) {
                case LightMode::OFF:
                    clearAllRgb();
                    vTaskDelay(pdMS_TO_TICKS(100));
                    break;

                case LightMode::STATIC:
                    setAllRgbColor(effect_color_);
                    vTaskDelay(pdMS_TO_TICKS(100));
                    break;

                case LightMode::BREATHE: {
                    float brightness = (sin(breathe_step * 0.1f) + 1.0f) / 2.0f;
                    uint8_t r = ((effect_color_ >> 16) & 0xFF) * brightness;
                    uint8_t g = ((effect_color_ >> 8) & 0xFF) * brightness;
                    uint8_t b = (effect_color_ & 0xFF) * brightness;
                    uint32_t color = (r << 16) | (g << 8) | b;
                    setAllRgbColor(color);
                    breathe_step++;
                    if (breathe_step > 1000) breathe_step = 0;
                    vTaskDelay(pdMS_TO_TICKS(60));
                    break;
                }

                case LightMode::RAINBOW: {
                    for (int c = 0; c < NUM_RGB_CHANNELS; c++) {
                        for (int i = 0; i < NUM_LEDS_PER_GROUP; i++) {
                            int led_pos = c * NUM_LEDS_PER_GROUP + i;
                            float led_hue = (hue + led_pos * 15.0f) / 360.0f;
                            uint32_t color = HsvToRgb(led_hue, 1.0f, 1.0f);
                            setRgbColorInternal(c, i, color);
                        }
                    }
                    hue += 2.0f;
                    if (hue >= 360.0f) hue = 0.0f;
                    vTaskDelay(pdMS_TO_TICKS(effect_speed_));
                    break;
                }

                case LightMode::CHASE: {
                    clearAllRgb();
                    int total_leds = NUM_RGB_CHANNELS * NUM_LEDS_PER_GROUP;
                    int led_pos = position % total_leds;
                    int channel = led_pos / NUM_LEDS_PER_GROUP;
                    int led_index = led_pos % NUM_LEDS_PER_GROUP;
                    setRgbColorInternal(channel, led_index, effect_color_);
                    position = (position + 1) % total_leds;
                    vTaskDelay(pdMS_TO_TICKS(50));
                    break;
                }

                default:
                    vTaskDelay(pdMS_TO_TICKS(100));
                    break;
            }
        }
    }

    uint32_t HsvToRgb(float h, float s, float v) {
        int i = (int)(h * 6.0f);
        float f = h * 6.0f - i;
        float p = v * (1.0f - s);
        float q = v * (1.0f - f * s);
        float t = v * (1.0f - (1.0f - f) * s);

        float r, g, b;
        switch (i % 6) {
            case 0: r = v; g = t; b = p; break;
            case 1: r = q; g = v; b = p; break;
            case 2: r = p; g = v; b = t; break;
            case 3: r = p; g = q; b = v; break;
            case 4: r = t; g = p; b = v; break;
            case 5: r = v; g = p; b = q; break;
            default: r = g = b = 0; break;
        }

        return ((uint8_t)(r * 255) << 16) | ((uint8_t)(g * 255) << 8) | (uint8_t)(b * 255);
    }
};

// EchoPyramid 实现

EchoPyramid::EchoPyramid(i2c_master_bus_handle_t i2c_bus, uint8_t stm32_addr) {
    stm32_ = new Stm32Impl(i2c_bus, stm32_addr);
}

EchoPyramid::~EchoPyramid() {
    stopTouchDetection();
    if (stm32_) {
        delete stm32_;
        stm32_ = nullptr;
    }
}

/**
 * Si5351 Clock Generator
 * I2C Address: 0x60
 * Reference: https://www.skyworksinc.com/-/media/Skyworks/SL/documents/public/application-notes/AN619.pdf
 */
Si5351::Si5351(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
    uint8_t w_buffer[10] = {0};

    // Disable all outputs
    WriteReg(3, 0xff);
    ESP_LOGI(TAG, "Si5351 Register 3 (OUTPUT_ENABLE_CONTROL): %02X", ReadReg(3));

    // Power down output drivers
    w_buffer[0] = 0x80;  // 10000000
    w_buffer[1] = 0x80;
    w_buffer[2] = 0x80;
    WriteRegs(16, w_buffer, 3);
    ESP_LOGI(TAG, "Si5351 Registers 16-23 configured");

    // Crystal Internal Load Capacitance
    WriteReg(183, 0xC0);  // 11000000 // Internal CL = 10 pF (default)
    ESP_LOGI(TAG, "Si5351 Register 183 (CRYSTAL_LOAD): %02X", ReadReg(183));

    // Multisynth NA Parameters
    w_buffer[0] = 0xFF;
    w_buffer[1] = 0xFD;
    w_buffer[2] = 0x00;
    w_buffer[3] = 0x09;
    w_buffer[4] = 0x26;
    w_buffer[5] = 0xF7;
    w_buffer[6] = 0x4F;
    w_buffer[7] = 0x72;
    WriteRegs(26, w_buffer, 8);
    ESP_LOGI(TAG, "Si5351 Registers 26-33 (Multisynth NA) configured");

    // Multisynth1 Parameters
    w_buffer[0] = 0x00;
    w_buffer[1] = 0x01;
    w_buffer[2] = 0x00;
    w_buffer[3] = 0x2F;
    w_buffer[4] = 0x00;
    w_buffer[5] = 0x00;
    w_buffer[6] = 0x00;
    w_buffer[7] = 0x00;
    WriteRegs(50, w_buffer, 8);
    ESP_LOGI(TAG, "Si5351 Registers 50-57 (Multisynth1) configured");

    // CLK1 Control
    // Bit 6: MS1 operates in integer mode
    // Bits 5-4: Select MultiSynth 1 as the source for CLK1
    WriteReg(17, ((3 << 2) | (1 << 6)));  // 01001100 = 0x4C
    ESP_LOGI(TAG, "Si5351 Register 17 (CLK1_CONTROL): %02X", ReadReg(17));

    // PLL Reset
    WriteReg(177, 0xA0);  // 10100000
    ESP_LOGI(TAG, "Si5351 Register 177 (PLL_RESET): %02X", ReadReg(177));

    // Enable all outputs
    WriteReg(3, 0x00);
    ESP_LOGI(TAG, "Si5351 Register 3 (OUTPUT_ENABLE_CONTROL): %02X - outputs enabled", ReadReg(3));
}

/**
 * AW87559 Audio Amplifier
 * I2C Address: 0x5B
 */
Aw87559::Aw87559(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
    ESP_LOGI(TAG, "AW87559 ID: %02X", ReadReg(0x00)); // ID: 0x5A
    WriteReg(0x01, 0x78); // default enable PA
}

void Aw87559::SetSpeaker(bool enable) {
    WriteReg(0x01, enable ? 0x78 : 0x30); // BIT3: PA Enable
}

void EchoPyramid::addTouchEventCallback(TouchEventCallback callback) {
    touch_callbacks_.push_back(callback);
}

void EchoPyramid::clearTouchEventCallbacks() {
    touch_callbacks_.clear();
}

void EchoPyramid::setLightMode(LightMode mode) {
    if (stm32_) {
        stm32_->setEffectMode(mode);
    }
}

LightMode EchoPyramid::getLightMode() const {
    if (stm32_) {
        return stm32_->getEffectMode();
    }
    return LightMode::OFF;
}

void EchoPyramid::setLightColor(uint32_t color) {
    if (stm32_) {
        stm32_->setEffectColor(color);
    }
}

void EchoPyramid::setLightBrightness(uint8_t strip, uint8_t brightness) {
    if (stm32_) {
        stm32_->setRgbStripBrightness(strip, brightness);
    }
}

void EchoPyramid::startTouchDetection() {
    if (touch_task_handle_ == nullptr) {
        xTaskCreate(TouchTask, "touch_task", 8192, this, 3, &touch_task_handle_);
        ESP_LOGI(TAG, "Touch task started");
    }
}

void EchoPyramid::stopTouchDetection() {
    if (touch_task_handle_ != nullptr) {
        vTaskDelete(touch_task_handle_);
        touch_task_handle_ = nullptr;
        ESP_LOGI(TAG, "Touch task stopped");
    }
}

void EchoPyramid::pauseTouchCallbacks() {
    touch_callbacks_paused_ = true;
}

void EchoPyramid::resumeTouchCallbacks() {
    touch_callbacks_paused_ = false;
}

bool EchoPyramid::isTouchCallbacksPaused() const {
    return touch_callbacks_paused_.load();
}

void EchoPyramid::TouchTask(void* arg) {
    EchoPyramid* pyramid = static_cast<EchoPyramid*>(arg);
    
    while (true) {
        if (pyramid->stm32_ == nullptr) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

        // 读取所有触摸状态
        bool touch_states[4] = {
            pyramid->stm32_->readTouchStatus(1),
            pyramid->stm32_->readTouchStatus(2),
            pyramid->stm32_->readTouchStatus(3),
            pyramid->stm32_->readTouchStatus(4)
        };

        // 在检测到触摸状态变化时打印触摸状态
        if (touch_states[0] != pyramid->touch_last_state_[0] ||
            touch_states[1] != pyramid->touch_last_state_[1] ||
            touch_states[2] != pyramid->touch_last_state_[2] ||
            touch_states[3] != pyramid->touch_last_state_[3]) {
            ESP_LOGI(TAG, "Touch states: %d, %d, %d, %d", 
                     touch_states[0], touch_states[1], touch_states[2], touch_states[3]);
        }

        // 处理Touch1/2的滑动检测
        pyramid->ProcessSwipe(touch_states[0], touch_states[1],
                             pyramid->touch_last_state_[0], pyramid->touch_last_state_[1],
                             0, 1, 2, current_time);

        // 处理Touch3/4的滑动检测
        pyramid->ProcessSwipe(touch_states[2], touch_states[3],
                             pyramid->touch_last_state_[2], pyramid->touch_last_state_[3],
                             1, 3, 4, current_time);

        // 更新状态
        for (int i = 0; i < 4; i++) {
            pyramid->touch_last_state_[i] = touch_states[i];
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }

    vTaskDelete(NULL);
}

void EchoPyramid::ProcessSwipe(bool touch1, bool touch2, uint8_t& last_state1, uint8_t& last_state2,
                               uint8_t swipe_index, uint8_t touch_num1, uint8_t touch_num2, uint32_t current_time) {
    // 检测按下事件
    if ((touch1 && !last_state1) || (touch2 && !last_state2)) {
        if (touch_swipe_first_[swipe_index] == 0) {
            // 记录第一个按下的按键
            if (touch1 && !last_state1) {
                touch_swipe_first_[swipe_index] = touch_num1;
                touch_swipe_time_[swipe_index] = current_time;
            } else if (touch2 && !last_state2) {
                touch_swipe_first_[swipe_index] = touch_num2;
                touch_swipe_time_[swipe_index] = current_time;
            }
        } else {
            // 检测第二个按键是否在超时内按下
            if (current_time - touch_swipe_time_[swipe_index] <= TOUCH_SWIPE_TIMEOUT_MS) {
                if (touch_swipe_first_[swipe_index] == touch_num1 && touch2 && !last_state2) {
                    // touch_num1 → touch_num2 滑动
                    TouchEvent event = (swipe_index == 0) ? TouchEvent::LEFT_SLIDE_UP : TouchEvent::RIGHT_SLIDE_UP;
                    NotifyTouchEvent(event);
                    touch_swipe_first_[swipe_index] = 0;
                } else if (touch_swipe_first_[swipe_index] == touch_num2 && touch1 && !last_state1) {
                    // touch_num2 → touch_num1 滑动
                    TouchEvent event = (swipe_index == 0) ? TouchEvent::LEFT_SLIDE_DOWN : TouchEvent::RIGHT_SLIDE_DOWN;
                    NotifyTouchEvent(event);
                    touch_swipe_first_[swipe_index] = 0;
                }
            } else {
                touch_swipe_first_[swipe_index] = 0;
            }
        }
    }

    // 如果两个按键都释放，重置滑动检测
    if (!touch1 && !touch2 && touch_swipe_first_[swipe_index] != 0) {
        touch_swipe_first_[swipe_index] = 0;
    }

    // 滑动检测超时处理
    if (touch_swipe_first_[swipe_index] != 0 && (current_time - touch_swipe_time_[swipe_index] > TOUCH_SWIPE_TIMEOUT_MS)) {
        touch_swipe_first_[swipe_index] = 0;
    }
}

void EchoPyramid::NotifyTouchEvent(TouchEvent event) {
    if (touch_callbacks_paused_) {
        return;
    }
    
    const char* event_names[] = {"LEFT_SLIDE_UP", "LEFT_SLIDE_DOWN", "RIGHT_SLIDE_UP", "RIGHT_SLIDE_DOWN"};
    ESP_LOGI(TAG, "Touch event: %s", event_names[static_cast<int>(event)]);

    for (auto& callback : touch_callbacks_) {
        if (callback) {
            callback(event);
        }
    }
}

