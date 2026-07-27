#include "application.h"
#include "bsp_adc_calibration.h"
#include "button.h"
#include "codecs/es8389_audio_codec.h"
#include "config.h"
#include "display/lcd_display.h"
#include "esp_video.h"
#include "led/single_led.h"
#include "wifi_board.h"

#include <esp_adc/adc_oneshot.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_rgb.h>
#include <esp_lcd_touch_gt1151.h>
#include <esp_log.h>
#include <esp_lvgl_port.h>
#include <lvgl.h>

#define TAG "Esp32S31Korvo1"

// GPIO42 电阻分压网络上的 4 个 ADC 按键（上拉 10K 到 3.3V）。
typedef enum {
    BSP_ADC_BUTTON_SET,
    BSP_ADC_BUTTON_MODE,
    BSP_ADC_BUTTON_VOL_DOWN,
    BSP_ADC_BUTTON_VOL_UP,
    BSP_ADC_BUTTON_NUM
} bsp_adc_button_t;

// ADC 按键自定义驱动（官方 BSP 方案：S31 软件校准 + 相邻键中点窗口）。
// 中心电压在 config.h 中定义（官方数据，本板与官方一致：VOL+ 最低，SET 最高）。
typedef struct {
    button_driver_t base;
    uint16_t min_mv;
    uint16_t max_mv;
} korvo_adc_button_driver_t;

// 按键梯形表：必须按中心电压升序排列，窗口取相邻键中点
typedef struct {
    bsp_adc_button_t button;
    uint16_t center_mv;
} korvo_adc_button_ladder_t;

static adc_oneshot_unit_handle_t s_button_adc_handle = nullptr;
static korvo_adc_button_driver_t s_button_drivers[BSP_ADC_BUTTON_NUM];
static const korvo_adc_button_ladder_t s_button_ladder[BSP_ADC_BUTTON_NUM] = {
    { BSP_ADC_BUTTON_VOL_UP,   ADC_BUTTON_VOL_UP_MV   },  // 380mV
    { BSP_ADC_BUTTON_VOL_DOWN, ADC_BUTTON_VOL_DOWN_MV },  // 820mV
    { BSP_ADC_BUTTON_MODE,     ADC_BUTTON_MODE_MV     },  // 1340mV
    { BSP_ADC_BUTTON_SET,      ADC_BUTTON_SET_MV      },  // 1870mV
};

static uint16_t ButtonMvMidpoint(uint16_t a, uint16_t b) {
    return (uint16_t)(((uint32_t)a + (uint32_t)b) / 2U);
}

static uint8_t KorvoAdcButtonGetKeyLevel(button_driver_t* driver) {
    korvo_adc_button_driver_t* btn = (korvo_adc_button_driver_t*)driver;
    uint32_t raw_sum = 0;
    int raw = 0;
    for (int i = 0; i < 4; i++) {
        if (adc_oneshot_read(s_button_adc_handle, ADC_CHANNEL_0, &raw) != ESP_OK) {
            return BUTTON_INACTIVE;
        }
        raw_sum += raw;
    }
    int mv = 0;
    if (bsp_s31_adc_calibration_raw_to_mv(ADC_UNIT_1, (int)(raw_sum / 4), &mv) != ESP_OK) {
        return BUTTON_INACTIVE;
    }
    return (mv >= btn->min_mv && mv <= btn->max_mv) ? BUTTON_ACTIVE : BUTTON_INACTIVE;
}

class Esp32S31Korvo1 : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_ = nullptr;
    adc_oneshot_unit_handle_t adc_handle_ = nullptr;
    Button boot_button_;
    Button* adc_button_[BSP_ADC_BUTTON_NUM] = {};
    LcdDisplay* display_ = nullptr;
    EspVideo* camera_ = nullptr;

    void InitializeCodecI2c() {
        i2c_master_bus_config_t i2c_bus_config = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags =
                {
                    .enable_internal_pullup = 1,
                },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &codec_i2c_bus_));
    }

    void ChangeVol(int delta) {
        auto codec = GetAudioCodec();
        int volume = codec->output_volume() + delta;
        if (volume > 100) {
            volume = 100;
        }
        if (volume < 0) {
            volume = 0;
        }
        codec->SetOutputVolume(volume);
        ESP_LOGI(TAG, "Volume: %d", volume);
    }

    void InitializeButtons() {
        // BOOT 键：启动阶段进入配网，其他时候切换对话状态
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });

        // 4 个 ADC 按键在 GPIO42（ESP32-S31 的 ADC1 通道 0）。
        // 通道参数与官方 BSP 一致（0dB 衰减），随后执行 S31 SAR 软件校准。
        const adc_oneshot_unit_init_cfg_t adc_unit_config = {
            .unit_id = ADC_UNIT_1,
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_unit_config, &adc_handle_));
        const adc_oneshot_chan_cfg_t adc_chan_config = {
            .atten = ADC_ATTEN_DB_0,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle_, ADC_CHANNEL_0, &adc_chan_config));
        s_button_adc_handle = adc_handle_;
        ESP_ERROR_CHECK(bsp_s31_adc_calibration_init(adc_handle_, ADC_UNIT_1, ADC_CHANNEL_0));

        // 按键窗口：相邻键中心电压的中点（官方 BSP 算法，梯形表已按升序排列）
        const button_config_t button_config = {};
        for (int i = 0; i < BSP_ADC_BUTTON_NUM; i++) {
            const bsp_adc_button_t button = s_button_ladder[i].button;
            const uint16_t center = s_button_ladder[i].center_mv;
            if (i == 0) {
                uint16_t upper = ButtonMvMidpoint(s_button_ladder[0].center_mv, s_button_ladder[1].center_mv);
                uint16_t margin = upper - center;
                s_button_drivers[i].min_mv = center > margin ? center - margin : 0;
                s_button_drivers[i].max_mv = upper;
            } else if (i == BSP_ADC_BUTTON_NUM - 1) {
                s_button_drivers[i].min_mv = ButtonMvMidpoint(s_button_ladder[i - 1].center_mv, center);
                s_button_drivers[i].max_mv = ButtonMvMidpoint(center, ADC_BUTTON_IDLE_MV);
            } else {
                s_button_drivers[i].min_mv = ButtonMvMidpoint(s_button_ladder[i - 1].center_mv, center);
                s_button_drivers[i].max_mv = ButtonMvMidpoint(center, s_button_ladder[i + 1].center_mv);
            }
            s_button_drivers[i].base.get_key_level = KorvoAdcButtonGetKeyLevel;
            ESP_LOGI(TAG, "ADC button %d: center=%umV range=%u-%umV",
                     button, center, s_button_drivers[i].min_mv, s_button_drivers[i].max_mv);

            button_handle_t handle = nullptr;
            ESP_ERROR_CHECK(iot_button_create(&button_config, &s_button_drivers[i].base, &handle));
            adc_button_[button] = new Button(handle);
        }

        auto volume_up_button = adc_button_[BSP_ADC_BUTTON_VOL_UP];
        volume_up_button->OnClick([this]() { ChangeVol(10); });
        volume_up_button->OnLongPress([this]() { GetAudioCodec()->SetOutputVolume(100); });

        auto volume_down_button = adc_button_[BSP_ADC_BUTTON_VOL_DOWN];
        volume_down_button->OnClick([this]() { ChangeVol(-10); });
        volume_down_button->OnLongPress([this]() { GetAudioCodec()->SetOutputVolume(0); });

        auto set_button = adc_button_[BSP_ADC_BUTTON_SET];
        set_button->OnClick([this]() { EnterWifiConfigMode(); });

        auto mode_button = adc_button_[BSP_ADC_BUTTON_MODE];
        mode_button->OnClick([this]() { Application::GetInstance().ToggleChatState(); });
    }

    void InitializeRgbDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr; // RGB 面板不使用 panel IO
        esp_lcd_panel_handle_t panel_handle = nullptr;

        esp_lcd_rgb_panel_config_t rgb_config = {
            .clk_src = LCD_CLK_SRC_DEFAULT,
            .timings = {
                .pclk_hz = 16 * 1000 * 1000,
                .h_res = DISPLAY_WIDTH,
                .v_res = DISPLAY_HEIGHT,
                .hsync_pulse_width = 4,
                .hsync_back_porch = 8,
                .hsync_front_porch = 8,
                .vsync_pulse_width = 4,
                .vsync_back_porch = 8,
                .vsync_front_porch = 8,
                .flags = {
                    .pclk_active_neg = true,
                },
            },
            .data_width = 16,
            .in_color_format = LCD_COLOR_FMT_RGB565,
            .out_color_format = LCD_COLOR_FMT_RGB565,
            .num_fbs = 2,
            // bounce buffer 必须保持较小：太大会耗尽内部 SRAM，
            // 在启动负载尖峰时引发 LCD underrun（参考板实测 8 行最稳）。
            .bounce_buffer_size_px = DISPLAY_WIDTH * 8,
            .hsync_gpio_num = LCD_RGB_HSYNC,
            .vsync_gpio_num = LCD_RGB_VSYNC,
            .de_gpio_num = LCD_RGB_DE,
            .pclk_gpio_num = LCD_RGB_PCLK,
            .disp_gpio_num = LCD_RGB_DISP,
            .data_gpio_nums = {
                LCD_RGB_DATA0,  LCD_RGB_DATA1,  LCD_RGB_DATA2,  LCD_RGB_DATA3,
                LCD_RGB_DATA4,  LCD_RGB_DATA5,  LCD_RGB_DATA6,  LCD_RGB_DATA7,
                LCD_RGB_DATA8,  LCD_RGB_DATA9,  LCD_RGB_DATA10, LCD_RGB_DATA11,
                LCD_RGB_DATA12, LCD_RGB_DATA13, LCD_RGB_DATA14, LCD_RGB_DATA15,
            },
            .flags = {
                .fb_in_psram = 1,
            },
        };

        ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&rgb_config, &panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

        display_ = new RgbLcdDisplay(panel_io, panel_handle,
                                     DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                     DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
                                     DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeTouch() {
        esp_lcd_touch_handle_t tp;
        esp_lcd_touch_config_t tp_cfg = {
            .x_max = DISPLAY_WIDTH - 1,
            .y_max = DISPLAY_HEIGHT - 1,
            .rst_gpio_num = GPIO_NUM_NC,
            .int_gpio_num = GPIO_NUM_NC,
            .levels = {
                .reset = 0,
                .interrupt = 0,
            },
            .flags = {
                .swap_xy = 0,
                .mirror_x = 0,
                .mirror_y = 0,
            },
        };

        esp_lcd_panel_io_handle_t tp_io_handle = nullptr;
        esp_lcd_panel_io_i2c_config_t tp_io_config = {};
        tp_io_config.scl_speed_hz = 400 * 1000;
        tp_io_config.dev_addr = ESP_LCD_TOUCH_IO_I2C_GT1151_ADDRESS;
        tp_io_config.control_phase_bytes = 1;
        tp_io_config.dc_bit_offset = 0;
        tp_io_config.lcd_cmd_bits = 16;
        tp_io_config.flags.disable_control_phase = 1;

        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(codec_i2c_bus_, &tp_io_config, &tp_io_handle));
        ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt1151(tp_io_handle, &tp_cfg, &tp));

        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = lv_display_get_default(),
            .handle = tp,
        };
        lvgl_port_add_touch(&touch_cfg);
        ESP_LOGI(TAG, "GT1151 touch initialized");
    }

    void InitializeCamera() {
        static esp_cam_ctlr_dvp_pin_config_t dvp_pin_config = {
            .data_width = CAM_CTLR_DATA_WIDTH_8,
            .data_io = {
                [0] = CAMERA_PIN_D0,
                [1] = CAMERA_PIN_D1,
                [2] = CAMERA_PIN_D2,
                [3] = CAMERA_PIN_D3,
                [4] = CAMERA_PIN_D4,
                [5] = CAMERA_PIN_D5,
                [6] = CAMERA_PIN_D6,
                [7] = CAMERA_PIN_D7,
            },
            .vsync_io = CAMERA_PIN_VSYNC,
            .de_io = CAMERA_PIN_HREF,
            .pclk_io = CAMERA_PIN_PCLK,
            .xclk_io = CAMERA_PIN_XCLK,
        };

        // 参数以官方 factory demo 的 BSP 实测值为准：
        // SCCB 复用 codec 的 I2C 总线（10kHz），XCLK 20MHz。
        esp_video_init_sccb_config_t sccb_config = {
            .init_sccb = false,
            .i2c_handle = codec_i2c_bus_,
            .freq = CAMERA_SCCB_FREQ_HZ,
        };

        esp_video_init_dvp_config_t dvp_config = {
            .sccb_config = sccb_config,
            .reset_pin = CAMERA_PIN_RESET,
            .pwdn_pin = CAMERA_PIN_PWDN,
            .dvp_pin = dvp_pin_config,
            .xclk_freq = CAMERA_XCLK_FREQ_HZ,
        };

        esp_video_init_config_t video_config = {
            .dvp = &dvp_config,
        };

        camera_ = new EspVideo(video_config);

        // 方向是板级特定配置，见 config.h 的 CAMERA_VFLIP / CAMERA_HMIRROR
        camera_->SetVFlip(CAMERA_VFLIP);
        camera_->SetHMirror(CAMERA_HMIRROR);
    }

public:
    Esp32S31Korvo1() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeCodecI2c();
        InitializeButtons();
        InitializeRgbDisplay();
        InitializeTouch();
        InitializeCamera();
    }

    AudioCodec* GetAudioCodec() override {
        static Es8389AudioCodec audio_codec(
            codec_i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN, AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8389_ADDR,
            AUDIO_CODEC_USE_MCLK, AUDIO_INPUT_CHANNELS, AUDIO_OUTPUT_CHANNELS);
        return &audio_codec;
    }

    Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    Display* GetDisplay() override {
        return display_;
    }

    Camera* GetCamera() override {
        return camera_;
    }
};

DECLARE_BOARD(Esp32S31Korvo1);
