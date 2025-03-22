#include "wifi_board.h"
#include "k10_audio_codec.h"
#include "display/lcd_display.h"
#include "esp_lcd_ili9341.h"
#include "font_awesome_symbols.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "iot/thing_manager.h"
#include "led/circular_strip.h"
#include "assets/lang_config.h"
#include "settings.h"

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <wifi_station.h>

#include "esp_io_expander_tca95xx_16bit.h"
#include "aht20.h"

#define TAG "DF-K10"

LV_FONT_DECLARE(font_puhui_20_4);
LV_FONT_DECLARE(font_awesome_20_4);

class Df_K10Board : public WifiBoard {
 private:
    i2c_master_bus_handle_t i2c_bus_;
    esp_io_expander_handle_t io_expander;
    LcdDisplay *display_;
    button_handle_t btn_a;
    button_handle_t btn_b;
    AHT20* ath20_  = nullptr;

    void InitializeI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
                .i2c_port = (i2c_port_t)1,
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
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = GPIO_NUM_21;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = GPIO_NUM_12;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    esp_err_t IoExpanderSetLevel(uint16_t pin_mask, uint8_t level) {
        return esp_io_expander_set_level(io_expander, pin_mask, level);
    }

    uint8_t IoExpanderGetLevel(uint16_t pin_mask) {
        uint32_t pin_val = 0;
        esp_io_expander_get_level(io_expander, DRV_IO_EXP_INPUT_MASK, &pin_val);
        pin_mask &= DRV_IO_EXP_INPUT_MASK;
        return (uint8_t)((pin_val & pin_mask) ? 1 : 0);
    }

    void InitializeIoExpander() {
        esp_io_expander_new_i2c_tca95xx_16bit(
                i2c_bus_, ESP_IO_EXPANDER_I2C_TCA9555_ADDRESS_000, &io_expander);

        esp_err_t ret;
        ret = esp_io_expander_print_state(io_expander);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Print state failed: %s", esp_err_to_name(ret));
        }
        ret = esp_io_expander_set_dir(io_expander, IO_EXPANDER_PIN_NUM_0,
                                                                    IO_EXPANDER_OUTPUT);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Set direction failed: %s", esp_err_to_name(ret));
        }
        ret = esp_io_expander_set_level(io_expander, 0, 1);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Set level failed: %s", esp_err_to_name(ret));
        }
        ret = esp_io_expander_set_dir(
                io_expander, DRV_IO_EXP_INPUT_MASK,
                IO_EXPANDER_INPUT);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Set direction failed: %s", esp_err_to_name(ret));
        }
    }
    void InitializeButtons() {
        // Button A
        button_config_t btn_a_config = {
            .type = BUTTON_TYPE_CUSTOM,
            .long_press_time = 1000,
            .short_press_time = 50,
            .custom_button_config = {
                .active_level = 0,
                .button_custom_init = nullptr,
                .button_custom_get_key_value = [](void *param) -> uint8_t {
                    auto self = static_cast<Df_K10Board*>(param);
                    return self->IoExpanderGetLevel(IO_EXPANDER_PIN_NUM_2);
                },
                .button_custom_deinit = nullptr,
                .priv = this,
            },
        };
        btn_a = iot_button_create(&btn_a_config);
        iot_button_register_cb(btn_a, BUTTON_SINGLE_CLICK, [](void* button_handle, void* usr_data) {
            auto self = static_cast<Df_K10Board*>(usr_data);
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                self->ResetWifiConfiguration();
            }
            app.ToggleChatState();
        }, this);
        iot_button_register_cb(btn_a, BUTTON_LONG_PRESS_START, [](void* button_handle, void* usr_data) {
            auto self = static_cast<Df_K10Board*>(usr_data);
            auto codec = self->GetAudioCodec();
            auto volume = codec->output_volume() - 10;
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
            self->GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        }, this);

        // Button B
        button_config_t btn_b_config = {
            .type = BUTTON_TYPE_CUSTOM,
            .long_press_time = 1000,
            .short_press_time = 50,
            .custom_button_config = {
                .active_level = 0,
                .button_custom_init = nullptr,
                .button_custom_get_key_value = [](void *param) -> uint8_t {
                    auto self = static_cast<Df_K10Board*>(param);
                    return self->IoExpanderGetLevel(IO_EXPANDER_PIN_NUM_12);
                },
                .button_custom_deinit = nullptr,
                .priv = this,
            },
        };
        btn_b = iot_button_create(&btn_b_config);
        iot_button_register_cb(btn_b, BUTTON_SINGLE_CLICK, [](void* button_handle, void* usr_data) {
            auto self = static_cast<Df_K10Board*>(usr_data);
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                self->ResetWifiConfiguration();
            }
            app.ToggleChatState();
        }, this);
        iot_button_register_cb(btn_b, BUTTON_LONG_PRESS_START, [](void* button_handle, void* usr_data) {
            auto self = static_cast<Df_K10Board*>(usr_data);
            auto codec = self->GetAudioCodec();
            auto volume = codec->output_volume() + 10;
            if (volume > 100) {
                volume = 100;
            }
            codec->SetOutputVolume(volume);
            self->GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        }, this);
    }

    void InitializeIli9341Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = GPIO_NUM_14;
        io_config.dc_gpio_num = GPIO_NUM_13;
        io_config.spi_mode = 0;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片
        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.bits_per_pixel = 16;
        panel_config.color_space = ESP_LCD_COLOR_SPACE_BGR;

        ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io, &panel_config, &panel));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, false));
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

        display_ = new SpiLcdDisplay(panel_io, panel,
                                DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
                                DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                {
                                        .text_font = &font_puhui_20_4,
                                        .icon_font = &font_awesome_20_4,
                                        .emoji_font = font_emoji_64_init(),
                                });
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot() {
        auto &thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("Environment"));
    }

    void InitializeSensor() {
        ath20_ = new AHT20(i2c_bus_, 0x38);

        Settings settings("environment", true);
        if (settings.GetInt("set_diff", 0) == 0) {
            settings.SetInt("set_diff", 1);
            settings.SetInt("temp_diff", -80);    // 默认温度值偏移量 * 10
            settings.SetInt("humi_diff", 100);  // 默认是渎职偏移量 * 10

            ESP_LOGI(TAG, "Set default temperature_diff to -8");
            ESP_LOGI(TAG, "Set default humidity_diff to 10");
        }
    }

 public:
    Df_K10Board() {
        InitializeI2c();
        InitializeIoExpander();
        InitializeSpi();
        InitializeIli9341Display();
        InitializeButtons();
        InitializeSensor();
        InitializeIot();
    }

    virtual Led* GetLed() override {
        static CircularStrip led(BUILTIN_LED_GPIO, 3);
        return &led;
    }

    virtual AudioCodec *GetAudioCodec() override {
        static K10AudioCodec audio_codec(
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
                    AUDIO_CODEC_ES7210_ADDR,
                    AUDIO_INPUT_REFERENCE);
        return &audio_codec;
    }

    virtual Display *GetDisplay() override {
        return display_;
    }

    bool GetTemperature(float *temperature) {
        // 读取数据
        float temp, humi;
        if (ath20_->get_measurements(&temp, &humi)) {
            if (temperature) *temperature = temp;
            return true;
        } else {
            ESP_LOGE(TAG, "Failed to read sensor");
            return false;
        }
    }

    bool GetHumidity(float *humidity) {
        // 读取数据
        float temp, humi;
        if (ath20_->get_measurements(&temp, &humi)) {
            if (humidity) *humidity = humi;
            return true;
        } else {
            ESP_LOGE(TAG, "Failed to read sensor");
            return false;
        }
    }
};

DECLARE_BOARD(Df_K10Board);

namespace iot {

// 这里仅定义 Environment 的属性和方法，不包含具体的实现
class Environment : public Thing {
 private:
    float temperature_ = 0;
    float temperature_diff_ = -0.0;
    float humidity_ = 0;
    float humidity_diff_ = 0.0;

    void InitializeDiff() {
        Settings settings("environment", true);
        temperature_diff_ = settings.GetInt("temp_diff", 0) / 10;
        humidity_diff_ = settings.GetInt("humi_diff", 0) / 10;

        ESP_LOGI(TAG, "Get stored temperature_diff is %.1f", temperature_diff_);
        ESP_LOGI(TAG, "Get stored humidity_diff is %.1f", humidity_diff_);
    }

 public:
    Environment() : Thing("Environment", "当前环境信息") {
        // 获取diff初始值
        InitializeDiff();

        // 定义设备的属性
        properties_.AddNumberProperty("temperature", "当前环境温度", [this]() -> float {
            auto& board = static_cast<Df_K10Board&>(Board::GetInstance());
            if (board.GetTemperature(&temperature_)) {
                ESP_LOGI(TAG, "Original Temperature value is %.1f, "
                                "diff is %.1f, return is %.1f",
                                temperature_, temperature_diff_,
                                temperature_ + temperature_diff_);
                return temperature_ + temperature_diff_;
            }
            return 0;
        });

        properties_.AddNumberProperty("humidity", "当前环境湿度", [this]() -> float {
            auto& board = static_cast<Df_K10Board&>(Board::GetInstance());
            if (board.GetHumidity(&humidity_)) {
                if (humidity_ + humidity_diff_ >= 0) {
                    ESP_LOGI(TAG, "Original Humidity value is %.1f, "
                        "diff is %.1f, return is %.1f",
                        humidity_, humidity_diff_,
                        humidity_ + humidity_diff_);
                    return humidity_ + humidity_diff_;
                }
            }
            return 0;
        });

        properties_.AddNumberProperty("temperature_diff", "温度偏差", [this]() -> float {
            return temperature_diff_;
        });

        properties_.AddNumberProperty("humidity_diff", "湿度偏差", [this]() -> float {
            return humidity_diff_;
        });

        // 定义设备可以被远程执行的指令
        methods_.AddMethod("SetTemperatureDiff", "设置温度偏差", ParameterList({
            Parameter("temperature_diff", "-50到50之间的整数或者带有1位小数的数", kValueTypeNumber, true)
        }), [this](const ParameterList& parameters) {
            float tmp = parameters["temperature_diff"].number();
            if (tmp >= -50 && tmp <= 50) {
                temperature_diff_ = parameters["temperature_diff"].number();

                Settings settings("environment", true);
                settings.SetInt("temp_diff", static_cast<int>(temperature_diff_*10));
                ESP_LOGI(TAG, "Set Temperature diff to %.1f°C", temperature_diff_);
            } else {
                ESP_LOGE(TAG, "Temperature diff value %.1f°C is invalid", temperature_diff_);
            }
        });

        methods_.AddMethod("SetHumidityDiff", "设置湿度偏差", ParameterList({
            Parameter("humidity_diff", "-50到50之间的整数或者带有1位小数的数", kValueTypeNumber, true)
        }), [this](const ParameterList& parameters) {
            float tmp = parameters["humidity_diff"].number();
            if (tmp >= -50 && tmp <= 50) {
                humidity_diff_ = parameters["humidity_diff"].number();

                Settings settings("environment", true);
                settings.SetInt("humi_diff", static_cast<int>(humidity_diff_*10));
                ESP_LOGI(TAG, "Set Humidity diff to %.1f%%", humidity_diff_);
            } else {
                ESP_LOGE(TAG, "Humidity_diff value %.1f%% is invalid", humidity_diff_);
            }
        });
    }
};

}  // namespace iot

DECLARE_THING(Environment);
