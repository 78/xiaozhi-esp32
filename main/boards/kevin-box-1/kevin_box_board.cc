#include "ml307_board.h"  // 引入ML307板级支持库，提供4G模块相关功能
#include "audio_codecs/box_audio_codec.h"  // 引入Box音频编解码器库，用于音频处理
#include "display/ssd1306_display.h"  // 引入SSD1306显示库，提供OLED显示功能
#include "application.h"  // 引入应用程序库，提供应用程序管理功能
#include "button.h"  // 引入按钮库，用于处理按钮事件
#include "config.h"  // 引入配置文件，包含硬件配置信息
#include "iot/thing_manager.h"  // 引入IoT设备管理库，用于管理IoT设备
#include "led/single_led.h"  // 引入单LED库，用于控制LED
#include "assets/lang_config.h"  // 引入语言配置文件，用于多语言支持

#include <esp_log.h>  // 引入ESP32日志库，用于记录日志信息
#include <esp_spiffs.h>  // 引入SPIFFS库，用于文件系统操作
#include <driver/gpio.h>  // 引入GPIO驱动库，用于GPIO控制
#include <driver/i2c_master.h>  // 引入I2C主设备库，用于I2C通信

#define TAG "KevinBoxBoard"  // 定义日志标签，用于标识日志来源

LV_FONT_DECLARE(font_puhui_14_1);  // 声明普黑字体
LV_FONT_DECLARE(font_awesome_14_1);  // 声明Font Awesome字体

// KevinBox板子类，继承自Ml307Board
class KevinBoxBoard : public Ml307Board {
private:
    i2c_master_bus_handle_t display_i2c_bus_;  // 显示I2C总线句柄
    i2c_master_bus_handle_t codec_i2c_bus_;  // 音频编解码器I2C总线句柄
    Button boot_button_;  // 启动按钮对象
    Button volume_up_button_;  // 音量增加按钮对象
    Button volume_down_button_;  // 音量减少按钮对象

    // 挂载存储分区
    void MountStorage() {
        esp_vfs_spiffs_conf_t conf = {
            .base_path = "/storage",  // 挂载路径
            .partition_label = "storage",  // 分区标签
            .max_files = 5,  // 最大文件数
            .format_if_mount_failed = true,  // 挂载失败时格式化
        };
        esp_vfs_spiffs_register(&conf);  // 注册SPIFFS文件系统
    }

    // 启用4G模块
    void Enable4GModule() {
        gpio_config_t ml307_enable_config = {
            .pin_bit_mask = (1ULL << 15) | (1ULL << 18),  // GPIO15和GPIO18
            .mode = GPIO_MODE_OUTPUT,  // 输出模式
            .pull_up_en = GPIO_PULLUP_DISABLE,  // 禁用上拉
            .pull_down_en = GPIO_PULLDOWN_DISABLE,  // 禁用下拉
            .intr_type = GPIO_INTR_DISABLE,  // 禁用中断
        };
        gpio_config(&ml307_enable_config);  // 配置GPIO
        gpio_set_level(GPIO_NUM_15, 1);  // 设置GPIO15为高电平
        gpio_set_level(GPIO_NUM_18, 1);  // 设置GPIO18为高电平
    }

    // 初始化显示I2C总线
    void InitializeDisplayI2c() {
        i2c_master_bus_config_t bus_config = {
            .i2c_port = (i2c_port_t)0,  // I2C端口号
            .sda_io_num = DISPLAY_SDA_PIN,  // SDA引脚
            .scl_io_num = DISPLAY_SCL_PIN,  // SCL引脚
            .clk_source = I2C_CLK_SRC_DEFAULT,  // I2C时钟源
            .glitch_ignore_cnt = 7,  // 毛刺忽略计数
            .intr_priority = 0,  // 中断优先级
            .trans_queue_depth = 0,  // 传输队列深度
            .flags = {
                .enable_internal_pullup = 1,  // 启用内部上拉
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &display_i2c_bus_));  // 初始化I2C总线
    }

    // 初始化音频编解码器I2C总线
    void InitializeCodecI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)1,  // I2C端口号
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,  // SDA引脚
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,  // SCL引脚
            .clk_source = I2C_CLK_SRC_DEFAULT,  // I2C时钟源
            .glitch_ignore_cnt = 7,  // 毛刺忽略计数
            .intr_priority = 0,  // 中断优先级
            .trans_queue_depth = 0,  // 传输队列深度
            .flags = {
                .enable_internal_pullup = 1,  // 启用内部上拉
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));  // 初始化I2C总线
    }

    // 初始化按钮
    void InitializeButtons() {
        boot_button_.OnPressDown([this]() {
            Application::GetInstance().StartListening();  // 开始监听
        });
        boot_button_.OnPressUp([this]() {
            Application::GetInstance().StopListening();  // 停止监听
        });

        volume_up_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;  // 增加音量
            if (volume > 100) {
                volume = 100;  // 音量上限为100
            }
            codec->SetOutputVolume(volume);  // 设置音量
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));  // 显示音量通知
        });

        volume_up_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(100);  // 设置音量为最大值
            GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);  // 显示最大音量通知
        });

        volume_down_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;  // 减少音量
            if (volume < 0) {
                volume = 0;  // 音量下限为0
            }
            codec->SetOutputVolume(volume);  // 设置音量
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));  // 显示音量通知
        });

        volume_down_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(0);  // 设置音量为0（静音）
            GetDisplay()->ShowNotification(Lang::Strings::MUTED);  // 显示静音通知
        });
    }

    // 初始化物联网设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));  // 添加扬声器设备
    }

public:
    // 构造函数
    KevinBoxBoard() : Ml307Board(ML307_TX_PIN, ML307_RX_PIN, 4096),
        boot_button_(BOOT_BUTTON_GPIO),
        volume_up_button_(VOLUME_UP_BUTTON_GPIO),
        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO) {
        InitializeDisplayI2c();  // 初始化显示I2C总线
        InitializeCodecI2c();  // 初始化音频编解码器I2C总线
        MountStorage();  // 挂载存储分区
        Enable4GModule();  // 启用4G模块

        InitializeButtons();  // 初始化按钮
        InitializeIot();  // 初始化物联网设备
    }
    
    // 获取LED对象
    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);  // 创建单LED对象
        return &led;
    }

    // 获取音频编解码器
    virtual AudioCodec* GetAudioCodec() override {
        static BoxAudioCodec audio_codec(codec_i2c_bus_, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR, AUDIO_CODEC_ES7210_ADDR, AUDIO_INPUT_REFERENCE);  // 创建Box音频编解码器对象
        return &audio_codec;
    }

    // 获取显示对象
    virtual Display* GetDisplay() override {
        static Ssd1306Display display(display_i2c_bus_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y,
                                    &font_puhui_14_1, &font_awesome_14_1);  // 创建SSD1306显示对象
        return &display;
    }
};

DECLARE_BOARD(KevinBoxBoard);  // 声明KevinBox板子