#include "wifi_board.h"
#include "sensecap_audio_codec.h"
#include "display/lcd_display.h"
#include "font_awesome_symbols.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "iot/thing_manager.h"
#include "power_save_timer.h"

#include <esp_log.h>
#include "esp_check.h"
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_spd2010.h>
#include <driver/spi_master.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <wifi_station.h>
#include <iot_button.h>
#include <esp_io_expander_tca95xx_16bit.h>
#include <esp_sleep.h>

#define TAG "sensecap_watcher"  // 定义日志标签

LV_FONT_DECLARE(font_puhui_30_4);  // 声明字体
LV_FONT_DECLARE(font_awesome_30_4);

// SensecapWatcher 类继承自 WifiBoard，用于实现 Sensecap Watcher 设备的各项功能
class SensecapWatcher : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;  // I2C 总线句柄
    LcdDisplay* display_;  // LCD 显示对象指针
    esp_io_expander_handle_t io_exp_handle;  // IO 扩展器句柄
    button_handle_t btns;  // 按钮句柄
    PowerSaveTimer* power_save_timer_;  // 节能定时器对象指针
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;  // LCD 面板 IO 句柄
    esp_lcd_panel_handle_t panel_ = nullptr;  // LCD 面板句柄

    // 初始化节能定时器
    void InitializePowerSaveTimer() {
        power_save_timer_ = new PowerSaveTimer(-1, 60, 300);  // 创建节能定时器对象
        power_save_timer_->OnEnterSleepMode([this]() {  // 设置进入睡眠模式时的回调函数
            ESP_LOGI(TAG, "Enabling sleep mode");
            auto display = GetDisplay();
            display->SetChatMessage("system", "");  // 清空聊天消息
            display->SetEmotion("sleepy");  // 设置表情为睡眠状态
            GetBacklight()->SetBrightness(10);  // 降低背光亮度
        });
        power_save_timer_->OnExitSleepMode([this]() {  // 设置退出睡眠模式时的回调函数
            auto display = GetDisplay();
            display->SetChatMessage("system", "");  // 清空聊天消息
            display->SetEmotion("neutral");  // 设置表情为中性状态
            GetBacklight()->RestoreBrightness();  // 恢复背光亮度
        });
        power_save_timer_->OnShutdownRequest([this]() {  // 设置关机请求时的回调函数
            ESP_LOGI(TAG, "Shutting down");
            IoExpanderSetLevel(BSP_PWR_LCD, 0);  // 关闭 LCD 电源
            IoExpanderSetLevel(BSP_PWR_SYSTEM, 0);  // 关闭系统电源
        });
        power_save_timer_->SetEnabled(true);  // 启用节能定时器
    }

    // 初始化 I2C 总线
    void InitializeI2c() {
        // 配置 I2C 总线
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)0,
            .sda_io_num = BSP_GENERAL_I2C_SDA,
            .scl_io_num = BSP_GENERAL_I2C_SCL,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));  // 初始化 I2C 总线
    }

    // 设置 IO 扩展器的电平
    esp_err_t IoExpanderSetLevel(uint16_t pin_mask, uint8_t level) {
        return esp_io_expander_set_level(io_exp_handle, pin_mask, level);
    }

    // 获取 IO 扩展器的电平
    uint8_t IoExpanderGetLevel(uint16_t pin_mask) {
        uint32_t pin_val = 0;
        esp_io_expander_get_level(io_exp_handle, DRV_IO_EXP_INPUT_MASK, &pin_val);
        pin_mask &= DRV_IO_EXP_INPUT_MASK;
        return (uint8_t)((pin_val & pin_mask) ? 1 : 0);
    }

    // 初始化 IO 扩展器
    void InitializeExpander() {
        esp_err_t ret = ESP_OK;
        esp_io_expander_new_i2c_tca95xx_16bit(i2c_bus_, ESP_IO_EXPANDER_I2C_TCA9555_ADDRESS_001, &io_exp_handle);

        ret |= esp_io_expander_set_dir(io_exp_handle, DRV_IO_EXP_INPUT_MASK, IO_EXPANDER_INPUT);  // 设置输入方向
        ret |= esp_io_expander_set_dir(io_exp_handle, DRV_IO_EXP_OUTPUT_MASK, IO_EXPANDER_OUTPUT);  // 设置输出方向
        ret |= esp_io_expander_set_level(io_exp_handle, DRV_IO_EXP_OUTPUT_MASK, 0);  // 设置输出电平
        ret |= esp_io_expander_set_level(io_exp_handle, BSP_PWR_SYSTEM, 1);  // 打开系统电源
        vTaskDelay(100 / portTICK_PERIOD_MS);
        ret |= esp_io_expander_set_level(io_exp_handle, BSP_PWR_START_UP, 1);  // 打开启动电源
        vTaskDelay(50 / portTICK_PERIOD_MS);
    
        uint32_t pin_val = 0;
        ret |= esp_io_expander_get_level(io_exp_handle, DRV_IO_EXP_INPUT_MASK, &pin_val);  // 获取输入电平
        ESP_LOGI(TAG, "IO expander initialized: %x", DRV_IO_EXP_OUTPUT_MASK | (uint16_t)pin_val);  // 打印初始化信息
    
        assert(ret == ESP_OK);  // 确保初始化成功
    }

    // 初始化按钮
    void InitializeButton() {
        button_config_t btn_config = {
            .type = BUTTON_TYPE_CUSTOM,
            .long_press_time = 2000,
            .short_press_time = 50,
            .custom_button_config = {
                .active_level = 0,
                .button_custom_init =nullptr,
                .button_custom_get_key_value = [](void *param) -> uint8_t {
                    auto self = static_cast<SensecapWatcher*>(param);
                    return self->IoExpanderGetLevel(BSP_KNOB_BTN);  // 获取按钮电平
                },
                .button_custom_deinit = nullptr,
                .priv = this,
            },
        };
        btns = iot_button_create(&btn_config);  // 创建按钮对象
        iot_button_register_cb(btns, BUTTON_SINGLE_CLICK, [](void* button_handle, void* usr_data) {  // 注册单击回调
            auto self = static_cast<SensecapWatcher*>(usr_data);
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                self->ResetWifiConfiguration();  // 重置 WiFi 配置
            }
            self->power_save_timer_->WakeUp();  // 唤醒设备
            app.ToggleChatState();  // 切换聊天状态
        }, this);
        iot_button_register_cb(btns, BUTTON_LONG_PRESS_START, [](void* button_handle, void* usr_data) {  // 注册长按回调
            auto self = static_cast<SensecapWatcher*>(usr_data);
            bool is_charging = (self->IoExpanderGetLevel(BSP_PWR_VBUS_IN_DET) == 0);  // 检查是否在充电
            if (is_charging) {
                ESP_LOGI(TAG, "charging");  // 打印充电信息
            } else {
                self->IoExpanderSetLevel(BSP_PWR_LCD, 0);  // 关闭 LCD 电源
                self->IoExpanderSetLevel(BSP_PWR_SYSTEM, 0);  // 关闭系统电源
            }
        }, this);
    }

    // 初始化 SPI 总线
    void InitializeSpi() {
        ESP_LOGI(TAG, "Initialize QSPI bus");

        spi_bus_config_t qspi_cfg = {0};
        qspi_cfg.sclk_io_num = BSP_SPI3_HOST_PCLK;
        qspi_cfg.data0_io_num = BSP_SPI3_HOST_DATA0;
        qspi_cfg.data1_io_num = BSP_SPI3_HOST_DATA1;
        qspi_cfg.data2_io_num = BSP_SPI3_HOST_DATA2;
        qspi_cfg.data3_io_num = BSP_SPI3_HOST_DATA3;
        qspi_cfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * DRV_LCD_BITS_PER_PIXEL / 8 / CONFIG_BSP_LCD_SPI_DMA_SIZE_DIV;
    
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &qspi_cfg, SPI_DMA_CH_AUTO));  // 初始化 SPI 总线
    }

    // 初始化 SPD2010 显示屏
    void Initializespd2010Display() {
        ESP_LOGI(TAG, "Install panel IO");
        const esp_lcd_panel_io_spi_config_t io_config = {
            .cs_gpio_num = BSP_LCD_SPI_CS,
            .dc_gpio_num = -1,
            .spi_mode = 3,
            .pclk_hz = DRV_LCD_PIXEL_CLK_HZ,
            .trans_queue_depth = 2,
            .lcd_cmd_bits = DRV_LCD_CMD_BITS,
            .lcd_param_bits = DRV_LCD_PARAM_BITS,
            .flags = {
                .quad_mode = true,
            },
        };
        spd2010_vendor_config_t vendor_config = {
            .flags = {
                .use_qspi_interface = 1,
            },
        };
        esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BSP_LCD_SPI_NUM, &io_config, &panel_io_);  // 创建 LCD 面板 IO
    
        ESP_LOGD(TAG, "Install LCD driver");
        const esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = BSP_LCD_GPIO_RST, // 与触摸复位共享
            .rgb_ele_order = DRV_LCD_RGB_ELEMENT_ORDER,
            .bits_per_pixel = DRV_LCD_BITS_PER_PIXEL,
            .vendor_config = &vendor_config,
        };
        esp_lcd_new_panel_spd2010(panel_io_, &panel_config, &panel_);  // 创建 LCD 面板

        esp_lcd_panel_reset(panel_);  // 复位 LCD 面板
        esp_lcd_panel_init(panel_);  // 初始化 LCD 面板
        esp_lcd_panel_mirror(panel_, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);  // 设置镜像
        esp_lcd_panel_disp_on_off(panel_, true);  // 打开显示
        
        display_ = new SpiLcdDisplay(panel_io_, panel_,
            DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
            {
                .text_font = &font_puhui_30_4,
                .icon_font = &font_awesome_30_4,
                .emoji_font = font_emoji_64_init(),
            });  // 创建 LCD 显示对象
    }

    // 初始化物联网设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));  // 添加扬声器设备
        thing_manager.AddThing(iot::CreateThing("Backlight"));  // 添加背光设备
    }

public:
    SensecapWatcher(){
        ESP_LOGI(TAG, "Initialize Sensecap Watcher");
        InitializePowerSaveTimer();  // 初始化节能定时器
        InitializeI2c();  // 初始化 I2C 总线
        InitializeSpi();  // 初始化 SPI 总线
        InitializeExpander();  // 初始化 IO 扩展器
        InitializeButton();  // 初始化按钮
        Initializespd2010Display();  // 初始化 SPD2010 显示屏
        InitializeIot();  // 初始化物联网设备
        GetBacklight()->RestoreBrightness();  // 恢复背光亮度
    }

    // 获取音频编解码器
    virtual AudioCodec* GetAudioCodec() override {
        static SensecapAudioCodec audio_codec(
            i2c_bus_, 
            AUDIO_INPUT_SAMPLE_RATE, 
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, 
            AUDIO_I2S_GPIO_BCLK, 
            AUDIO_I2S_GPIO_WS, 
            AUDIO_I2S_GPIO_DOUT, 
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, 
            AUDIO_CODEC_ES8311_ADDR, 
            AUDIO_CODEC_ES7243E_ADDR, 
            AUDIO_INPUT_REFERENCE);
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

    // 设置节能模式
    virtual void SetPowerSaveMode(bool enabled) override {
        if (!enabled) {
            power_save_timer_->WakeUp();  // 唤醒设备
        }
        WifiBoard::SetPowerSaveMode(enabled);  // 调用基类的节能模式设置
    }
};

DECLARE_BOARD(SensecapWatcher);  // 声明 SensecapWatcher 为板级对象