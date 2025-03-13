#include "wifi_board.h"
#include "display/lcd_display.h"
#include "audio_codecs/es8311_audio_codec.h"
#include "application.h"
#include "button.h"
#include "led/single_led.h"
#include "iot/thing_manager.h"
#include "config.h"
#include "power_save_timer.h"
#include "font_awesome_symbols.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_nv3023.h>
#include <esp_efuse_table.h>

#define TAG "magiclick_c3"  // 定义日志标签

LV_FONT_DECLARE(font_puhui_16_4);  // 声明字体
LV_FONT_DECLARE(font_awesome_16_4);  // 声明字体

// NV3023Display 类继承自 SpiLcdDisplay，用于管理 NV3023 液晶显示屏
class NV3023Display : public SpiLcdDisplay {
public:
    NV3023Display(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                int width, int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y, bool swap_xy)
        : SpiLcdDisplay(panel_io, panel, width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy, 
                    {
                        .text_font = &font_puhui_16_4,  // 设置文本字体
                        .icon_font = &font_awesome_16_4,  // 设置图标字体
                        .emoji_font = font_emoji_32_init(),  // 设置表情字体
                    }) {

        DisplayLockGuard lock(this);  // 锁定显示，防止多线程冲突
        // 只需要覆盖颜色相关的样式
        auto screen = lv_disp_get_scr_act(lv_disp_get_default());  // 获取当前屏幕
        lv_obj_set_style_text_color(screen, lv_color_black(), 0);  // 设置屏幕文本颜色为黑色

        // 设置容器背景色
        lv_obj_set_style_bg_color(container_, lv_color_black(), 0);  // 设置容器背景色为黑色

        // 设置状态栏背景色和文本颜色
        lv_obj_set_style_bg_color(status_bar_, lv_color_white(), 0);  // 设置状态栏背景色为白色
        lv_obj_set_style_text_color(network_label_, lv_color_black(), 0);  // 设置网络标签文本颜色为黑色
        lv_obj_set_style_text_color(notification_label_, lv_color_black(), 0);  // 设置通知标签文本颜色为黑色
        lv_obj_set_style_text_color(status_label_, lv_color_black(), 0);  // 设置状态标签文本颜色为黑色
        lv_obj_set_style_text_color(mute_label_, lv_color_black(), 0);  // 设置静音标签文本颜色为黑色
        lv_obj_set_style_text_color(battery_label_, lv_color_black(), 0);  // 设置电池标签文本颜色为黑色

        // 设置内容区背景色和文本颜色
        lv_obj_set_style_bg_color(content_, lv_color_black(), 0);  // 设置内容区背景色为黑色
        lv_obj_set_style_border_width(content_, 0, 0);  // 设置内容区边框宽度为0
        lv_obj_set_style_text_color(emotion_label_, lv_color_white(), 0);  // 设置表情标签文本颜色为白色
        lv_obj_set_style_text_color(chat_message_label_, lv_color_white(), 0);  // 设置聊天消息标签文本颜色为白色
    }
};

// magiclick_c3 类继承自 WifiBoard，用于管理整个设备的硬件和功能
class magiclick_c3 : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;  // I2C 总线句柄，用于音频编解码器
    Button boot_button_;  // 启动按钮
    NV3023Display* display_;  // 显示屏对象
    PowerSaveTimer* power_save_timer_;  // 节能定时器

    // 初始化节能定时器
    void InitializePowerSaveTimer() {
        power_save_timer_ = new PowerSaveTimer(160);  // 创建节能定时器，设置超时时间为160秒
        power_save_timer_->OnEnterSleepMode([this]() {  // 设置进入睡眠模式的回调函数
            ESP_LOGI(TAG, "Enabling sleep mode");  // 打印日志
            auto display = GetDisplay();  // 获取显示屏对象
            display->SetChatMessage("system", "");  // 清空聊天消息
            display->SetEmotion("sleepy");  // 设置表情为“sleepy”
            GetBacklight()->SetBrightness(10);  // 设置背光亮度为10
            
            auto codec = GetAudioCodec();  // 获取音频编解码器
            codec->EnableInput(false);  // 禁用音频输入
        });
        power_save_timer_->OnExitSleepMode([this]() {  // 设置退出睡眠模式的回调函数
            auto codec = GetAudioCodec();  // 获取音频编解码器
            codec->EnableInput(true);  // 启用音频输入
            
            auto display = GetDisplay();  // 获取显示屏对象
            display->SetChatMessage("system", "");  // 清空聊天消息
            display->SetEmotion("neutral");  // 设置表情为“neutral”
            GetBacklight()->RestoreBrightness();  // 恢复背光亮度
        });
        power_save_timer_->SetEnabled(true);  // 启用节能定时器
    }

    // 初始化音频编解码器的 I2C 总线
    void InitializeCodecI2c() {
        // 初始化 I2C 外设
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,  // 使用 I2C 端口 0
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,  // 设置 SDA 引脚
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,  // 设置 SCL 引脚
            .clk_source = I2C_CLK_SRC_DEFAULT,  // 使用默认时钟源
            .glitch_ignore_cnt = 7,  // 设置毛刺忽略计数
            .intr_priority = 0,  // 设置中断优先级
            .trans_queue_depth = 0,  // 设置传输队列深度
            .flags = {
                .enable_internal_pullup = 1,  // 启用内部上拉
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));  // 创建 I2C 主总线
    }

    // 初始化按钮
    void InitializeButtons() {
        boot_button_.OnClick([this]() {  // 设置按钮点击回调函数
            auto& app = Application::GetInstance();  // 获取应用实例
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();  // 如果设备正在启动且未连接 Wi-Fi，则重置 Wi-Fi 配置
            }
        });
        boot_button_.OnPressDown([this]() {  // 设置按钮按下回调函数
            power_save_timer_->WakeUp();  // 唤醒设备
            Application::GetInstance().StartListening();  // 开始监听
        });
        boot_button_.OnPressUp([this]() {  // 设置按钮释放回调函数
            Application::GetInstance().StopListening();  // 停止监听
        });
    }

    // 初始化 SPI 总线
    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_SDA_PIN;  // 设置 MOSI 引脚
        buscfg.miso_io_num = GPIO_NUM_NC;  // 设置 MISO 引脚为未连接
        buscfg.sclk_io_num = DISPLAY_SCL_PIN;  // 设置 SCLK 引脚
        buscfg.quadwp_io_num = GPIO_NUM_NC;  // 设置 QUADWP 引脚为未连接
        buscfg.quadhd_io_num = GPIO_NUM_NC;  // 设置 QUADHD 引脚为未连接
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);  // 设置最大传输大小
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));  // 初始化 SPI 总线
    }

    // 初始化 NV3023 显示屏
    void InitializeNv3023Display(){
        esp_lcd_panel_io_handle_t panel_io = nullptr;  // 液晶屏控制 IO 句柄
        esp_lcd_panel_handle_t panel = nullptr;  // 液晶屏驱动芯片句柄
        // 液晶屏控制 IO 初始化
        ESP_LOGD(TAG, "Install panel IO");  // 打印调试日志
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;  // 设置片选引脚
        io_config.dc_gpio_num = DISPLAY_DC_PIN;  // 设置数据/命令引脚
        io_config.spi_mode = 0;  // 设置 SPI 模式
        io_config.pclk_hz = 40 * 1000 * 1000;  // 设置像素时钟频率
        io_config.trans_queue_depth = 10;  // 设置传输队列深度
        io_config.lcd_cmd_bits = 8;  // 设置命令位宽
        io_config.lcd_param_bits = 8;  // 设置参数位宽
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &panel_io));  // 创建 SPI 面板 IO

        // 初始化液晶屏驱动芯片 NV3023
        ESP_LOGD(TAG, "Install LCD driver");  // 打印调试日志
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;  // 设置复位引脚
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR;  // 设置 RGB 元素顺序
        panel_config.bits_per_pixel = 16;  // 设置每像素位数
        ESP_ERROR_CHECK(esp_lcd_new_panel_nv3023(panel_io, &panel_config, &panel));  // 创建 NV3023 面板

        esp_lcd_panel_reset(panel);  // 复位面板
        esp_lcd_panel_init(panel);  // 初始化面板
        esp_lcd_panel_invert_color(panel, false);  // 不反转颜色
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);  // 设置是否交换 X 和 Y 轴
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);  // 设置是否镜像 X 和 Y 轴
        esp_lcd_panel_disp_on_off(panel, true);  // 打开显示
        display_ = new NV3023Display(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);  // 创建 NV3023 显示屏对象
    }

    // 初始化物联网功能，添加对 AI 可见设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();  // 获取物联网设备管理器实例
        thing_manager.AddThing(iot::CreateThing("Speaker"));  // 添加扬声器设备
        thing_manager.AddThing(iot::CreateThing("Backlight"));  // 添加背光设备
    }

public:
    // 构造函数
    magiclick_c3() : boot_button_(BOOT_BUTTON_GPIO) {
        // 把 ESP32C3 的 VDD SPI 引脚作为普通 GPIO 口使用
        esp_efuse_write_field_bit(ESP_EFUSE_VDD_SPI_AS_GPIO);

        InitializeCodecI2c();  // 初始化音频编解码器的 I2C 总线
        InitializeButtons();  // 初始化按钮
        InitializePowerSaveTimer();  // 初始化节能定时器
        InitializeSpi();  // 初始化 SPI 总线
        InitializeNv3023Display();  // 初始化 NV3023 显示屏
        InitializeIot();  // 初始化物联网功能
        GetBacklight()->RestoreBrightness();  // 恢复背光亮度
    }

    // 获取 LED 对象
    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);  // 创建单 LED 对象
        return &led;
    }

    // 获取音频编解码器对象
    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(codec_i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);  // 创建 ES8311 音频编解码器对象
        return &audio_codec;
    }

    // 获取显示屏对象
    virtual Display* GetDisplay() override {
        return display_;
    }

    // 获取背光对象
    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);  // 创建 PWM 背光对象
        return &backlight;
    }
};

DECLARE_BOARD(magiclick_c3);  // 声明 magiclick_c3 板级支持