#include "wifi_board.h"
#include "display/lcd_display.h"
#include "codecs/es8311_audio_codec.h"
#include "application.h"
#include "button.h"
#include "mcp_server.h"
#include "config.h"
#include "power_save_timer.h"
#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include "i2c_bus.h"
#include <driver/spi_master.h>
#include <esp_lcd_touch_cst816s.h>
#include "esp_io_expander_tca9554.h"
#include "bq27220.h"
#include <esp_lvgl_port.h>
#include <lvgl.h>

#define TAG "WaveshareEsp32c5TouchLCD1inch69"

class WaveshareEsp32c5TouchLCD1inch69 : public WifiBoard {
private:
    i2c_bus_handle_t i2c_bus_;
    i2c_master_bus_handle_t i2c_handle_;
    bq27220_handle_t bq27220 = NULL;
    esp_io_expander_handle_t expander = NULL;
    Button boot_button_;
    LcdDisplay *display_;
    PowerSaveTimer* power_save_timer_;

    void InitializePowerSaveTimer() {
        power_save_timer_ = new PowerSaveTimer(-1, 60, 300);
        power_save_timer_->OnEnterSleepMode([this]() {
            GetDisplay()->SetPowerSaveMode(true);
            GetBacklight()->SetBrightness(20);
        });
        power_save_timer_->OnExitSleepMode([this]() {
            GetDisplay()->SetPowerSaveMode(false);
            GetBacklight()->RestoreBrightness();
        });
        power_save_timer_->SetEnabled(true);
    }

    void InitializeCodecI2c() {
        // Initialize I2C peripheral
        i2c_config_t conf = {};
        conf.mode = I2C_MODE_MASTER;
        conf.sda_io_num = AUDIO_CODEC_I2C_SDA_PIN;
        conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
        conf.scl_io_num = AUDIO_CODEC_I2C_SCL_PIN;
        conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
        conf.master.clk_speed = 400 * 1000;
        i2c_bus_ = i2c_bus_create(I2C_NUM_0, &conf);

        i2c_handle_ = i2c_bus_get_internal_bus_handle(i2c_bus_);
        
        esp_io_expander_new_i2c_tca9554(i2c_handle_, ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000, &expander);
        /* Setup power amplifier pin, set default to enable */
        esp_io_expander_set_dir(expander, DISPLAY_TOUCH_RST_PIN | AUDIO_CODEC_PA_PIN, IO_EXPANDER_OUTPUT);
        esp_io_expander_set_level(expander, AUDIO_CODEC_PA_PIN | DISPLAY_TOUCH_RST_PIN, true);

    }

    void InitializeBq27220(uint16_t mah = 600) {
        static parameter_cedv_t default_cedv = {
            .full_charge_cap = 650,
            .design_cap = 650,
            .reserve_cap = 0,
            .near_full = 100,
            .self_discharge_rate = 10,
            .EDV0 = 3200,
            .EDV1 = 3300,
            .EDV2 = 3400,
            .EMF = 3670,
            .C0 = 115,
            .R0 = 968,
            .T0 = 4547,
            .R1 = 4764,
            .TC = 11,
            .C1 = 0,
            .DOD0   = 4200,
            .DOD10  = 4100,
            .DOD20  = 4000,
            .DOD30  = 3920,
            .DOD40  = 3850,
            .DOD50  = 3800,
            .DOD60  = 3750,
            .DOD70  = 3700,
            .DOD80  = 3600,
            .DOD90  = 3450,
            .DOD100 = 3200,
        };
        default_cedv.full_charge_cap = mah;
        default_cedv.design_cap = mah;
        static const gauging_config_t default_config = {
            .CCT = 1,
            .CSYNC = 0,
            .EDV_CMP = 0,
            .SC = 1,
            .FIXED_EDV0 = 0,
            .FCC_LIM = 1,
            .FC_FOR_VDQ = 1,
            .IGNORE_SD = 1,
            .SME0 = 0,
        };

        bq27220_config_t bq27220_cfg = {
            .i2c_bus = i2c_bus_,
            .cfg = &default_config,
            .cedv = &default_cedv,
        };

        bq27220 = bq27220_create(&bq27220_cfg);
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_CLK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(DISPLAY_SPI_MODE, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
    }

    void InitializeDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = 0;
        io_config.pclk_hz = 24 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(DISPLAY_SPI_MODE, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片
        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, true);
        // esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        esp_lcd_panel_disp_on_off(panel, true);
        display_ = new SpiLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }
    
    void InitializeTouch() {
        esp_lcd_touch_handle_t tp;
        esp_lcd_touch_config_t tp_cfg = {
            .x_max = DISPLAY_WIDTH - 1,
            .y_max = DISPLAY_HEIGHT - 1,
            .rst_gpio_num = GPIO_NUM_NC,
            .int_gpio_num = DISPLAY_TOUCH_INT_PIN,
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
        esp_lcd_panel_io_handle_t tp_io_handle = NULL;
        esp_lcd_panel_io_i2c_config_t tp_io_config = {};
        tp_io_config.dev_addr = ESP_LCD_TOUCH_IO_I2C_CST816S_ADDRESS;
        tp_io_config.on_color_trans_done = nullptr;
        tp_io_config.user_ctx = nullptr;
        tp_io_config.control_phase_bytes = 1;
        tp_io_config.dc_bit_offset = 0;
        tp_io_config.lcd_cmd_bits = 8;
        tp_io_config.lcd_param_bits = 0;
        tp_io_config.flags.dc_low_on_data = 0;
        tp_io_config.flags.disable_control_phase = 1;
        tp_io_config.scl_speed_hz = 400 * 1000;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_handle_, &tp_io_config, &tp_io_handle));
        ESP_LOGI(TAG, "Initialize touch controller");

        esp_io_expander_set_level(expander, DISPLAY_TOUCH_RST_PIN, false);
        vTaskDelay(pdMS_TO_TICKS(100));
        esp_io_expander_set_level(expander, DISPLAY_TOUCH_RST_PIN, true);
        vTaskDelay(pdMS_TO_TICKS(100));
        
        ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst816s(tp_io_handle, &tp_cfg, &tp));
        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = lv_display_get_default(),
            .handle = tp,
        };
        lvgl_port_add_touch(&touch_cfg);
        ESP_LOGI(TAG, "Touch panel initialized successfully");
    }

    // 初始化工具
    void InitializeTools() {
        auto &mcp_server = McpServer::GetInstance();
        mcp_server.AddTool("self.system.reconfigure_wifi",
            "Reboot the device and enter WiFi configuration mode.\n"
            "**CAUTION** You must ask the user to confirm this action.",
            PropertyList(), [this](const PropertyList& properties) {
                EnterWifiConfigMode();
                return true;
            });
    }

public:
    WaveshareEsp32c5TouchLCD1inch69() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializePowerSaveTimer();
        InitializeCodecI2c();
        InitializeBq27220(1000);
        InitializeSpi();
        InitializeDisplay();
        InitializeTouch();
        InitializeButtons();
        InitializeTools();
        GetBacklight()->RestoreBrightness();
    }

    virtual AudioCodec *GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(i2c_handle_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
                                            GPIO_NUM_NC, AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }

    virtual Display *GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    virtual bool GetBatteryLevel(int &level, bool& charging, bool& discharging) override {
        static bool last_discharging = false;
        int16_t max_current = bq27220_get_current(bq27220);
        charging = max_current > 0;
        discharging = max_current < 0;
        if (discharging != last_discharging) {
            power_save_timer_->SetEnabled(discharging);
            last_discharging = discharging;
        }
        level = bq27220_get_state_of_charge(bq27220);
        return true;
    }

    virtual void SetPowerSaveLevel(PowerSaveLevel level) override {
        if (level != PowerSaveLevel::LOW_POWER) {
            power_save_timer_->WakeUp();
        }
        WifiBoard::SetPowerSaveLevel(level);
    }

};

DECLARE_BOARD(WaveshareEsp32c5TouchLCD1inch69);
