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
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_gc9a01.h>
#include <esp_efuse_table.h>

#define TAG "magiclick_c3_v2"  // 定义日志标签

LV_FONT_DECLARE(font_puhui_16_4);  // 声明字体
LV_FONT_DECLARE(font_awesome_16_4);

// GC9107Display 类继承自 SpiLcdDisplay，用于管理 GC9107 液晶显示屏
class GC9107Display : public SpiLcdDisplay {
public:
    // 构造函数，初始化显示屏并设置样式
    GC9107Display(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                int width, int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y, bool swap_xy)
        : SpiLcdDisplay(panel_io, panel, width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy, 
                    {
                        .text_font = &font_puhui_16_4,  // 设置文本字体
                        .icon_font = &font_awesome_16_4,  // 设置图标字体
                        .emoji_font = font_emoji_32_init(),  // 设置表情字体
                    }) {

        DisplayLockGuard lock(this);  // 加锁，确保线程安全

        // 设置屏幕文本颜色为黑色
        auto screen = lv_disp_get_scr_act(lv_disp_get_default());
        lv_obj_set_style_text_color(screen, lv_color_black(), 0);

        // 设置容器背景色为黑色
        lv_obj_set_style_bg_color(container_, lv_color_black(), 0);

        // 设置状态栏背景色和文本颜色
        lv_obj_set_style_bg_color(status_bar_, lv_color_make(0x1e, 0x90, 0xff), 0);
        lv_obj_set_style_text_color(network_label_, lv_color_black(), 0);
        lv_obj_set_style_text_color(notification_label_, lv_color_black(), 0);
        lv_obj_set_style_text_color(status_label_, lv_color_black(), 0);
        lv_obj_set_style_text_color(mute_label_, lv_color_black(), 0);
        lv_obj_set_style_text_color(battery_label_, lv_color_black(), 0);

        // 设置内容区背景色和文本颜色
        lv_obj_set_style_bg_color(content_, lv_color_black(), 0);
        lv_obj_set_style_border_width(content_, 0, 0);
        lv_obj_set_style_text_color(emotion_label_, lv_color_white(), 0);
        lv_obj_set_style_text_color(chat_message_label_, lv_color_white(), 0);
    }   
};

// GC9107 液晶屏初始化命令
static const gc9a01_lcd_init_cmd_t gc9107_lcd_init_cmds[] = {
    //  {cmd, { data }, data_size, delay_ms}
    {0xfe, (uint8_t[]){0x00}, 0, 0},
    {0xef, (uint8_t[]){0x00}, 0, 0},
    {0xb0, (uint8_t[]){0xc0}, 1, 0},
    {0xb1, (uint8_t[]){0x80}, 1, 0},
    {0xb2, (uint8_t[]){0x27}, 1, 0},
    {0xb3, (uint8_t[]){0x13}, 1, 0},
    {0xb6, (uint8_t[]){0x19}, 1, 0},
    {0xb7, (uint8_t[]){0x05}, 1, 0},
    {0xac, (uint8_t[]){0xc8}, 1, 0},
    {0xab, (uint8_t[]){0x0f}, 1, 0},
    {0x3a, (uint8_t[]){0x05}, 1, 0},
    {0xb4, (uint8_t[]){0x04}, 1, 0},
    {0xa8, (uint8_t[]){0x08}, 1, 0},
    {0xb8, (uint8_t[]){0x08}, 1, 0},
    {0xea, (uint8_t[]){0x02}, 1, 0},
    {0xe8, (uint8_t[]){0x2A}, 1, 0},
    {0xe9, (uint8_t[]){0x47}, 1, 0},
    {0xe7, (uint8_t[]){0x5f}, 1, 0},
    {0xc6, (uint8_t[]){0x21}, 1, 0},
    {0xc7, (uint8_t[]){0x15}, 1, 0},
    {0xf0,
    (uint8_t[]){0x1D, 0x38, 0x09, 0x4D, 0x92, 0x2F, 0x35, 0x52, 0x1E, 0x0C,
                0x04, 0x12, 0x14, 0x1f},
    14, 0},
    {0xf1,
    (uint8_t[]){0x16, 0x40, 0x1C, 0x54, 0xA9, 0x2D, 0x2E, 0x56, 0x10, 0x0D,
                0x0C, 0x1A, 0x14, 0x1E},
    14, 0},
    {0xf4, (uint8_t[]){0x00, 0x00, 0xFF}, 3, 0},
    {0xba, (uint8_t[]){0xFF, 0xFF}, 2, 0},
};

// magiclick_c3_v2 类继承自 WifiBoard，用于管理整个硬件平台
class magiclick_c3_v2 : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;  // I2C 总线句柄
    Button boot_button_;  // 启动按钮
    GC9107Display* display_;  // 显示屏对象
    PowerSaveTimer* power_save_timer_;  // 节能定时器

    // 初始化节能定时器
    void InitializePowerSaveTimer() {
        power_save_timer_ = new PowerSaveTimer(160);  // 创建节能定时器
        power_save_timer_->OnEnterSleepMode([this]() {  // 进入睡眠模式时的回调
            ESP_LOGI(TAG, "Enabling sleep mode");
            auto display = GetDisplay();
            display->SetChatMessage("system", "");
            display->SetEmotion("sleepy");
            GetBacklight()->SetBrightness(10);  // 降低背光亮度
            
            auto codec = GetAudioCodec();
            codec->EnableInput(false);  // 禁用音频输入
        });
        power_save_timer_->OnExitSleepMode([this]() {  // 退出睡眠模式时的回调
            auto codec = GetAudioCodec();
            codec->EnableInput(true);  // 启用音频输入
            
            auto display = GetDisplay();
            display->SetChatMessage("system", "");
            display->SetEmotion("neutral");
            GetBacklight()->RestoreBrightness();  // 恢复背光亮度
        });
        power_save_timer_->SetEnabled(true);  // 启用节能定时器
    }

    // 初始化音频编解码器的 I2C 总线
    void InitializeCodecI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,  // 启用内部上拉电阻
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));  // 初始化 I2C 总线
    }

    // 初始化按钮
    void InitializeButtons() {
        boot_button_.OnClick([this]() {  // 按钮点击事件
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();  // 重置 WiFi 配置
            }
        });
        boot_button_.OnPressDown([this]() {  // 按钮按下事件
            power_save_timer_->WakeUp();  // 唤醒设备
            Application::GetInstance().StartListening();  // 开始监听
        });
        boot_button_.OnPressUp([this]() {  // 按钮释放事件
            Application::GetInstance().StopListening();  // 停止监听
        });
    }

    // 初始化 SPI 总线
    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_SDA_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_SCL_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));  // 初始化 SPI 总线
    }

    // 初始化 GC9107 显示屏
    void InitializeGc9107Display(){
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        // 液晶屏控制 IO 初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = 0;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &panel_io));  // 初始化 SPI 面板 IO

        // 初始化液晶屏驱动芯片 GC9107
        ESP_LOGD(TAG, "Install LCD driver");        
        gc9a01_vendor_config_t gc9107_vendor_config = {
            .init_cmds = gc9107_lcd_init_cmds,
            .init_cmds_size = sizeof(gc9107_lcd_init_cmds) / sizeof(gc9a01_lcd_init_cmd_t),
        };
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        panel_config.vendor_config = &gc9107_vendor_config;

        esp_lcd_new_panel_gc9a01(panel_io, &panel_config, &panel);  // 初始化 GC9107 面板

        esp_lcd_panel_reset(panel);  // 重置面板
        esp_lcd_panel_init(panel);  // 初始化面板
        esp_lcd_panel_invert_color(panel, false);  // 不反转颜色
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);  // 交换 X 和 Y 轴
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);  // 镜像显示
        esp_lcd_panel_disp_on_off(panel, true);  // 打开显示
        display_ = new GC9107Display(panel_io, panel,
            DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);  // 创建 GC9107 显示对象
    }

    // 初始化物联网设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));  // 添加扬声器设备
        thing_manager.AddThing(iot::CreateThing("Backlight"));  // 添加背光设备
    }

public:
    // 构造函数，初始化硬件平台
    magiclick_c3_v2() : boot_button_(BOOT_BUTTON_GPIO) {
        // 把 ESP32C3 的 VDD SPI 引脚作为普通 GPIO 口使用
        esp_efuse_write_field_bit(ESP_EFUSE_VDD_SPI_AS_GPIO);

        InitializeCodecI2c();  // 初始化音频编解码器的 I2C 总线
        InitializeButtons();  // 初始化按钮
        InitializePowerSaveTimer();  // 初始化节能定时器
        InitializeSpi();  // 初始化 SPI 总线
        InitializeGc9107Display();  // 初始化 GC9107 显示屏
        InitializeIot();  // 初始化物联网设备
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

    // 获取显示对象
    virtual Display* GetDisplay() override {
        return display_;
    }

    // 获取背光对象
    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);  // 创建 PWM 背光对象
        return &backlight;
    }
};

// 声明 magiclick_c3_v2 板子
DECLARE_BOARD(magiclick_c3_v2);