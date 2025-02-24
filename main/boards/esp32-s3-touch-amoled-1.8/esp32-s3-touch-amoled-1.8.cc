#include "wifi_board.h"
#include "display/lcd_display.h"
#include "esp_lcd_sh8601.h"
#include "font_awesome_symbols.h"

#include "audio_codecs/es8311_audio_codec.h"
#include "application.h"
#include "button.h"
#include "led/single_led.h"
#include "iot/thing_manager.h"
#include "config.h"
#include "axp2101.h"
#include "i2c_device.h"
#include <wifi_station.h>

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <driver/spi_master.h>
#include "esp_io_expander_tca9554.h"

#define TAG "waveshare_amoled_1_8"

LV_FONT_DECLARE(font_puhui_30_4);
LV_FONT_DECLARE(font_awesome_30_4);

static const sh8601_lcd_init_cmd_t vendor_specific_init[] = {
    {0x11, (uint8_t[]){0x00}, 0, 120},
    {0x44, (uint8_t[]){0x01, 0xD1}, 2, 0},
    {0x35, (uint8_t[]){0x00}, 1, 0},
    {0x53, (uint8_t[]){0x20}, 1, 10},
    {0x2A, (uint8_t[]){0x00, 0x00, 0x01, 0x6F}, 4, 0},
    {0x2B, (uint8_t[]){0x00, 0x00, 0x01, 0xBF}, 4, 0},
    {0x51, (uint8_t[]){0x00}, 1, 10},
    {0x29, (uint8_t[]){0x00}, 0, 10},
    {0x51, (uint8_t[]){0xFF}, 1, 0},
};

// 在waveshare_amoled_1_8类之前添加新的显示类
class CustomLcdDisplay : public SpiLcdDisplay {
public:
    CustomLcdDisplay(esp_lcd_panel_io_handle_t io_handle, 
                    esp_lcd_panel_handle_t panel_handle,
                    gpio_num_t backlight_pin,
                    bool backlight_output_invert,
                    int width,
                    int height,
                    int offset_x,
                    int offset_y,
                    bool mirror_x,
                    bool mirror_y,
                    bool swap_xy) 
        : SpiLcdDisplay(io_handle, panel_handle, backlight_pin, backlight_output_invert,
                    width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy,
                    {
                        .text_font = &font_puhui_30_4,
                        .icon_font = &font_awesome_30_4,
                        .emoji_font = font_emoji_64_init(),
                    }) {

        DisplayLockGuard lock(this);
        // 由于屏幕是带圆角的，所以状态栏需要增加左右内边距
        lv_obj_set_style_pad_left(status_bar_, LV_HOR_RES * 0.1, 0);
        lv_obj_set_style_pad_right(status_bar_, LV_HOR_RES * 0.1, 0);
    }
};

class waveshare_amoled_1_8 : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;
    Axp2101* axp2101_ = nullptr;
    esp_timer_handle_t power_save_timer_ = nullptr;

    Button boot_button_;
    LcdDisplay* display_;
    esp_io_expander_handle_t io_expander = NULL;

    void InitializeCodecI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
    }

    void InitializeTca9554(void)
    {
        esp_err_t ret = esp_io_expander_new_i2c_tca9554(codec_i2c_bus_, I2C_ADDRESS, &io_expander);
        if(ret != ESP_OK)
            ESP_LOGE(TAG, "TCA9554 create returned error");        
        ret = esp_io_expander_set_dir(io_expander, IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1 |IO_EXPANDER_PIN_NUM_2, IO_EXPANDER_OUTPUT);
        ESP_ERROR_CHECK(ret);
        ret = esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1|IO_EXPANDER_PIN_NUM_2, 1);
        ESP_ERROR_CHECK(ret);
        vTaskDelay(pdMS_TO_TICKS(100));
        ret = esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1|IO_EXPANDER_PIN_NUM_2, 0);
        ESP_ERROR_CHECK(ret);
        vTaskDelay(pdMS_TO_TICKS(300));
        ret = esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1|IO_EXPANDER_PIN_NUM_2, 1);
        ESP_ERROR_CHECK(ret);
    }

    void InitializeAxp2101() {
        // 使用 ESP_LOGI 宏记录信息日志，TAG 是日志标签，"Init AXP2101" 是日志信息
        ESP_LOGI(TAG, "Init AXP2101");
        // 创建一个新的 Axp2101 对象，该对象通过 I2C 总线 i2c_bus_ 和设备地址 0x34 进行初始化
        // axp2101_ 是一个指向 Axp2101 对象的指针
        axp2101_ = new Axp2101(codec_i2c_bus_, 0x34);
    }

    void InitializePowerSaveTimer() {
        esp_timer_create_args_t power_save_timer_args = {
            .callback = [](void *arg) {
                auto board = static_cast<waveshare_amoled_1_8*>(arg);
                board->PowerSaveCheck();
            },
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "Power Save Timer",
            .skip_unhandled_events = false,
        };
        ESP_ERROR_CHECK(esp_timer_create(&power_save_timer_args, &power_save_timer_));
        ESP_ERROR_CHECK(esp_timer_start_periodic(power_save_timer_, 1000000));
    }

    void PowerSaveCheck() {
        // 电池放电模式下，如果待机超过一定时间，则自动关机
        const int seconds_to_shutdown = 600;
        static int seconds = 0;
        if (Application::GetInstance().GetDeviceState() != kDeviceStateIdle) {
            seconds = 0;
            return;
        }
        if (!axp2101_->IsDischarging()) {
            seconds = 0;
            return;
        }
        
        seconds++;
        if (seconds >= seconds_to_shutdown) {
            axp2101_->PowerOff();
        }
    }
    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.sclk_io_num = GPIO_NUM_11;
        buscfg.data0_io_num = GPIO_NUM_4;
        buscfg.data1_io_num = GPIO_NUM_5;
        buscfg.data2_io_num = GPIO_NUM_6;
        buscfg.data3_io_num = GPIO_NUM_7;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        buscfg.flags = SPICOMMON_BUSFLAG_QUAD;
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState();
        });
    }

    void InitializeSH8601Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = SH8601_PANEL_IO_QSPI_CONFIG(
            EXAMPLE_PIN_NUM_LCD_CS,
            nullptr,
            nullptr
        );
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片
        ESP_LOGD(TAG, "Install LCD driver");
        const sh8601_vendor_config_t vendor_config = {
            .init_cmds = &vendor_specific_init[0],
            .init_cmds_size = sizeof(vendor_specific_init) / sizeof(sh8601_lcd_init_cmd_t),
            .flags ={
                .use_qspi_interface = 1,
            }
        };

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.flags.reset_active_high = 1,
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        panel_config.vendor_config = (void *)&vendor_config;
        ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(panel_io, &panel_config, &panel));

        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, false);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        esp_lcd_panel_disp_on_off(panel, true);
        display_ = new CustomLcdDisplay(panel_io, panel, DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("BoardControl"));
    }

public:
    waveshare_amoled_1_8() :
        boot_button_(BOOT_BUTTON_GPIO) {
        InitializeCodecI2c();
        InitializeTca9554();
        InitializeAxp2101();
        InitializeSpi();
        InitializeSH8601Display();
        InitializeButtons();
        InitializePowerSaveTimer();
        InitializeIot();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(codec_i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);

        return &audio_codec;

    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual bool GetBatteryLevel(int &level, bool& charging) override {
        static int last_level = 0;
        static bool last_charging = false;
        level = axp2101_->GetBatteryLevel();
        charging = axp2101_->IsCharging();
        if (level != last_level || charging != last_charging) {
            last_level = level;
            last_charging = charging;
            ESP_LOGI(TAG, "Battery level: %d, charging: %d", level, charging);
        }
        return true;
    }
};

DECLARE_BOARD(waveshare_amoled_1_8);
