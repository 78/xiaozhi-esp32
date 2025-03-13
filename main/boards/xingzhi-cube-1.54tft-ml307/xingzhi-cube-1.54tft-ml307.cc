#include "ml307_board.h"
#include "audio_codecs/no_audio_codec.h"
#include "display/lcd_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "power_save_timer.h"
#include "iot/thing_manager.h"
#include "led/single_led.h"
#include "assets/lang_config.h"
#include "../xingzhi-cube-1.54tft-wifi/power_manager.h"

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>

#include <driver/rtc_io.h>
#include <esp_sleep.h>

#define TAG "XINGZHI_CUBE_1_54TFT_ML307"  // 定义日志标签

LV_FONT_DECLARE(font_puhui_20_4);  // 声明字体
LV_FONT_DECLARE(font_awesome_20_4);

// CustomDisplay 类继承自 SpiLcdDisplay，用于自定义显示功能
class CustomDisplay : public SpiLcdDisplay {
private:
    lv_obj_t* low_battery_popup_ = nullptr;  // 低电量弹窗对象

public:
    // 构造函数，初始化显示参数
    CustomDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
          int width, int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y, bool swap_xy)
        : SpiLcdDisplay(panel_io, panel, width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy, 
    {
        .text_font = &font_puhui_20_4,  // 设置文本字体
        .icon_font = &font_awesome_20_4,  // 设置图标字体
        .emoji_font = font_emoji_64_init(),  // 设置表情字体
    }) {
    }

    // 显示低电量弹窗
    void ShowLowBatteryPopup() {
        DisplayLockGuard lock(this);  // 加锁，确保线程安全
        if (low_battery_popup_ == nullptr) {
            low_battery_popup_ = lv_obj_create(lv_screen_active());  // 创建弹窗对象
            lv_obj_set_size(low_battery_popup_, LV_HOR_RES * 0.9, LV_VER_RES * 0.5);  // 设置弹窗大小
            lv_obj_center(low_battery_popup_);  // 居中弹窗
            lv_obj_set_style_bg_color(low_battery_popup_, lv_color_black(), 0);  // 设置背景颜色
            lv_obj_set_style_radius(low_battery_popup_, 10, 0);  // 设置圆角

            lv_obj_t* label = lv_label_create(low_battery_popup_);  // 创建标签
            lv_label_set_text(label, "电量过低，请充电");  // 设置标签文本
            lv_obj_set_style_text_color(label, lv_color_white(), 0);  // 设置文本颜色
            lv_obj_center(label);  // 居中标签
        }
        lv_obj_clear_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);  // 显示弹窗
    }

    // 隐藏低电量弹窗
    void HideLowBatteryPopup() {
        DisplayLockGuard lock(this);  // 加锁，确保线程安全
        if (low_battery_popup_ != nullptr) {
            lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);  // 隐藏弹窗
        }
    }
};

// XINGZHI_CUBE_1_54TFT_ML307 类继承自 Ml307Board，用于管理设备功能
class XINGZHI_CUBE_1_54TFT_ML307 : public Ml307Board {
private:
    Button boot_button_;  // 启动按钮
    Button volume_up_button_;  // 音量增加按钮
    Button volume_down_button_;  // 音量减少按钮
    CustomDisplay* display_;  // 自定义显示对象
    PowerSaveTimer* power_save_timer_;  // 节能定时器
    PowerManager power_manager_;  // 电源管理器
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;  // LCD面板IO句柄
    esp_lcd_panel_handle_t panel_ = nullptr;  // LCD面板句柄

    // 初始化节能定时器
    void InitializePowerSaveTimer() {
        rtc_gpio_init(GPIO_NUM_21);  // 初始化RTC GPIO
        rtc_gpio_set_direction(GPIO_NUM_21, RTC_GPIO_MODE_OUTPUT_ONLY);  // 设置GPIO方向
        rtc_gpio_set_level(GPIO_NUM_21, 1);  // 设置GPIO电平

        power_save_timer_ = new PowerSaveTimer(-1, 60, 300);  // 创建节能定时器
        power_save_timer_->OnEnterSleepMode([this]() {  // 进入睡眠模式回调
            ESP_LOGI(TAG, "Enabling sleep mode");
            auto display = GetDisplay();
            display->SetChatMessage("system", "");
            display->SetEmotion("sleepy");
            GetBacklight()->SetBrightness(1);
        });
        power_save_timer_->OnExitSleepMode([this]() {  // 退出睡眠模式回调
            auto display = GetDisplay();
            display->SetChatMessage("system", "");
            display->SetEmotion("neutral");
            GetBacklight()->RestoreBrightness();
        });
        power_save_timer_->OnShutdownRequest([this]() {  // 关机请求回调
            ESP_LOGI(TAG, "Shutting down");
            rtc_gpio_set_level(GPIO_NUM_21, 0);
            rtc_gpio_hold_en(GPIO_NUM_21);  // 启用保持功能
            esp_lcd_panel_disp_on_off(panel_, false);  // 关闭显示
            esp_deep_sleep_start();  // 进入深度睡眠
        });
        power_save_timer_->SetEnabled(true);  // 启用节能定时器
    }

    // 初始化SPI总线
    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_SDA;  // 设置MOSI引脚
        buscfg.miso_io_num = GPIO_NUM_NC;  // 设置MISO引脚
        buscfg.sclk_io_num = DISPLAY_SCL;  // 设置SCLK引脚
        buscfg.quadwp_io_num = GPIO_NUM_NC;  // 设置QUADWP引脚
        buscfg.quadhd_io_num = GPIO_NUM_NC;  // 设置QUADHD引脚
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);  // 设置最大传输大小
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));  // 初始化SPI总线
    }

    // 初始化按钮
    void InitializeButtons() {
        boot_button_.OnClick([this]() {  // 启动按钮点击回调
            power_save_timer_->WakeUp();  // 唤醒设备
            auto& app = Application::GetInstance();
            app.ToggleChatState();  // 切换聊天状态
        });

        volume_up_button_.OnClick([this]() {  // 音量增加按钮点击回调
            power_save_timer_->WakeUp();  // 唤醒设备
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;  // 增加音量
            if (volume > 100) {
                volume = 100;
            }
            codec->SetOutputVolume(volume);  // 设置音量
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));  // 显示音量通知
        });

        volume_up_button_.OnLongPress([this]() {  // 音量增加按钮长按回调
            power_save_timer_->WakeUp();  // 唤醒设备
            GetAudioCodec()->SetOutputVolume(100);  // 设置最大音量
            GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);  // 显示最大音量通知
        });

        volume_down_button_.OnClick([this]() {  // 音量减少按钮点击回调
            power_save_timer_->WakeUp();  // 唤醒设备
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;  // 减少音量
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);  // 设置音量
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));  // 显示音量通知
        });

        volume_down_button_.OnLongPress([this]() {  // 音量减少按钮长按回调
            power_save_timer_->WakeUp();  // 唤醒设备
            GetAudioCodec()->SetOutputVolume(0);  // 静音
            GetDisplay()->ShowNotification(Lang::Strings::MUTED);  // 显示静音通知
        });
    }

    // 初始化ST7789显示
    void InitializeSt7789Display() {
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS;  // 设置CS引脚
        io_config.dc_gpio_num = DISPLAY_DC;  // 设置DC引脚
        io_config.spi_mode = 3;  // 设置SPI模式
        io_config.pclk_hz = 80 * 1000 * 1000;  // 设置时钟频率
        io_config.trans_queue_depth = 10;  // 设置传输队列深度
        io_config.lcd_cmd_bits = 8;  // 设置命令位数
        io_config.lcd_param_bits = 8;  // 设置参数位数
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io_));  // 初始化面板IO

        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RES;  // 设置复位引脚
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;  // 设置RGB元素顺序
        panel_config.bits_per_pixel = 16;  // 设置每像素位数
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io_, &panel_config, &panel_));  // 初始化ST7789面板
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));  // 复位面板
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_));  // 初始化面板
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_, DISPLAY_SWAP_XY));  // 交换XY轴
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));  // 镜像显示
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_, true));  // 反色显示

        display_ = new CustomDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, 
            DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);  // 创建自定义显示对象
    }

    // 初始化IoT设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));  // 添加扬声器设备
        thing_manager.AddThing(iot::CreateThing("Backlight"));  // 添加背光设备
        thing_manager.AddThing(iot::CreateThing("Battery"));  // 添加电池设备
    }

public:
    // 构造函数，初始化设备
    XINGZHI_CUBE_1_54TFT_ML307() :
        Ml307Board(ML307_TX_PIN, ML307_RX_PIN, 4096),
        boot_button_(BOOT_BUTTON_GPIO),
        volume_up_button_(VOLUME_UP_BUTTON_GPIO),
        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO),
        power_manager_(GPIO_NUM_38) {
        InitializePowerSaveTimer();  // 初始化节能定时器
        InitializeSpi();  // 初始化SPI
        InitializeButtons();  // 初始化按钮
        InitializeSt7789Display();  // 初始化ST7789显示
        InitializeIot();  // 初始化IoT设备
        GetBacklight()->RestoreBrightness();  // 恢复背光亮度
    }

    // 获取音频编解码器
    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
        return &audio_codec;
    }

    // 获取显示对象
    virtual Display* GetDisplay() override {
        return display_;
    }
    
    // 获取背光对象
    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    // 获取电池电量
    virtual bool GetBatteryLevel(int& level, bool& charging) override {
        static int last_level = 0;
        static bool last_charging = false;

        charging = power_manager_.IsCharging();  // 获取充电状态
        if (charging != last_charging) {
            power_save_timer_->WakeUp();  // 唤醒设备
        }

        level = power_manager_.ReadBatteryLevel(charging != last_charging);  // 读取电池电量
        if (level != last_level || charging != last_charging) {
            last_level = level;
            last_charging = charging;
            ESP_LOGI(TAG, "Battery level: %d, charging: %d", level, charging);  // 记录电池电量和充电状态
        }

        static bool show_low_power_warning_ = false;
        if (power_manager_.IsBatteryLevelSteady()) {
            if (!charging) {
                // 电量低于 15% 时，显示低电量警告
                if (!show_low_power_warning_ && level <= 15) {
                    display_->ShowLowBatteryPopup();  // 显示低电量弹窗
                    show_low_power_warning_ = true;
                }
                power_save_timer_->SetEnabled(true);  // 启用节能定时器
            } else {
                if (show_low_power_warning_) {
                    display_->HideLowBatteryPopup();  // 隐藏低电量弹窗
                    show_low_power_warning_ = false;
                }
                power_save_timer_->SetEnabled(false);  // 禁用节能定时器
            }
        }
        return true;
    }

    // 设置节能模式
    virtual void SetPowerSaveMode(bool enabled) override {
        if (!enabled) {
            power_save_timer_->WakeUp();  // 唤醒设备
        }
        Ml307Board::SetPowerSaveMode(enabled);  // 设置节能模式
    }
};

// 声明设备
DECLARE_BOARD(XINGZHI_CUBE_1_54TFT_ML307);