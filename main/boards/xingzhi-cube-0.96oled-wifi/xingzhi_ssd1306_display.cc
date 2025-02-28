#include "xingzhi_ssd1306_display.h"
#include "font_awesome_symbols.h"

#include <esp_log.h>
#include <esp_err.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lvgl_port.h>

#include <driver/ledc.h>
#include <driver/gpio.h>
#include <vector>
#include "board.h"
#include <esp_timer.h>

#include "esp_adc/adc_oneshot.h"

#include "button.h"
#include <inttypes.h>  
#include "config.h"  
#include "settings.h"
#include "esp_sleep.h"
#include "application.h"
#include "driver/rtc_io.h"
#include "led/single_led.h"

#define TAG "XINGZHI_Ssd1306Display"

LV_FONT_DECLARE(font_awesome_30_1);

XINGZHI_Ssd1306Display::XINGZHI_Ssd1306Display(void* i2c_master_handle, int width, int height, bool mirror_x, bool mirror_y,
                                               const lv_font_t* text_font, const lv_font_t* icon_font) 
    : text_font_(text_font), icon_font_(icon_font), last_interaction_time_(esp_timer_get_time()), boot_button_(BOOT_BUTTON_GPIO),volume_up_button_(VOLUME_UP_BUTTON_GPIO), volume_down_button_(VOLUME_DOWN_BUTTON_GPIO){
    width_ = width;
    height_ = height;

    // 创建充电检测定时器
    esp_timer_create_args_t charging_timer_args = {
        .callback = &XINGZHI_Ssd1306Display::ChargingTimerCallback,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "charging_timer"
    };
    ESP_ERROR_CHECK(esp_timer_create(&charging_timer_args, &charging_timer_));

    // 创建电量检测定时器
    esp_timer_create_args_t battery_timer_args = {
        .callback = &XINGZHI_Ssd1306Display::BatteryTimerCallback,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "battery_timer"
    };
    ESP_ERROR_CHECK(esp_timer_create(&battery_timer_args, &battery_timer_));

    // 初始化充电引脚
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << charging_pin_);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE; 
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;     
    gpio_config(&io_conf);

    rtc_gpio_init(GPIO_NUM_21);
    rtc_gpio_set_direction(GPIO_NUM_21, RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_set_level(GPIO_NUM_21, 1);
    
    boot_button_.OnPressDown([this]() {
        this->UpdateInteractionTime();
    });

    volume_up_button_.OnPressDown([this]() {
        this->UpdateInteractionTime();
    });

    volume_down_button_.OnPressDown([this]() {
        this->UpdateInteractionTime();
    });

    ESP_LOGI(TAG, "Initialize LVGL");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_port_init(&port_cfg);

    // SSD1306 config
    esp_lcd_panel_io_i2c_config_t io_config = {
        .dev_addr = 0x3C,
        .on_color_trans_done = nullptr,
        .user_ctx = nullptr,
        .control_phase_bytes = 1,
        .dc_bit_offset = 6,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .flags = {
            .dc_low_on_data = 0,
            .disable_control_phase = 0,
        },
        .scl_speed_hz = 400 * 1000,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c_v2((i2c_master_bus_t*)i2c_master_handle, &io_config, &panel_io_));

    ESP_LOGI(TAG, "Install SSD1306 driver");
    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = -1;
    panel_config.bits_per_pixel = 1;

    esp_lcd_panel_ssd1306_config_t ssd1306_config = {
        .height = static_cast<uint8_t>(height_),
    };
    panel_config.vendor_config = &ssd1306_config;

    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(panel_io_, &panel_config, &panel_));
    ESP_LOGI(TAG, "SSD1306 driver installed");

    // Reset the display
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
    if (esp_lcd_panel_init(panel_) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize display");
        return;
    }

    // Set the display to on
    ESP_LOGI(TAG, "Turning display on");
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

    ESP_LOGI(TAG, "Adding LCD screen");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io_,
        .panel_handle = panel_,
        .control_handle = nullptr,
        .buffer_size = static_cast<uint32_t>(width_ * height_),
        .double_buffer = false,
        .trans_size = 0,
        .hres = static_cast<uint32_t>(width_),
        .vres = static_cast<uint32_t>(height_),
        .monochrome = true,
        .rotation = {
            .swap_xy = false,
            .mirror_x = mirror_x,
            .mirror_y = mirror_y,
        },
        .flags = {
            .buff_dma = 1,
            .buff_spiram = 0,
            .sw_rotate = 0,
            .full_refresh = 0,
            .direct_mode = 0,
        },
    };

    display_ = lvgl_port_add_disp(&display_cfg);
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add display");
        return;
    }

    if (height_ == 64) {
        SetupUI_128x64();
    } else {
        SetupUI_128x32();
    }
    StartChargingTimer();
    StartBatteryTimer();
}

XINGZHI_Ssd1306Display::~XINGZHI_Ssd1306Display() {
    if (content_ != nullptr) {
        lv_obj_del(content_);
    }
    if (status_bar_ != nullptr) {
        lv_obj_del(status_bar_);
    }
    if (side_bar_ != nullptr) {
        lv_obj_del(side_bar_);
    }
    if (container_ != nullptr) {
        lv_obj_del(container_);
    }

    if (panel_ != nullptr) {
        esp_lcd_panel_del(panel_);
    }
    if (panel_io_ != nullptr) {
        esp_lcd_panel_io_del(panel_io_);
    }
    lvgl_port_deinit();
}

void XINGZHI_Ssd1306Display::UpdateInteractionTime() {
    last_interaction_time_ = esp_timer_get_time();
    if (is_light_sleep_) {
        // 从浅睡眠中唤醒，打开显示
        esp_lcd_panel_disp_on_off(panel_, true); 
        is_light_sleep_ = false;
    }
}

void XINGZHI_Ssd1306Display::CheckSleepState() {
    int64_t current_time = esp_timer_get_time();
    int64_t elapsed_time = (current_time - last_interaction_time_) / 1000000; // 转换为秒

    int charging_level = gpio_get_level(charging_pin_);
    bool is_charging = (charging_level == 1);

    if (is_charging) {
        // 正在充电，不进入睡眠
        return;
    }

    if (elapsed_time >= 60 && !is_light_sleep_ && !is_deep_sleep_) {
        is_light_sleep_ = true;
        // 关闭显示
        esp_lcd_panel_disp_on_off(panel_, false); 
    } else if (elapsed_time >= 300 && is_light_sleep_) {
        is_deep_sleep_ = true; // 深睡眠
        is_light_sleep_ = false;
        // 初始化GPIO 21为RTC GPIO，关闭4g模块
        rtc_gpio_set_level(GPIO_NUM_21, 0);
        // 启用保持功能，确保睡眠期间电平不变
        rtc_gpio_hold_en(GPIO_NUM_21);
        esp_deep_sleep_start();
    }
}

bool XINGZHI_Ssd1306Display::Lock(int timeout_ms) {
    return lvgl_port_lock(timeout_ms);
}

void XINGZHI_Ssd1306Display::Unlock() {
    lvgl_port_unlock();
}

uint16_t XINGZHI_Ssd1306Display::ReadBatteryLevel() {
    adc_oneshot_unit_handle_t adc_handle;
    adc_oneshot_unit_init_cfg_t init_config = {
       .unit_id = ADC_UNIT_2,
       .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    // 初始化 ADC 单元
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));
    adc_oneshot_chan_cfg_t chan_config = {
        .atten = ADC_ATTEN_DB_12, 
        .bitwidth = ADC_BITWIDTH_12,
    };
    // 配置 ADC 通道
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_6, &chan_config));
    int adc_value;
    // 读取 ADC 值
    ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL_6, &adc_value));
    adc_oneshot_del_unit(adc_handle);
    return adc_value;
}

void XINGZHI_Ssd1306Display::BatteryTimerCallback(void* arg) {
    XINGZHI_Ssd1306Display* display = static_cast<XINGZHI_Ssd1306Display*>(arg);
    uint16_t adc_value = display->ReadBatteryLevel();
    if (display->first_battery_invert_) {
        display->adc_samp_interval = 180000000;  // adc值采样的时间间隔
        // 停止当前定时器
        esp_timer_stop(display->battery_timer_);
        // 重新启动定时器，使用新的时间间隔
        ESP_ERROR_CHECK(esp_timer_start_periodic(display->battery_timer_, display->adc_samp_interval));
    }
    ESP_LOGI(TAG, "adc_samp_interval: %" PRId32 "", display->adc_samp_interval);
    ESP_LOGI(TAG, "Value of first_battery_invert_ before condition: %d", display->first_battery_invert_);
    display->adc_values.push_back(adc_value);
    display->adc_count++;

    if (display->adc_count >= 1) {
        uint32_t sum = 0;
        for (uint16_t value : display->adc_values) {
            sum += value;
        }
        display->average_adc = sum / display->adc_values.size();
        display->first_battery_invert_ = true; 
    }
}

void XINGZHI_Ssd1306Display::StartChargingTimer() {
    ESP_ERROR_CHECK(esp_timer_start_periodic(charging_timer_, adc_samp_interval));
}

void XINGZHI_Ssd1306Display::StartBatteryTimer() {
    ESP_ERROR_CHECK(esp_timer_start_periodic(battery_timer_, adc_samp_interval));
}

void XINGZHI_Ssd1306Display::SetChatMessage(const char* role, const char* content) {
    DisplayLockGuard lock(this);
    if (chat_message_label_ == nullptr) {
        return;
    }
    if (content_right_ == nullptr) {
        lv_label_set_text(chat_message_label_, content);
    } else {
        if (content == nullptr || content[0] == '\0') {
            lv_obj_add_flag(content_right_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_label_set_text(chat_message_label_, content);
            lv_obj_clear_flag(content_right_, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void XINGZHI_Ssd1306Display::SetupUI_128x64() {
    DisplayLockGuard lock(this);

    auto screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, text_font_, 0);
    lv_obj_set_style_text_color(screen, lv_color_black(), 0);

    /* Container */
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_row(container_, 0, 0);

    /* Status bar */
    status_bar_ = lv_obj_create(container_);
    lv_obj_set_size(status_bar_, LV_HOR_RES, 16);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_radius(status_bar_, 0, 0);

    /* Content */
    content_ = lv_obj_create(container_);
    lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_radius(content_, 0, 0);
    lv_obj_set_style_pad_all(content_, 0, 0);
    lv_obj_set_width(content_, LV_HOR_RES);
    lv_obj_set_flex_grow(content_, 1);
    lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_flex_main_place(content_, LV_FLEX_ALIGN_CENTER, 0);

    // 创建左侧固定宽度的容器
    content_left_ = lv_obj_create(content_);
    lv_obj_set_size(content_left_, 32, LV_SIZE_CONTENT);  // 固定宽度32像素
    lv_obj_set_style_pad_all(content_left_, 0, 0);
    lv_obj_set_style_border_width(content_left_, 0, 0);

    emotion_label_ = lv_label_create(content_left_);
    lv_obj_set_style_text_font(emotion_label_, &font_awesome_30_1, 0);
    lv_label_set_text(emotion_label_, FONT_AWESOME_AI_CHIP);
    lv_obj_center(emotion_label_);
    lv_obj_set_style_pad_top(emotion_label_, 8, 0);

    // 创建右侧可扩展的容器
    content_right_ = lv_obj_create(content_);
    lv_obj_set_size(content_right_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(content_right_, 0, 0);
    lv_obj_set_style_border_width(content_right_, 0, 0);
    lv_obj_set_flex_grow(content_right_, 1);
    lv_obj_add_flag(content_right_, LV_OBJ_FLAG_HIDDEN);

    chat_message_label_ = lv_label_create(content_right_);
    lv_label_set_text(chat_message_label_, "");
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(chat_message_label_, lv_pct(100));
    lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_LEFT, 0);

    /* Status bar */
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_column(status_bar_, 0, 0);

    network_label_ = lv_label_create(status_bar_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, icon_font_, 0);

    notification_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(notification_label_, 1);
    lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(notification_label_, "通知");
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(status_label_, 1);
    lv_label_set_text(status_label_, "正在初始化");
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);

    mute_label_ = lv_label_create(status_bar_);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, icon_font_, 0);

    battery_label_ = lv_label_create(status_bar_);
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, icon_font_, 0);

    /* 充电状态标签 */
    charging_label_ = lv_label_create(status_bar_);
    lv_obj_set_style_text_font(charging_label_, icon_font_, 0);
    lv_obj_set_style_text_align(charging_label_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_flex_grow(charging_label_, 0);
    lv_label_set_text(charging_label_, "");

    // 检查充电状态
    int charging_level = gpio_get_level(charging_pin_);
    if (charging_level == 1) {
        lv_label_set_text(charging_label_, FONT_AWESOME_BATTERY_CHARGING);
    }
}

void XINGZHI_Ssd1306Display::SetupUI_128x32() {
    DisplayLockGuard lock(this);

    auto screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, text_font_, 0);

    /* Container */
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_column(container_, 0, 0);

    /* Left side */
    side_bar_ = lv_obj_create(container_);
    lv_obj_set_flex_grow(side_bar_, 1);
    lv_obj_set_flex_flow(side_bar_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(side_bar_, 0, 0);
    lv_obj_set_style_border_width(side_bar_, 0, 0);
    lv_obj_set_style_radius(side_bar_, 0, 0);
    lv_obj_set_style_pad_row(side_bar_, 0, 0);

    /* Emotion label on the right side */
    content_ = lv_obj_create(container_);
    lv_obj_set_size(content_, 32, 32);
    lv_obj_set_style_pad_all(content_, 0, 0);
    lv_obj_set_style_border_width(content_, 0, 0);
    lv_obj_set_style_radius(content_, 0, 0);

    emotion_label_ = lv_label_create(content_);
    lv_obj_set_style_text_font(emotion_label_, &font_awesome_30_1, 0);
    lv_label_set_text(emotion_label_, FONT_AWESOME_AI_CHIP);
    lv_obj_center(emotion_label_);

    /* Status bar */
    status_bar_ = lv_obj_create(side_bar_);
    lv_obj_set_size(status_bar_, LV_SIZE_CONTENT, 16);
    lv_obj_set_style_radius(status_bar_, 0, 0);
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_column(status_bar_, 0, 0);

    network_label_ = lv_label_create(status_bar_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, icon_font_, 0);

    mute_label_ = lv_label_create(status_bar_);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, icon_font_, 0);

    battery_label_ = lv_label_create(status_bar_);
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, icon_font_, 0);

    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_style_pad_left(status_label_, 2, 0);
    lv_label_set_text(status_label_, "正在初始化");

    notification_label_ = lv_label_create(status_bar_);
    lv_label_set_text(notification_label_, "通知");
    lv_obj_set_style_pad_left(notification_label_, 2, 0);
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    chat_message_label_ = lv_label_create(side_bar_);
    lv_obj_set_flex_grow(chat_message_label_, 1);
    lv_obj_set_width(chat_message_label_, width_ - 32);
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(chat_message_label_, "");

    /* 充电状态标签 */
    charging_label_ = lv_label_create(status_bar_);
    lv_obj_set_style_text_font(charging_label_, icon_font_, 0);
    lv_obj_set_style_text_align(charging_label_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_flex_grow(charging_label_, 0);
    lv_label_set_text(charging_label_, "");

    // 检查充电状态
    int charging_level = gpio_get_level(charging_pin_);
    if (charging_level == 1) {
        lv_label_set_text(charging_label_, FONT_AWESOME_BATTERY_CHARGING);
    }
}

void XINGZHI_Ssd1306Display::UpdateBatteryAndChargingDisplay(uint16_t average_adc) {
    DisplayLockGuard lock(this);

    // 未充电时，显示电池图标
    if (charging_label_ != nullptr) {
        lv_label_set_text(charging_label_, "");
    }

    uint8_t battery_level = 0;
    if (average_adc < 2000) {
        battery_level = 0;
        // 显示电量过低提示窗口
        ShowLowBatteryPopup();
    } else if (average_adc >= 2000 && average_adc < 2100) {
        battery_level = 1;
        // 如果电量回升，隐藏提示窗口
        if (low_battery_popup_ != nullptr) {
            lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);
        }
    } else if (average_adc >= 2100 && average_adc < 2200) {
        battery_level = 2;
        if (low_battery_popup_ != nullptr) {
            lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);
        }
    } else if (average_adc >= 2200 && average_adc < 2300) {
        battery_level = 3;
        if (low_battery_popup_ != nullptr) {
            lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        battery_level = 4;
        if (low_battery_popup_ != nullptr) {
            lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);
        }
    }

    const char* battery_icon;
    switch (battery_level) {
        case 0:
            battery_icon = FONT_AWESOME_BATTERY_EMPTY;
            break;
        case 1:
            battery_icon = FONT_AWESOME_BATTERY_1;
            break;
        case 2:
            battery_icon = FONT_AWESOME_BATTERY_2;
            break;
        case 3:
            battery_icon = FONT_AWESOME_BATTERY_3;
            break;
        case 4:
            battery_icon = FONT_AWESOME_BATTERY_FULL;
            break;
        default:
            battery_icon = FONT_AWESOME_BATTERY_SLASH;
            break;
    }

    if (battery_label_ != nullptr) {
        lv_obj_set_style_text_font(battery_label_, icon_font_, 0);
        lv_label_set_text(battery_label_, battery_icon);
    }
}

void XINGZHI_Ssd1306Display::ChargingTimerCallback(void* arg) {
    XINGZHI_Ssd1306Display* display = static_cast<XINGZHI_Ssd1306Display*>(arg);
    DisplayLockGuard lock(display);

    // 检查充电状态
    int charging_level = gpio_get_level(display->charging_pin_);
    bool is_charging = (charging_level == 1);
    display->OnStateChanged();//检测当前对对话状态
    // 检查电池是否充满，adc值超过2430，判定为充满
    bool is_battery_full = 0;
    if (display->average_adc > 2430)
    {
        is_battery_full = 1;
    } 
    if (is_charging) {
        // 正在充电，更新交互时间，防止进入睡眠
        display->UpdateInteractionTime();
        if (is_battery_full) {
            if (display->charging_label_ != nullptr) {
                lv_label_set_text(display->charging_label_, "");
            }
            if (display->battery_label_ != nullptr) {
                lv_obj_set_style_text_font(display->battery_label_, display->icon_font_, 0);
                lv_label_set_text(display->battery_label_, FONT_AWESOME_BATTERY_FULL);
            }
        } else {
            if (display->charging_label_ != nullptr) {
                lv_obj_set_style_text_font(display->charging_label_, display->icon_font_, 0);
                lv_label_set_text(display->charging_label_, FONT_AWESOME_BATTERY_CHARGING);
            }
            if (display->battery_label_ != nullptr) {
                lv_label_set_text(display->battery_label_, "");
            }
        }
        // 如果正在充电，隐藏电量过低提示窗口
        if (display->low_battery_popup_ != nullptr) {
            lv_obj_add_flag(display->low_battery_popup_, LV_OBJ_FLAG_HIDDEN);
        }
        display->was_charging = true; // 更新上一次的充电状态为正在充电
    } else {
        if (display->was_charging) {
            // 充电状态从充电变为未充电，立即读取并更新电池电量
            display->average_adc = display->ReadBatteryLevel();
        } else {
            // 一直处于未充电状态，正常显示电池图标
            if (display->charging_label_ != nullptr) {
                if (!display->first_battery_invert_) {
                    display->average_adc = display->ReadBatteryLevel();
                }
                display->UpdateBatteryAndChargingDisplay(display->average_adc);
                // 清空数组和计数器
                display->adc_values.clear();
                display->adc_count = 0;
            }
        }
        display->was_charging = false; // 更新上一次的充电状态为未充电
    }
    // 检查睡眠状态
    display->CheckSleepState();
}

void XINGZHI_Ssd1306Display::ShowLowBatteryPopup() {
    DisplayLockGuard lock(this);

    if (low_battery_popup_ == nullptr) {
        // 创建弹出窗口
        low_battery_popup_ = lv_obj_create(lv_scr_act());
        lv_obj_set_size(low_battery_popup_, 120, 30);
        lv_obj_center(low_battery_popup_);
        lv_obj_set_style_bg_color(low_battery_popup_, lv_color_black(), 0);
        lv_obj_set_style_radius(low_battery_popup_, 10, 0);

        // 创建提示文本标签
        lv_obj_t* label = lv_label_create(low_battery_popup_);
        lv_label_set_text(label, "电量过低，请充电");
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_obj_center(label);
    }

    // 显示弹出窗口
    lv_obj_clear_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);
}

void XINGZHI_Ssd1306Display::OnStateChanged() {
    auto& app = Application::GetInstance();
    auto device_state = app.GetDeviceState();
    if (device_state != kDeviceStateIdle && !this->was_charging) {
        UpdateInteractionTime();
    }
}

