#ifndef SSD1306_DISPLAY_H
#define SSD1306_DISPLAY_H

#include "display/display.h"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <esp_timer.h>
#include <font_emoji.h>
#include <driver/gpio.h>
#include <atomic>
// 新增的头文件包含语句
#include <vector>
#include "button.h"


class XINGZHI_Ssd1306Display : public Display {
private:
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;

    lv_obj_t* status_bar_ = nullptr;
    lv_obj_t* content_ = nullptr;
    lv_obj_t* content_left_ = nullptr;
    lv_obj_t* content_right_ = nullptr;
    lv_obj_t* container_ = nullptr;
    lv_obj_t* side_bar_ = nullptr;

    const lv_font_t* text_font_ = nullptr;
    const lv_font_t* icon_font_ = nullptr;

    DisplayFonts fonts_;

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

    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

    void SetupUI_128x64();
    void SetupUI_128x32();

public:
    XINGZHI_Ssd1306Display(void* i2c_master_handle, int width, int height, bool mirror_x, bool mirror_y,
                   const lv_font_t* text_font, const lv_font_t* icon_font);
    ~XINGZHI_Ssd1306Display();
    

    static void ChargingTimerCallback(void* arg);
    static void BatteryTimerCallback(void* arg);
    void StartChargingTimer();
    void StartBatteryTimer();
    void UpdateBatteryAndChargingDisplay(uint16_t average_adc);
    void OnStateChanged();

    void UpdateInteractionTime();
    void CheckSleepState();


    virtual void SetChatMessage(const char* role, const char* content) override;
};

#endif // SSD1306_DISPLAY_H
