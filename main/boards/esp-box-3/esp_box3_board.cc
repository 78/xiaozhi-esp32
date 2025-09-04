#include "wifi_board.h"
#include "codecs/box_audio_codec.h"
#include "display/lcd_display.h"
#include "esp_lcd_ili9341.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "emote_display.h"
#if CONFIG_USE_EMOTE_STYLE
#include "mmap_generate_emoji_large.h"
#endif

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <wifi_station.h>

#define TAG "EspBox3Board"

#if CONFIG_USE_EMOTE_STYLE
LV_FONT_DECLARE(font_puhui_basic_30_4);
#else
LV_FONT_DECLARE(font_awesome_20_4);
#endif
LV_FONT_DECLARE(font_puhui_20_4);

#if CONFIG_USE_EMOTE_STYLE
static const anim::EmoteDisplayConfig kEmoteConfig = {
    .emotion_map = {
        {"happy",       {MMAP_EMOJI_LARGE_HAPPY_EAF,    true,  20}},
        {"laughing",    {MMAP_EMOJI_LARGE_HAPPY_EAF,    true,  20}},
        {"funny",       {MMAP_EMOJI_LARGE_HAPPY_EAF,    true,  20}},
        {"loving",      {MMAP_EMOJI_LARGE_HAPPY_EAF,    true,  20}},
        {"embarrassed", {MMAP_EMOJI_LARGE_HAPPY_EAF,    true,  20}},
        {"confident",   {MMAP_EMOJI_LARGE_HAPPY_EAF,    true,  20}},
        {"delicious",   {MMAP_EMOJI_LARGE_HAPPY_EAF,    true,  20}},
        {"sad",         {MMAP_EMOJI_LARGE_SAD_EAF,      true,  20}},
        {"crying",      {MMAP_EMOJI_LARGE_CRY_EAF,      true,  20}},
        {"sleepy",      {MMAP_EMOJI_LARGE_SLEEP_EAF,    true,  20}},
        {"silly",       {MMAP_EMOJI_LARGE_HAPPY_EAF,    true,  20}},
        {"angry",       {MMAP_EMOJI_LARGE_ANGRY_EAF,    true,  20}},
        {"surprised",   {MMAP_EMOJI_LARGE_HAPPY_EAF,    true,  20}},
        {"shocked",     {MMAP_EMOJI_LARGE_SHOCKED_EAF,  true,  20}},
        {"thinking",    {MMAP_EMOJI_LARGE_CONFUSED_EAF, true,  20}},
        {"winking",     {MMAP_EMOJI_LARGE_NEUTRAL_EAF,  true,  20}},
        {"relaxed",     {MMAP_EMOJI_LARGE_HAPPY_EAF,    true,  20}},
        {"confused",    {MMAP_EMOJI_LARGE_CONFUSED_EAF, true,  20}},
        {"neutral",     {MMAP_EMOJI_LARGE_WINKING_EAF,  false, 20}},
        {"idle",        {MMAP_EMOJI_LARGE_NEUTRAL_EAF,  false, 20}},
        {"listen",      {MMAP_EMOJI_LARGE_LISTEN_EAF,   true,  20}}, // 添加监听动画
    },
    .icon_map = {
        {"wifi",     MMAP_EMOJI_LARGE_ICON_WIFI_BIN},
        {"battery",  MMAP_EMOJI_LARGE_ICON_BATTERY_BIN},
        {"mic",      MMAP_EMOJI_LARGE_ICON_MIC_BIN},
        {"speaker",  MMAP_EMOJI_LARGE_ICON_SPEAKER_ZZZ_BIN},
        {"error",    MMAP_EMOJI_LARGE_ICON_WIFI_FAILED_BIN},
    },
    .layout = {
        .eye_anim = {
            .align = GFX_ALIGN_LEFT_MID,
            .x = 10,
            .y = 30
        },
        .status_icon = {
            .align = GFX_ALIGN_TOP_MID,
            .x = -120,
            .y = 18
        },
        .toast_label = {
            .align = GFX_ALIGN_TOP_MID,
            .x = 0,
            .y = 20,
            .width = 200,
            .height = 40
        },
        .clock_label = {
            .align = GFX_ALIGN_TOP_MID,
            .x = 0,
            .y = 15,
            .width = 200,
            .height = 50
        },
        .listen_anim = {
            .align = GFX_ALIGN_TOP_MID,
            .x = 0,
            .y = 5
        }
    },
};
#endif

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

class EspBox3Board : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    Button boot_button_;
#if CONFIG_USE_EMOTE_STYLE
    anim::EmoteDisplay* display_ = nullptr;
    mmap_assets_handle_t assets_handle_ = nullptr;
#else
    LcdDisplay* display_;
#endif

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
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState();
        });

#if CONFIG_USE_DEVICE_AEC
        boot_button_.OnDoubleClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateIdle) {
                app.SetAecMode(app.GetAecMode() == kAecOff ? kAecOnDeviceSide : kAecOff);
            }
        });
#endif
    }

    void InitializeAssets() 
    {
#if CONFIG_USE_EMOTE_STYLE
        // Initialize assets for EmoteDisplay
        const mmap_assets_config_t assets_cfg = {
            .partition_label = "assets",
            .max_files = MMAP_EMOJI_LARGE_FILES,
            .checksum = MMAP_EMOJI_LARGE_CHECKSUM,
            .flags = {.mmap_enable = true, .full_check = true}
        };
        ESP_ERROR_CHECK(mmap_assets_new(&assets_cfg, &assets_handle_));
        ESP_LOGI(TAG, "Assets initialized successfully");
#endif
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
        esp_lcd_panel_invert_color(panel, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        esp_lcd_panel_disp_on_off(panel, true);
        
#if CONFIG_USE_EMOTE_STYLE
        display_ = new anim::SPIEmoteDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT, {
            .text_font = &font_puhui_20_4,
            .basic_font = &font_puhui_basic_30_4,
        }, assets_handle_, kEmoteConfig);
#else
        display_ = new SpiLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                    {
                                        .text_font = &font_puhui_20_4,
                                        .icon_font = &font_awesome_20_4,
#if CONFIG_USE_WECHAT_MESSAGE_STYLE
                                        .emoji_font = font_emoji_32_init(),
#else
                                        .emoji_font = font_emoji_64_init(),
#endif
                                    });
#endif
    }

public:
    EspBox3Board() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();
        InitializeSpi();
        InitializeAssets();
        InitializeIli9341Display();
        InitializeButtons();
        GetBacklight()->RestoreBrightness();
    }

    virtual ~EspBox3Board()
    {
#if CONFIG_USE_EMOTE_STYLE
        if (assets_handle_) {
            mmap_assets_del(assets_handle_);
            assets_handle_ = nullptr;
        }
#endif
    }

    virtual AudioCodec* GetAudioCodec() override {
        static BoxAudioCodec audio_codec(
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

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }
};

DECLARE_BOARD(EspBox3Board);
