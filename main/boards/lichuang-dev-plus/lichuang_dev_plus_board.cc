#include "dual_network_board.h" 
#include "audio_codecs/box_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "i2c_device.h"
#include "axp2101.h"
#include "power_save_timer.h"
#include "esp32_camera.h"
#include <esp_lcd_touch_ft5x06.h>
#include <esp_lvgl_port.h> 
#include <lvgl.h> 

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <esp_timer.h>
#include <wifi_station.h> 

#define TAG "LichuangDevPlusBoard"

LV_FONT_DECLARE(font_puhui_20_4);
LV_FONT_DECLARE(font_awesome_20_4);

// ======================================================================
//                         PMIC (电源管理)
// ======================================================================
class Pmic : public Axp2101 {
public:
    Pmic(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : Axp2101(i2c_bus, addr) {
        // --- 核心电源和充电配置 ---
        WriteReg(0x22, 0b110); WriteReg(0x27, 0x10);
        WriteReg(0x64, 0x03); WriteReg(0x61, 0x05); WriteReg(0x62, 0x08); WriteReg(0x63, 0x15);
        WriteReg(0x14, 0x00); WriteReg(0x15, 0x00); WriteReg(0x16, 0x05);
        WriteReg(0x24, 0x01); WriteReg(0x50, 0x14);

        // --- LDO 电压配置 ---
        WriteReg(0x92, 0x1C); // ALDO1 -> 3.3V
        WriteReg(0x93, 0x17); // ALDO2 -> 2.8V
        WriteReg(0x96, 0x0A); // BLDO1 -> 1.5V

        // --- LDO 开关统一控制 ---
        uint8_t ldo_onoff_ctrl0 = ReadReg(0x90); 
        ldo_onoff_ctrl0 |= (1 << 0) | (1 << 1) | (1 << 4);
        WriteReg(0x90, ldo_onoff_ctrl0);
        ESP_LOGI("Pmic", "AXP2101 LDOs configured: ALDO1=3.3V, ALDO2=2.8V, BLDO1=1.5V. All enabled.");
    }
};

// ======================================================================
//                      AW9523B (IO扩展 & LED驱动)
// ======================================================================
class Aw9523b : public I2cDevice {
public:
    Aw9523b(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {}

    void Init() {
        WriteReg(0x7F, 0x00); vTaskDelay(pdMS_TO_TICKS(10));
        uint16_t led_mask = (1 << AW9523B_PIN_RGB_R) | (1 << AW9523B_PIN_RGB_G) | (1 << AW9523B_PIN_RGB_B) | (1 << AW9523B_PIN_BACKLIGHT);
        WriteReg(0x12, (uint8_t)(led_mask & 0xFF)); WriteReg(0x13, (uint8_t)(led_mask >> 8));
        uint16_t dir_mask = (1 << AW9523B_PIN_RTC_INT) | (1 << AW9523B_PIN_PJ_SET);
        WriteReg(0x04, (uint8_t)(dir_mask & 0xFF)); WriteReg(0x05, (uint8_t)(dir_mask >> 8));
        WriteReg(0x06, (uint8_t)(dir_mask & 0xFF)); WriteReg(0x07, (uint8_t)(dir_mask >> 8));
        WriteReg(0x11, 0x00);
    }

    void SetGpio(uint8_t pin, bool level) {
        uint8_t reg = (pin < 8) ? 0x02 : 0x03; uint8_t bit = (pin < 8) ? pin : pin - 8;
        uint8_t data = ReadReg(reg);
        if (level) { data |= (1 << bit); } else { data &= ~(1 << bit); }
        WriteReg(reg, data);
    }
    
    void SetPwm(uint8_t pin, uint8_t brightness) {
        if (pin > 15) {
            return;
        }
        WriteReg(0x20 + pin, brightness);
    }

    bool GetInput(uint8_t pin) {
        if (pin > 15) {
            return false;
        }
        uint8_t reg = (pin < 8) ? 0x00 : 0x01; uint8_t bit = (pin < 8) ? pin : pin - 8;
        return (ReadReg(reg) & (1 << bit)) != 0;
    }

    uint16_t ReadAndClearInterrupts() {
        uint8_t p0 = ReadReg(0x08); uint8_t p1 = ReadReg(0x09); return (p1 << 8) | p0;
    }
};

// ======================================================================
//                       背光控制 (使用AW9523B)
// ======================================================================
#include "backlight.h"
class Aw9523bBacklight : public Backlight {
private:
    Aw9523b* expander_; 
    uint8_t pin_;
public:
    Aw9523bBacklight(Aw9523b* expander, uint8_t pin) : expander_(expander), pin_(pin) {}
    
    void SetBrightnessImpl(uint8_t brightness) override {
        if (expander_) {
            uint8_t pwm_value = (uint8_t)((float)brightness * 2.55f);
            expander_->SetPwm(pin_, pwm_value);
        }
    }
};

// ======================================================================
//                         音频 CODEC
// ======================================================================
class LichuangDevPlusAudioCodec : public BoxAudioCodec {
private:
    Aw9523b* expander_; 
    bool speaker_enabled_ = false;
public:
    LichuangDevPlusAudioCodec(i2c_master_bus_handle_t i2c_bus, Aw9523b* expander) 
        : BoxAudioCodec(i2c_bus, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                       AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, 
                       AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN, GPIO_NUM_NC, 
                       AUDIO_CODEC_ES8311_ADDR, AUDIO_CODEC_ES7210_ADDR, AUDIO_INPUT_REFERENCE),
          expander_(expander) {}

    virtual void EnableOutput(bool enable) override {
        BoxAudioCodec::EnableOutput(enable);
        if (expander_) { 
            expander_->SetGpio(AW9523B_PIN_PA_EN, enable); 
            speaker_enabled_ = enable; 
        }
    }
    bool IsSpeakerEnabled() const { return speaker_enabled_; }
};

// ======================================================================
//                          主板类定义
// ======================================================================

// ISR必须是一个简单的C风格函数，不能是类的成员，以避免链接问题
static void IRAM_ATTR gpio_isr_handler(void* arg) {
    QueueHandle_t queue = (QueueHandle_t)arg;
    uint32_t pin = AW9523B_INTERRUPT_PIN;
    xQueueSendFromISR(queue, &pin, NULL);
}

class LichuangDevPlusBoard : public DualNetworkBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    Button boot_button_;
    Aw9523b* aw9523b_ = nullptr;
    Pmic* pmic_ = nullptr;
    LcdDisplay* display_ = nullptr;
    PowerSaveTimer* power_save_timer_ = nullptr;
    Esp32Camera* camera_ = nullptr;
    QueueHandle_t gpio_evt_queue_;

    static void io_expander_interrupt_task(void* arg) {
        LichuangDevPlusBoard* board = reinterpret_cast<LichuangDevPlusBoard*>(arg);
        uint32_t io_num;
        for (;;) {
            if (xQueueReceive(board->gpio_evt_queue_, &io_num, portMAX_DELAY)) {
                if (board->aw9523b_) {
                    uint16_t int_status = board->aw9523b_->ReadAndClearInterrupts();
                    if (int_status & (1 << AW9523B_PIN_PJ_SET)) {
                        board->check_headphone_status();
                    }
                    if (int_status & (1 << AW9523B_PIN_RTC_INT)) {
                        ESP_LOGI(TAG, "RTC Interrupt Triggered (Handler not implemented yet).");
                    }
                }
            }
        }
    }

    void check_headphone_status() {
        if (!aw9523b_ || !GetAudioCodec()) return;
        bool headphone_inserted = !aw9523b_->GetInput(AW9523B_PIN_PJ_SET);
        auto* codec = static_cast<LichuangDevPlusAudioCodec*>(GetAudioCodec());
        if (headphone_inserted && codec->IsSpeakerEnabled()) {
            codec->EnableOutput(false);
            ESP_LOGI(TAG, "Headphone inserted, onboard speaker OFF");
        } else if (!headphone_inserted && !codec->IsSpeakerEnabled()) {
            codec->EnableOutput(true);
            ESP_LOGI(TAG, "Headphone removed, onboard speaker ON");
        }
    }
    
    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)I2C_MASTER_PORT,
            .sda_io_num = I2C_MASTER_SDA_PIN,
            .scl_io_num = I2C_MASTER_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7, .intr_priority = 0, .trans_queue_depth = 0,
            .flags = { .enable_internal_pullup = true },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
        aw9523b_ = new Aw9523b(i2c_bus_, AW9523B_I2C_ADDR);
        aw9523b_->Init();
    }
    
    void InitializeInterrupts() {
        gpio_evt_queue_ = xQueueCreate(10, sizeof(uint32_t));
        xTaskCreate(io_expander_interrupt_task, "io_exp_int_task", 2048, this, 10, NULL);
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_NEGEDGE;
        io_conf.pin_bit_mask = (1ULL << AW9523B_INTERRUPT_PIN);
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        gpio_config(&io_conf);
        gpio_install_isr_service(0);
        gpio_isr_handler_add((gpio_num_t)AW9523B_INTERRUPT_PIN, gpio_isr_handler, (void*) gpio_evt_queue_);
    }

    void InitializePmic() { pmic_ = new Pmic(i2c_bus_, AXP2101_I2C_ADDR); }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_SPI_MOSI;
        buscfg.sclk_io_num = DISPLAY_SPI_SCLK;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(DISPLAY_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeSt7789Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = GPIO_NUM_NC;
        io_config.dc_gpio_num = DISPLAY_SPI_DC;
        io_config.spi_mode = 2; io_config.pclk_hz = 80 * 1000 * 1000;
        io_config.trans_queue_depth = 10; io_config.lcd_cmd_bits = 8; io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((spi_host_device_t)DISPLAY_SPI_HOST, &io_config, &panel_io));

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC; panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB; panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
        
        esp_lcd_panel_reset(panel);
        if (aw9523b_) {
            aw9523b_->SetGpio(AW9523B_PIN_LCD_CS, false);
        }

        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, true);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        display_ = new SpiLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                    {
                                        .text_font = &font_puhui_20_4,
                                        .icon_font = &font_awesome_20_4,
                                        .emoji_font = font_emoji_64_init(),
                                    });
    }

    void InitializeTouch() {
        esp_lcd_touch_handle_t tp;
        esp_lcd_touch_config_t tp_cfg = {
            .x_max = DISPLAY_WIDTH,
            .y_max = DISPLAY_HEIGHT,
            .rst_gpio_num = GPIO_NUM_NC,
            .int_gpio_num = GPIO_NUM_NC, 
            .levels = { .reset = 0, .interrupt = 0, },
            .flags = { .swap_xy = DISPLAY_SWAP_XY, .mirror_x = DISPLAY_MIRROR_X, .mirror_y = DISPLAY_MIRROR_Y, },
        };
        esp_lcd_panel_io_handle_t tp_io_handle = NULL;
        esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
        tp_io_config.scl_speed_hz = 400000;
        esp_lcd_new_panel_io_i2c(i2c_bus_, &tp_io_config, &tp_io_handle);
        esp_lcd_touch_new_i2c_ft5x06(tp_io_handle, &tp_cfg, &tp);
        assert(tp);
        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = lv_display_get_default(), 
            .handle = tp,
        };
        lvgl_port_add_touch(&touch_cfg);
    }

    void InitializePowerSaveTimer() {
        power_save_timer_ = new PowerSaveTimer(240, 60, 300);
        power_save_timer_->OnEnterSleepMode([this]() {
            ESP_LOGI(TAG, "Entering sleep mode via PowerSaveTimer");
            if (GetDisplay()) { GetDisplay()->SetChatMessage("system", ""); GetDisplay()->SetEmotion("sleepy"); }
            if (GetAudioCodec()) GetAudioCodec()->EnableInput(false);
            if (GetBacklight()) GetBacklight()->SetBrightness(10);
        });
        power_save_timer_->OnExitSleepMode([this]() {
            ESP_LOGI(TAG, "Exiting sleep mode via PowerSaveTimer");
            if (GetAudioCodec()) GetAudioCodec()->EnableInput(true);
            if (GetDisplay()) { GetDisplay()->SetChatMessage("system", ""); GetDisplay()->SetEmotion("neutral"); }
            if (GetBacklight()) GetBacklight()->RestoreBrightness();
        });
        power_save_timer_->OnShutdownRequest([this]() {
            ESP_LOGI(TAG, "Shutdown requested via PowerSaveTimer");
            if (pmic_) pmic_->PowerOff();
        });
        power_save_timer_->SetEnabled(true);
    }
    
    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            power_save_timer_->WakeUp();
            auto& app = Application::GetInstance();
            if (GetNetworkType() == NetworkType::WIFI) {
                if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                    auto& wifi_board = static_cast<WifiBoard&>(GetCurrentBoard());
                    wifi_board.ResetWifiConfiguration();
                }
            }
            app.ToggleChatState();
        });

        boot_button_.OnDoubleClick([this]() {
            auto& app = Application::GetInstance();
#if CONFIG_USE_DEVICE_AEC
            if (app.GetDeviceState() == kDeviceStateIdle) {
                app.SetAecMode(app.GetAecMode() == kAecOff ? kAecOnDeviceSide : kAecOff);
                return;
            }
#endif
            if (app.GetDeviceState() == kDeviceStateStarting || app.GetDeviceState() == kDeviceStateWifiConfiguring) {
                SwitchNetworkType();
            }
        });
    }

    void InitializeCamera() {
        if (aw9523b_) {
            aw9523b_->SetGpio(AW9523B_PIN_DVP_PWDN, false);
        }
        camera_config_t config = {};
        config.ledc_channel = LEDC_CHANNEL_2; config.ledc_timer = LEDC_TIMER_2;
        config.pin_d0 = CAMERA_PIN_D0; config.pin_d1 = CAMERA_PIN_D1; config.pin_d2 = CAMERA_PIN_D2; config.pin_d3 = CAMERA_PIN_D3;
        config.pin_d4 = CAMERA_PIN_D4; config.pin_d5 = CAMERA_PIN_D5; config.pin_d6 = CAMERA_PIN_D6; config.pin_d7 = CAMERA_PIN_D7;
        config.pin_xclk = CAMERA_PIN_XCLK; config.pin_pclk = CAMERA_PIN_PCLK;
        config.pin_vsync = CAMERA_PIN_VSYNC; config.pin_href = CAMERA_PIN_HREF;
        config.pin_sccb_sda = -1; config.pin_sccb_scl = -1;
        config.sccb_i2c_port = I2C_MASTER_PORT;
        config.pin_pwdn = CAMERA_PIN_PWDN; config.pin_reset = CAMERA_PIN_RESET;
        config.xclk_freq_hz = XCLK_FREQ_HZ; config.pixel_format = PIXFORMAT_RGB565;
        config.frame_size = FRAMESIZE_VGA; config.jpeg_quality = 12; config.fb_count = 1;
        config.fb_location = CAMERA_FB_IN_PSRAM; config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
        camera_ = new Esp32Camera(config);
        if (!camera_) { ESP_LOGE(TAG, "Camera initialization failed!"); }
    }

public:
    LichuangDevPlusBoard() : DualNetworkBoard(ML307_TX_PIN, ML307_RX_PIN, ML307_RX_BUFFER_SIZE), boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();
        InitializeInterrupts();
        InitializePmic();
        InitializeSpi();
        InitializeSt7789Display();
        InitializeTouch();
        InitializeButtons();
        InitializePowerSaveTimer();
        InitializeCamera();
        
        check_headphone_status(); 
        GetBacklight()->RestoreBrightness();
    }

    virtual ~LichuangDevPlusBoard() {
        if (display_) { delete display_; }
        if (pmic_) { delete pmic_; }
        if (aw9523b_) { delete aw9523b_; }
        if (power_save_timer_) { delete power_save_timer_; }
    }

    virtual AudioCodec* GetAudioCodec() override {
        static LichuangDevPlusAudioCodec audio_codec(i2c_bus_, aw9523b_);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override { return display_; }
    virtual Camera* GetCamera() override { return camera_; }

    virtual Backlight* GetBacklight() override {
        static Aw9523bBacklight backlight(aw9523b_, AW9523B_PIN_BACKLIGHT);
        return &backlight;
    }

    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        if (!pmic_) return false;
        static bool last_discharging = false;
        charging = pmic_->IsCharging(); discharging = pmic_->IsDischarging();
        if (discharging != last_discharging) { power_save_timer_->SetEnabled(discharging); last_discharging = discharging; }
        level = pmic_->GetBatteryLevel();
        return true;
    }
    
    virtual bool GetTemperature(float& temp) override { return false; }

	virtual void SetPowerSaveMode(bool enabled) override {
        if (!enabled) { power_save_timer_->WakeUp(); }
        DualNetworkBoard::SetPowerSaveMode(enabled);
    }
};

DECLARE_BOARD(LichuangDevPlusBoard);