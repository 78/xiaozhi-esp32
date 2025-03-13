#include "wifi_board.h"
#include "display/lcd_display.h"
#include "audio_codecs/es8311_audio_codec.h"
#include "application.h"
#include "button.h"
#include "led/circular_strip.h"
#include "iot/thing_manager.h"
#include "config.h"
#include "font_awesome_symbols.h"
#include "assets/lang_config.h"

#include <esp_lcd_panel_vendor.h>
#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <esp_lcd_nv3023.h>

#define TAG "magiclick_2p4"  // 定义日志标签

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

        DisplayLockGuard lock(this);  // 锁定显示以防止并发访问

        // 设置屏幕文本颜色为黑色
        auto screen = lv_disp_get_scr_act(lv_disp_get_default());
        lv_obj_set_style_text_color(screen, lv_color_black(), 0);

        // 设置容器背景色为黑色
        lv_obj_set_style_bg_color(container_, lv_color_black(), 0);

        // 设置状态栏背景色为白色，文本颜色为黑色
        lv_obj_set_style_bg_color(status_bar_, lv_color_white(), 0);
        lv_obj_set_style_text_color(network_label_, lv_color_black(), 0);
        lv_obj_set_style_text_color(notification_label_, lv_color_black(), 0);
        lv_obj_set_style_text_color(status_label_, lv_color_black(), 0);
        lv_obj_set_style_text_color(mute_label_, lv_color_black(), 0);
        lv_obj_set_style_text_color(battery_label_, lv_color_black(), 0);

        // 设置内容区背景色为黑色，文本颜色为白色
        lv_obj_set_style_bg_color(content_, lv_color_black(), 0);
        lv_obj_set_style_border_width(content_, 0, 0);
        lv_obj_set_style_text_color(emotion_label_, lv_color_white(), 0);
        lv_obj_set_style_text_color(chat_message_label_, lv_color_white(), 0);
    }
};

// magiclick_2p4 类继承自 WifiBoard，用于管理整个硬件板的功能
class magiclick_2p4 : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;  // I2C 总线句柄，用于音频编解码器
    Button main_button_;  // 主按钮
    Button left_button_;  // 左按钮
    Button right_button_;  // 右按钮
    NV3023Display* display_;  // 显示对象

    // 初始化音频编解码器的 I2C 总线
    void InitializeCodecI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,  // 使用 I2C 端口 0
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,  // SDA 引脚
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,  // SCL 引脚
            .clk_source = I2C_CLK_SRC_DEFAULT,  // 使用默认时钟源
            .glitch_ignore_cnt = 7,  // 忽略毛刺计数
            .intr_priority = 0,  // 中断优先级
            .trans_queue_depth = 0,  // 传输队列深度
            .flags = {
                .enable_internal_pullup = 1,  // 启用内部上拉
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));  // 初始化 I2C 总线
    }

    // 初始化按钮功能
    void InitializeButtons() {
        // 主按钮按下时开始监听
        main_button_.OnPressDown([this]() {
            Application::GetInstance().StartListening();
        });
        // 主按钮释放时停止监听
        main_button_.OnPressUp([this]() {
            Application::GetInstance().StopListening();
        });

        // 左按钮点击时降低音量
        left_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();  // 重置 WiFi 配置
            }
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;  // 音量减少 10
            if (volume < 0) {
                volume = 0;  // 音量最小为 0
            }
            codec->SetOutputVolume(volume);  // 设置音量
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));  // 显示音量通知
        });

        // 左按钮长按时静音
        left_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(0);  // 设置音量为 0
            GetDisplay()->ShowNotification(Lang::Strings::MUTED);  // 显示静音通知
        });

        // 右按钮点击时增加音量
        right_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;  // 音量增加 10
            if (volume > 100) {
                volume = 100;  // 音量最大为 100
            }
            codec->SetOutputVolume(volume);  // 设置音量
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));  // 显示音量通知
        });

        // 右按钮长按时设置音量为最大
        right_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(100);  // 设置音量为 100
            GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);  // 显示最大音量通知
        });
    }

    // 初始化 LED 电源
    void InitializeLedPower() {
        gpio_reset_pin(BUILTIN_LED_POWER);  // 重置 LED 电源引脚
        gpio_set_direction(BUILTIN_LED_POWER, GPIO_MODE_OUTPUT);  // 设置引脚为输出模式
        gpio_set_level(BUILTIN_LED_POWER, BUILTIN_LED_POWER_OUTPUT_INVERT ? 0 : 1);  // 设置引脚电平
    }

    // 初始化 SPI 总线
    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_SDA_PIN;  // MOSI 引脚
        buscfg.miso_io_num = GPIO_NUM_NC;  // MISO 引脚未使用
        buscfg.sclk_io_num = DISPLAY_SCL_PIN;  // SCLK 引脚
        buscfg.quadwp_io_num = GPIO_NUM_NC;  // QUADWP 引脚未使用
        buscfg.quadhd_io_num = GPIO_NUM_NC;  // QUADHD 引脚未使用
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);  // 最大传输大小
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));  // 初始化 SPI 总线
    }

    // 初始化 NV3023 显示屏
    void InitializeNv3023Display(){
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        // 初始化液晶屏控制 IO
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;  // CS 引脚
        io_config.dc_gpio_num = DISPLAY_DC_PIN;  // DC 引脚
        io_config.spi_mode = 0;  // SPI 模式
        io_config.pclk_hz = 40 * 1000 * 1000;  // 像素时钟频率
        io_config.trans_queue_depth = 10;  // 传输队列深度
        io_config.lcd_cmd_bits = 8;  // 命令位宽
        io_config.lcd_param_bits = 8;  // 参数位宽
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));  // 初始化 SPI 面板 IO

        // 初始化液晶屏驱动芯片 NV3023
        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;  // 复位引脚
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR;  // RGB 元素顺序
        panel_config.bits_per_pixel = 16;  // 每像素位数
        ESP_ERROR_CHECK(esp_lcd_new_panel_nv3023(panel_io, &panel_config, &panel));  // 初始化 NV3023 面板

        esp_lcd_panel_reset(panel);  // 复位面板
        esp_lcd_panel_init(panel);  // 初始化面板
        esp_lcd_panel_invert_color(panel, false);  // 不反转颜色
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);  // 交换 XY 轴
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);  // 镜像显示
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));  // 打开显示
        display_ = new NV3023Display(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);  // 创建显示对象
    }

    // 初始化物联网功能，添加对 AI 可见设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));  // 添加扬声器设备
        thing_manager.AddThing(iot::CreateThing("Backlight"));  // 添加背光设备
    }

public:
    // 构造函数，初始化所有硬件组件
    magiclick_2p4() :
        main_button_(MAIN_BUTTON_GPIO),
        left_button_(LEFT_BUTTON_GPIO), 
        right_button_(RIGHT_BUTTON_GPIO) {
        InitializeCodecI2c();  // 初始化音频编解码器 I2C
        InitializeButtons();  // 初始化按钮
        InitializeLedPower();  // 初始化 LED 电源
        InitializeSpi();  // 初始化 SPI
        InitializeNv3023Display();  // 初始化 NV3023 显示屏
        InitializeIot();  // 初始化物联网
        GetBacklight()->RestoreBrightness();  // 恢复背光亮度
    }

    // 获取 LED 对象
    virtual Led* GetLed() override {
        static CircularStrip led(BUILTIN_LED_GPIO, BUILTIN_LED_NUM);  // 创建环形 LED 条
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

// 声明 magiclick_2p4 板
DECLARE_BOARD(magiclick_2p4);