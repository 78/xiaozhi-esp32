#include "wifi_board.h"
#include "audio_codecs/box_audio_codec.h"
#include "display/st7789_display.h"
#include "esp_lcd_ili9341.h"
#include "font_awesome_symbols.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "iot/thing_manager.h"

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>

#define TAG "EspBox3Board"

// Can move to display/st7789_display.h
LV_FONT_DECLARE(font_puhui_14_1);
LV_FONT_DECLARE(font_awesome_30_1);
LV_FONT_DECLARE(font_awesome_14_1);

// Init ili9341 by custom cmd
static const ili9341_lcd_init_cmd_t vendor_specific_init[] = {
    {0xC8, (uint8_t []){0xFF, 0x93, 0x42}, 3, 0},
    {0xC0, (uint8_t []){0x0E, 0x0E}, 2, 0},
    {0xC5, (uint8_t []){0xD0}, 1, 0},
    {0xC1, (uint8_t []){0x02}, 1, 0},
    {0xB4, (uint8_t []){0x02}, 1, 0},
    {0xE0, (uint8_t []){0x00, 0x03, 0x08, 0x06, 0x13, 0x09, 0x39, 0x39, 0x48, 0x02, 0x0a, 0x08, 0x17, 0x17, 0x0F}, 15, 0},
    {0xE1, (uint8_t []){0x00, 0x28, 0x29, 0x01, 0x0d, 0x03, 0x3f, 0x33, 0x52, 0x04, 0x0f, 0x0e, 0x37, 0x38, 0x0F}, 15, 0},

    {0xB1, (uint8_t []){00, 0x1B}, 2, 0},
    {0x36, (uint8_t []){0x08}, 1, 0},
    {0x3A, (uint8_t []){0x55}, 1, 0},
    {0xB7, (uint8_t []){0x06}, 1, 0},

    {0x11, (uint8_t []){0}, 0x80, 0},
    {0x29, (uint8_t []){0}, 0x80, 0},

    {0, (uint8_t []){0}, 0xff, 0},
};

// Example Display and UI overwrite in different board
class Ili9341Display : public St7789Display {
private:    
    lv_obj_t *user_messge_label_ = nullptr;
    lv_obj_t *ai_messge_label_ = nullptr;
public:
    Ili9341Display(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                         gpio_num_t backlight_pin, bool backlight_output_invert,
                         int width, int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y, bool swap_xy)
        : St7789Display(panel_io, panel, backlight_pin, backlight_output_invert, width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy) {}

    void SetStatus(const std::string &status) override
    {
        if (status_label_ == nullptr) {
            return;
        }
        if(status=="待命")
        {
            SetChatMessage(" "," ");
        }
        DisplayLockGuard lock(this);
        lv_label_set_text(status_label_, status.c_str());
    }

    void SetChatMessage(const std::string &role, const std::string &content) override {
        if (ai_messge_label_== nullptr || user_messge_label_== nullptr) {
            return;
        }
        DisplayLockGuard lock(this);
        ESP_LOGI(TAG,"role:%s",role.c_str());
        if(role=="assistant")
        {
            std::string new_content = "AI:" + content;
            lv_label_set_text(ai_messge_label_, new_content.c_str());
        }
        else if(role=="user")
        {
            std::string new_content = "User:" + content;
            lv_label_set_text(user_messge_label_, new_content.c_str());
        }
        else{
            std::string new_content = "AI:";
            lv_label_set_text(ai_messge_label_, new_content.c_str());
            new_content="User:";
            lv_label_set_text(user_messge_label_, new_content.c_str());
        }
    }
    void SetupUI() override {
        DisplayLockGuard lock(this);

        lv_obj_del(chat_message_label_);

        lv_obj_set_flex_align(content_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_EVENLY); // 子对象居中对齐，等距分布

        lv_obj_set_style_text_font(emotion_label_, &font_awesome_30_1, 0);
        lv_label_set_text(emotion_label_, FONT_AWESOME_AI_CHIP);
        lv_obj_align(emotion_label_,LV_ALIGN_TOP_MID, 0, -10); // 左侧居中，向右偏移10个单位

        static lv_style_t style_msg;
        lv_style_init(&style_msg);
        lv_style_set_width(&style_msg, LV_HOR_RES-25);

        user_messge_label_ = lv_label_create(content_);
        lv_obj_set_style_text_font(user_messge_label_, &font_puhui_14_1, 0);
        lv_label_set_text(user_messge_label_, "User:");
        lv_obj_add_style(user_messge_label_, &style_msg, 0);
        lv_obj_align(user_messge_label_,LV_ALIGN_TOP_LEFT, 2, 25);

        ai_messge_label_ = lv_label_create(content_);
        lv_obj_set_style_text_font(ai_messge_label_, &font_puhui_14_1, 0);
        lv_label_set_text(ai_messge_label_, "AI:");
        lv_obj_add_style(ai_messge_label_, &style_msg, 0);
        lv_obj_align(ai_messge_label_,LV_ALIGN_TOP_LEFT, 2, 77);
    }
};

class EspBox3Board : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    Button boot_button_;
    Ili9341Display* display_;

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
        buscfg.mosi_io_num = GPIO_NUM_6;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = GPIO_NUM_7;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            Application::GetInstance().ToggleChatState();
        });
    }

    void InitializeIli9341Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = GPIO_NUM_5;
        io_config.dc_gpio_num = GPIO_NUM_4;
        io_config.spi_mode = 0;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片
        ESP_LOGD(TAG, "Install LCD driver");
        const ili9341_vendor_config_t vendor_config = {
            .init_cmds = &vendor_specific_init[0],
            .init_cmds_size = sizeof(vendor_specific_init) / sizeof(ili9341_lcd_init_cmd_t),
        };

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_48;
        panel_config.flags.reset_active_high = 1,
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        panel_config.vendor_config = (void *)&vendor_config;
        ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io, &panel_config, &panel));
        
        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, true);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        esp_lcd_panel_disp_on_off(panel, true);
        display_ = new Ili9341Display(panel_io, panel, DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
        display_->SetupUI();
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
    }

public:
    EspBox3Board() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();
        InitializeSpi();
        InitializeIli9341Display();
        InitializeButtons();
        InitializeIot();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static BoxAudioCodec* audio_codec = nullptr;
        if (audio_codec == nullptr) {
            audio_codec = new BoxAudioCodec(i2c_bus_, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
                AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR, AUDIO_CODEC_ES7210_ADDR, AUDIO_INPUT_REFERENCE);
            audio_codec->SetOutputVolume(AUDIO_DEFAULT_OUTPUT_VOLUME);
        }
        return audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual bool GetBatteryLevel(int &level, bool& charging) override
    {
        charging = false;
        level = 60;
        return true;
    };
};

DECLARE_BOARD(EspBox3Board);
