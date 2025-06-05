#include "dual_network_board.h" 
#include "audio_codecs/box_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "i2c_device.h"
#include "iot/thing_manager.h"
#include "axp2101.h"
#include "power_save_timer.h"
#include "esp32_camera.h" // For Esp32Camera
#include <esp_lcd_touch_ft5x06.h> // For FT5x06 touch
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

// Forward declaration
class Pca9557;

class Pmic : public Axp2101 {
public:
    Pmic(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : Axp2101(i2c_bus, addr) {
        // From esp32-s3-wrist-gem:
        WriteReg(0x22, 0b110); // PWRON > OFFLEVEL as POWEROFF Source enable
        WriteReg(0x27, 0x10);  // hold 4s to power off

        WriteReg(0x92, 0x1C); // 配置 aldo1 输出为 3.3V

        uint8_t value = ReadReg(0x90); // XPOWERS_AXP2101_LDO_ONOFF_CTRL0
        value = value | 0x01; // set bit 1 (ALDO1)
        WriteReg(0x90, value);  // and power channels now enabled

        WriteReg(0x64, 0x03); // CV charger voltage setting to 4.2V

        WriteReg(0x61, 0x05); // set Main battery precharge current to 125mA
        WriteReg(0x62, 0x08); // set Main battery charger current to 200mA ( 0x08-200mA, 0x09-300mA, 0x0A-400mA )
        WriteReg(0x63, 0x15); // set Main battery term charge current to 125mA

        WriteReg(0x14, 0x00); // set minimum system voltage to 4.1V (default 4.7V), for poor USB cables
        WriteReg(0x15, 0x00); // set input voltage limit to 3.88v, for poor USB cables
        WriteReg(0x16, 0x05); // set input current limit to 2000mA

        WriteReg(0x24, 0x01); // set Vsys for PWROFF threshold to 3.2V (default - 2.6V and kill battery)
        WriteReg(0x50, 0x14); // set TS pin to EXTERNAL input (not temperature)
    }
};

class Pca9557 : public I2cDevice {
public:
    Pca9557(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
        // Initialize output states for IO0, IO1, IO2 to low (PCA9557 default is 1).
        // IO0: LCD Backlight (original lichuang-dev uses direct GPIO) -> Assuming this is for other purposes or Display PMIC.
        // IO1: Speaker Amp Control
        // IO2: Camera Power
        //暂时注销WriteReg(0x01, 0x00);
        // Configure IO0, IO1, IO2 as outputs (0), rest (IO3-IO7) as inputs (1).
        // Binary: 0b11111000 -> Hex 0xF0.
        //暂时注销WriteReg(0x03, 0xF0);
        WriteReg(0x01, 0x03);
        WriteReg(0x03, 0xf8);
    }

    void SetOutputState(uint8_t bit, uint8_t level) {
        uint8_t data = ReadReg(0x01); // Read current output port values
        data = (data & ~(1 << bit)) | (level << bit);
        WriteReg(0x01, data); // Write updated output port values
    }

    uint8_t ReadInputs() {
        return ReadReg(0x00); // Input Port Register
    }

    bool GetInputState(uint8_t bit) {
        uint8_t inputs = ReadInputs();
        return (inputs & (1 << bit)) == 0; // PCA955x inputs are active low (pressed = 0)
                                           // So, we return true if the bit IS 0.
                                           // This depends on pull-up/pull-down configuration.
                                           // If the button pulls the line low when pressed, then (inputs & (1 << bit)) == 0 means pressed.
    }
};

class LichuangDevPlusAudioCodec : public BoxAudioCodec {
private:
    Pca9557* pca9557_;
    bool speaker_enabled_ = false; // To track speaker state

public:
    LichuangDevPlusAudioCodec(i2c_master_bus_handle_t i2c_bus, Pca9557* pca9557) 
        : BoxAudioCodec(i2c_bus, 
                       AUDIO_INPUT_SAMPLE_RATE, 
                       AUDIO_OUTPUT_SAMPLE_RATE,
                       AUDIO_I2S_GPIO_MCLK, 
                       AUDIO_I2S_GPIO_BCLK, 
                       AUDIO_I2S_GPIO_WS, 
                       AUDIO_I2S_GPIO_DOUT, 
                       AUDIO_I2S_GPIO_DIN,
                       GPIO_NUM_NC, 
                       AUDIO_CODEC_ES8311_ADDR, 
                       AUDIO_CODEC_ES7210_ADDR, 
                       AUDIO_INPUT_REFERENCE),
          pca9557_(pca9557) {
    }

    virtual void EnableOutput(bool enable) override {
        BoxAudioCodec::EnableOutput(enable); // Call base method for ES8311
        if (pca9557_) {
            if (enable) {
                // Enable NS4150B connected to PCA9557 IO1
                pca9557_->SetOutputState(PCA9557_PIN_SPEAKER_CTRL, SPEAKER_ENABLE_LEVEL);
                speaker_enabled_ = true;
            } else {
                // Disable NS4150B
                pca9557_->SetOutputState(PCA9557_PIN_SPEAKER_CTRL, !SPEAKER_ENABLE_LEVEL);
                speaker_enabled_ = false;
            }
        }
    }

    bool IsSpeakerEnabled() const {
        return speaker_enabled_;
    }
};

class LichuangDevPlusBoard : public DualNetworkBoard { // Changed base class
private:
    i2c_master_bus_handle_t i2c_bus_;
    Button boot_button_;
    Pca9557* pca9557_;
    Pmic* pmic_ = nullptr;
    LcdDisplay* display_;
    esp_timer_handle_t headphone_check_timer_;
    PowerSaveTimer* power_save_timer_ = nullptr;
    Esp32Camera* camera_ = nullptr;
    esp_timer_handle_t volume_check_timer_;
    bool volume_up_pressed_ = false;
    bool volume_down_pressed_ = false;

    static void headphone_check_callback(void* arg) {
        reinterpret_cast<LichuangDevPlusBoard*>(arg)->check_headphone_status();
    }

    static void volume_check_callback(void* arg) {
        reinterpret_cast<LichuangDevPlusBoard*>(arg)->check_volume_buttons();
    }

    void check_headphone_status() {
        if (!pca9557_ || !GetAudioCodec()) return;

        bool headphone_inserted = pca9557_->GetInputState(PCA9557_PIN_HEADPHONE_DETECT);

        auto* codec = static_cast<LichuangDevPlusAudioCodec*>(GetAudioCodec()); // Cast to derived type
        if (headphone_inserted) {
            if (codec->IsSpeakerEnabled()) {
                codec->EnableOutput(false); // Disables NS4150B via PCA
                ESP_LOGI(TAG, "Headphone inserted, onboard speaker OFF");
            }
        } else {
            if (!codec->IsSpeakerEnabled()) {
                codec->EnableOutput(true);  // Enables NS4150B via PCA
                ESP_LOGI(TAG, "Headphone removed, onboard speaker ON");
            }
        }
    }

    void check_volume_buttons() {
        if (!pca9557_) return;

        bool current_volume_up_state = pca9557_->GetInputState(PCA9557_PIN_VOLUME_UP);
        bool current_volume_down_state = pca9557_->GetInputState(PCA9557_PIN_VOLUME_DOWN);

        if (power_save_timer_) power_save_timer_->WakeUp(); // Wake up on any volume button interaction

        // Volume Up logic (simplified from Button class handlers)
        if (current_volume_up_state && !volume_up_pressed_) {
            // Button pressed (falling edge, assuming active low)
            volume_up_pressed_ = true;
            auto codec = GetAudioCodec();
            if (codec) {
                auto volume = codec->output_volume() + 10;
                if (volume > 100) volume = 100;
                codec->SetOutputVolume(volume);
                if (GetDisplay()) GetDisplay()->ShowNotification("Volume: " + std::to_string(volume));
            }
        } else if (!current_volume_up_state && volume_up_pressed_) {
            // Button released (rising edge)
            volume_up_pressed_ = false;
        }

        // Volume Down logic (simplified from Button class handlers)
        if (current_volume_down_state && !volume_down_pressed_) {
            // Button pressed (falling edge, assuming active low)
            volume_down_pressed_ = true;
            auto codec = GetAudioCodec();
            if (codec) {
                auto volume = codec->output_volume() - 10;
                if (volume < 0) volume = 0;
                codec->SetOutputVolume(volume);
                if (GetDisplay()) GetDisplay()->ShowNotification("Volume: " + std::to_string(volume));
            }
        } else if (!current_volume_down_state && volume_down_pressed_) {
            // Button released (rising edge)
            volume_down_pressed_ = false;
        }
    }

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

        // Initialize PCA9557 (merged from InitializePca9557)
        pca9557_ = new Pca9557(i2c_bus_, 0x19);
    }

    void InitializePmic() {
        // Initialize PMIC (AXP2101)
        //暂时注销pmic_ = new Pmic(i2c_bus_, AXP2101_I2C_ADDR);
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = GPIO_NUM_40;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = GPIO_NUM_41;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeSt7789Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = GPIO_NUM_NC;
        io_config.dc_gpio_num = GPIO_NUM_39;
        io_config.spi_mode = 2;
        io_config.pclk_hz = 80 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片ST7789
        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
        
        esp_lcd_panel_reset(panel);
        pca9557_->SetOutputState(0, 0);

        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, true);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
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
    }

    void InitializeTouch()
    {
        esp_lcd_touch_handle_t tp;
        esp_lcd_touch_config_t tp_cfg = {
            .x_max = DISPLAY_WIDTH,
            .y_max = DISPLAY_HEIGHT,
            .rst_gpio_num = GPIO_NUM_NC, // Shared with LCD reset
            .int_gpio_num = GPIO_NUM_NC, 
            .levels = {
                .reset = 0,
                .interrupt = 0,
            },
            .flags = {
                .swap_xy = 1,
                .mirror_x = 1,
                .mirror_y = 0,
            },
        };
        esp_lcd_panel_io_handle_t tp_io_handle = NULL;
        esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
        tp_io_config.scl_speed_hz = 400000;

        esp_lcd_new_panel_io_i2c(i2c_bus_, &tp_io_config, &tp_io_handle);
        esp_lcd_touch_new_i2c_ft5x06(tp_io_handle, &tp_cfg, &tp);
        assert(tp);

        /* Add touch input (for selected screen) */
        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = lv_display_get_default(), 
            .handle = tp,
        };

        lvgl_port_add_touch(&touch_cfg);
    }

    void InitializePowerSaveTimer() {
        // Using timings from esp32-s3-wrist-gem (240s idle, 60s warning, 300s total to sleep)
        // Or adjust as needed. The xingzhi-cube example used (-1, 60, 300)
        power_save_timer_ = new PowerSaveTimer(240, 60, 300);
        power_save_timer_->OnEnterSleepMode([this]() {
            ESP_LOGI(TAG, "Entering sleep mode via PowerSaveTimer");
            // Actions from esp32-s3-wrist-gem:
            if (GetDisplay()) {
                GetDisplay()->SetChatMessage("system", ""); // Clear chat
                GetDisplay()->SetEmotion("sleepy");
            }
            if (GetAudioCodec()) GetAudioCodec()->EnableInput(false); // Disable audio input
            if (GetBacklight()) GetBacklight()->SetBrightness(10); // Dim backlight
            // Add other necessary sleep preparations
        });
        power_save_timer_->OnExitSleepMode([this]() {
            ESP_LOGI(TAG, "Exiting sleep mode via PowerSaveTimer");
            // Actions from esp32-s3-wrist-gem:
            if (GetAudioCodec()) GetAudioCodec()->EnableInput(true); // Re-enable audio input
            if (GetDisplay()) {
                GetDisplay()->SetChatMessage("system", "");
                GetDisplay()->SetEmotion("neutral");
            }
            if (GetBacklight()) GetBacklight()->RestoreBrightness();
            // Add other necessary wake-up actions
        });
        power_save_timer_->OnShutdownRequest([this]() {
            ESP_LOGI(TAG, "Shutdown requested via PowerSaveTimer");
            if (pmic_) pmic_->PowerOff();
        });
        power_save_timer_->SetEnabled(true); // Initially enable the timer
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            if (power_save_timer_) power_save_timer_->WakeUp();
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
                return; // AEC handled, exit
            }
#endif
            // If AEC not handled or CONFIG_USE_DEVICE_AEC is not defined, proceed with network switch
            if (app.GetDeviceState() == kDeviceStateStarting || app.GetDeviceState() == kDeviceStateWifiConfiguring) {
                SwitchNetworkType();
            }
        });

        // Keep existing PressDown, PressUp, LongPress for boot_button_ if they are still relevant
        // For example, StartListening/StopListening are likely still good.
        // LongPress for AXP2101 power off is also fine as a comment or log.
        boot_button_.OnPressDown([this]() {
            if (power_save_timer_) power_save_timer_->WakeUp(); // Good practice to wake on any interaction
            Application::GetInstance().StartListening();
        });
        boot_button_.OnPressUp([this]() {
            if (power_save_timer_) power_save_timer_->WakeUp();
            Application::GetInstance().StopListening();
        });
    }

    void InitializeCamera() {
        // Open camera power
        if (pca9557_) pca9557_->SetOutputState(2, 0);

        camera_config_t config = {};
        config.ledc_channel = LEDC_CHANNEL_2;
        config.ledc_timer = LEDC_TIMER_2;
        config.pin_d0 = CAMERA_PIN_D0;
        config.pin_d1 = CAMERA_PIN_D1;
        config.pin_d2 = CAMERA_PIN_D2;
        config.pin_d3 = CAMERA_PIN_D3;
        config.pin_d4 = CAMERA_PIN_D4;
        config.pin_d5 = CAMERA_PIN_D5;
        config.pin_d6 = CAMERA_PIN_D6;
        config.pin_d7 = CAMERA_PIN_D7;
        config.pin_xclk = CAMERA_PIN_XCLK;
        config.pin_pclk = CAMERA_PIN_PCLK;
        config.pin_vsync = CAMERA_PIN_VSYNC;
        config.pin_href = CAMERA_PIN_HREF;
        config.pin_sccb_sda = -1;   // 这里写-1 表示使用已经初始化的I2C接口
        config.pin_sccb_scl = CAMERA_PIN_SIOC;
        config.sccb_i2c_port = 1;
        config.pin_pwdn = CAMERA_PIN_PWDN;
        config.pin_reset = CAMERA_PIN_RESET;
        config.xclk_freq_hz = XCLK_FREQ_HZ;
        config.pixel_format = PIXFORMAT_RGB565;
        config.frame_size = FRAMESIZE_VGA;
        config.jpeg_quality = 12;
        config.fb_count = 1;
        config.fb_location = CAMERA_FB_IN_PSRAM;
        config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

        camera_ = new Esp32Camera(config);
        if (!camera_ /* || !camera_->IsInitOk() */) {
            ESP_LOGE(TAG, "Camera initialization failed!");
            camera_ = nullptr;
        } else {
             ESP_LOGI(TAG, "Camera Initialized successfully.");
        }
    }

    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("Backlight"));
        thing_manager.AddThing(iot::CreateThing("Battery"));
    }

public:
    LichuangDevPlusBoard() : DualNetworkBoard(ML307_TX_PIN, ML307_RX_PIN, ML307_RX_BUFFER_SIZE), boot_button_(BOOT_BUTTON_GPIO), pca9557_(nullptr), pmic_(nullptr), display_(nullptr), headphone_check_timer_(nullptr), power_save_timer_(nullptr), camera_(nullptr) {
        InitializeI2c();
        InitializePmic();
        InitializeSpi();
        InitializeSt7789Display();
        InitializeTouch();
        InitializeButtons();
        InitializePowerSaveTimer();
        InitializeCamera();
        InitializeIot();

        //暂时注销check_headphone_status();
        GetBacklight()->RestoreBrightness();

        const esp_timer_create_args_t headphone_timer_args = {
            .callback = &LichuangDevPlusBoard::headphone_check_callback,
            .arg = this,
            .name = "headphone_check"
        };
        ESP_ERROR_CHECK(esp_timer_create(&headphone_timer_args, &headphone_check_timer_));
        ESP_ERROR_CHECK(esp_timer_start_periodic(headphone_check_timer_, 500000));

        const esp_timer_create_args_t volume_timer_args = {
            .callback = &LichuangDevPlusBoard::volume_check_callback,
            .arg = this,
            .name = "volume_check"
        };
        ESP_ERROR_CHECK(esp_timer_create(&volume_timer_args, &volume_check_timer_));
        ESP_ERROR_CHECK(esp_timer_start_periodic(volume_check_timer_, 50000));
    }

    virtual ~LichuangDevPlusBoard() {
        if (display_) delete display_;
        if (pmic_) delete pmic_;
        if (pca9557_) delete pca9557_;
        if (power_save_timer_) delete power_save_timer_;
        // if (camera_) delete camera_; // Removed due to non-virtual destructor constraint

        if (headphone_check_timer_) esp_timer_delete(headphone_check_timer_);
        if (volume_check_timer_) esp_timer_delete(volume_check_timer_);
    }

    virtual AudioCodec* GetAudioCodec() override {
        static LichuangDevPlusAudioCodec audio_codec(i2c_bus_, pca9557_);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }
    
    virtual Camera* GetCamera() override {
        return camera_;
    }
    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        if (!pmic_) return false;
        static bool last_discharging = false;
        charging = pmic_->IsCharging();
        discharging = pmic_->IsDischarging();
        if (discharging != last_discharging) {
            power_save_timer_->SetEnabled(discharging);
            last_discharging = discharging;
        }

        level = pmic_->GetBatteryLevel();
        return true;
    }

    virtual bool GetTemperature(float& esp32temp) override {
        esp32temp = 0.0f; // Default to 0.0f
        return false; // Indicate that temperature is not obtained
    }
};

DECLARE_BOARD(LichuangDevPlusBoard);
