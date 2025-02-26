#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include "display/display.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_timer.h>
#include <font_emoji.h>

#include <atomic>
// 新增的头文件包含语句
#include <vector>
#include "button.h"

class XINGZHI_1_54_TFT_LcdDisplay : public Display {
protected:
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    gpio_num_t backlight_pin_ = GPIO_NUM_NC;
    bool backlight_output_invert_ = false;
    
    lv_draw_buf_t draw_buf_;
    lv_obj_t* status_bar_ = nullptr;
    lv_obj_t* content_ = nullptr;
    lv_obj_t* container_ = nullptr;
    lv_obj_t* side_bar_ = nullptr;

    DisplayFonts fonts_;

    esp_timer_handle_t backlight_timer_ = nullptr;
    uint8_t current_brightness_ = 0;


    lv_obj_t* charging_label_ = nullptr; // 充电状态标签
    lv_obj_t* low_battery_popup_ = nullptr;  // 电量过低提示窗口对象指针
    lv_obj_t* battery_label_ = nullptr;  // 已有，用于显示电量
    int32_t adc_samp_interval = 500000;         // adc值采样的时间间隔，单位为微秒
    uint16_t average_adc = 0;  // adc平均值
    esp_timer_handle_t charging_timer_;  // 充电检测定时器句柄
    esp_timer_handle_t battery_timer_;  // 电量检测定时器句柄
    gpio_num_t charging_pin_ = GPIO_NUM_38; // 充电检测引脚
    std::vector<uint16_t> adc_values; // 用于存储读取的ADC值
    int adc_count = 0; // 记录已检测的ADC值数量
    bool was_charging = false; // 上一次的充电状态
    bool first_battery_invert_ = false; //首次上电直接检测电量
    void ShowLowBatteryPopup();  // 显示电量过低提示窗口的方法声明
    uint16_t ReadBatteryLevel();  // 读取电量的方法

    int64_t last_interaction_time_; // 上次交互时间
    bool is_light_sleep_ = false; // 浅睡眠
    bool is_deep_sleep_ = false; // 深睡眠
    Button boot_button_;
    Button volume_up_button_;
    Button volume_down_button_;


    void OnBacklightTimer();
    void InitializeBacklight(gpio_num_t backlight_pin);
    virtual void SetupUI();
    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;


public:
    XINGZHI_1_54_TFT_LcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                  gpio_num_t backlight_pin, bool backlight_output_invert,
                  int width, int height,  int offset_x, int offset_y, bool mirror_x, bool mirror_y, bool swap_xy,
                  DisplayFonts fonts);
    ~XINGZHI_1_54_TFT_LcdDisplay();


    static void ChargingTimerCallback(void* arg);
    static void BatteryTimerCallback(void* arg);
    void StartChargingTimer();
    void StartBatteryTimer();
    void UpdateBatteryAndChargingDisplay(uint16_t average_adc);
    void OnStateChanged();

    void UpdateInteractionTime();
    void CheckSleepState();


    virtual void SetEmotion(const char* emotion) override;
    virtual void SetIcon(const char* icon) override;
    virtual void SetBacklight(uint8_t brightness) override;

};

#endif // LCD_DISPLAY_H
