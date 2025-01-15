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
#include <wifi_station.h>

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <driver/spi_master.h>

#define TAG "waveshare_amoled_1_8"

LV_FONT_DECLARE(font_puhui_14_1);
LV_FONT_DECLARE(font_awesome_30_1);
LV_FONT_DECLARE(font_awesome_14_1);

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

class SH8601Display : public LcdDisplay {
private:
    lv_obj_t *user_messge_label_ = nullptr;
    lv_obj_t *ai_messge_label_ = nullptr;
public:
    SH8601Display(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                         gpio_num_t backlight_pin, bool backlight_output_invert,
                         int width, int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y, bool swap_xy)
        : LcdDisplay(panel_io, panel, backlight_pin, backlight_output_invert, width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy) {}

    void SetChatMessage(const std::string &role, const std::string &content) override {
        if (ai_messge_label_== nullptr || user_messge_label_== nullptr) {
            return;
        }

        DisplayLockGuard lock(this);
        ESP_LOGI(TAG, "role:%s", role.c_str());
        if(role == "assistant") {
            std::string new_content = "AI: " + content;
            lv_label_set_text(ai_messge_label_, new_content.c_str());
        } else if(role == "user") {
            std::string new_content = "User: " + content;
            lv_label_set_text(user_messge_label_, new_content.c_str());
        } else{
            lv_label_set_text(ai_messge_label_, "AI: ");
            lv_label_set_text(user_messge_label_, "User: ");
        }
    }

    void SetupUI() override {
        DisplayLockGuard lock(this);

        lv_obj_del(chat_message_label_);

        lv_obj_set_flex_align(content_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_EVENLY); // 子对象居中对齐，等距分布

        lv_obj_set_style_text_font(emotion_label_, &font_awesome_30_1, 0);
        lv_label_set_text(emotion_label_, FONT_AWESOME_AI_CHIP);
        lv_obj_align(emotion_label_, LV_ALIGN_TOP_MID, 0, -10); // 左侧居中，向右偏移10个单位

        static lv_style_t style_msg;
        lv_style_init(&style_msg);
        lv_style_set_width(&style_msg, LV_HOR_RES - 25);

        user_messge_label_ = lv_label_create(content_);
        lv_obj_set_style_text_font(user_messge_label_, &font_puhui_14_1, 0);
        lv_label_set_text(user_messge_label_, "User: ");
        lv_obj_add_style(user_messge_label_, &style_msg, 0);
        lv_obj_align(user_messge_label_, LV_ALIGN_TOP_LEFT, 2, 25);

        ai_messge_label_ = lv_label_create(content_);
        lv_obj_set_style_text_font(ai_messge_label_, &font_puhui_14_1, 0);
        lv_label_set_text(ai_messge_label_, "AI: ");
        lv_obj_add_style(ai_messge_label_, &style_msg, 0);
        lv_obj_align(ai_messge_label_, LV_ALIGN_TOP_LEFT, 2, 77);
    }
};



class waveshare_amoled_1_8 : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;
    Button boot_button_;
    SH8601Display* display_;

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
        boot_button_.OnPressDown([this]() {
            Application::GetInstance().StartListening();
        });
        boot_button_.OnPressUp([this]() {
            Application::GetInstance().StopListening();
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
        esp_lcd_panel_invert_color(panel, true);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        esp_lcd_panel_disp_on_off(panel, true);
        display_ = new SH8601Display(panel_io, panel, DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
        if (display_) {
            display_->SetupUI();
        } else {
            ESP_LOGE(TAG, "Display is not initialized!");
        }

    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
    }

public:
    waveshare_amoled_1_8() :
        boot_button_(BOOT_BUTTON_GPIO) {
        InitializeCodecI2c();
        InitializeSpi();
        InitializeSH8601Display();
        InitializeButtons();
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
};

DECLARE_BOARD(waveshare_amoled_1_8);
