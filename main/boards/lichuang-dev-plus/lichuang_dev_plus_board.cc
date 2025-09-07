#include "dual_network_board.h" 
#include "codecs/box_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "i2c_device.h"
#include "axp2101.h"
#include "power_save_timer.h"
#include "esp32_camera.h"

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <esp_timer.h>
#include <wifi_station.h> 
#include <esp_lcd_touch_ft5x06.h>
#include <esp_lvgl_port.h>
#include <lvgl.h>


#define TAG "LichuangDevPlusBoard"

LV_FONT_DECLARE(font_puhui_basic_20_4);
LV_FONT_DECLARE(font_awesome_20_4);

class Pmic : public Axp2101 {
public:
    Pmic(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : Axp2101(i2c_bus, addr) {
        ESP_LOGI(TAG, "Init AXP2101 PMIC");
        
        WriteReg(0x22, 0b110); // PWRON > OFFLEVEL as POWEROFF Source enable
        WriteReg(0x27, 0x21);  // hold 4s to power off

        WriteReg(0x92, 0x1C); // 配置 aldo1 输出为 3.3V
        WriteReg(0x93, 0x17); // 配置 aldo2 输出为 2.8V
    
        uint8_t value = ReadReg(0x90); // XPOWERS_AXP2101_LDO_ONOFF_CTRL0
        value = value | 0x02; // set bit 1 (ALDO2)
        WriteReg(0x90, value);  // and power channels now enabled
    
        WriteReg(0x64, 0x03); // CV charger voltage setting to 4.2V
        
        WriteReg(0x61, 0x05); // set Main battery precharge current to 125mA
        WriteReg(0x62, 0x0A); // set Main battery charger current to 400mA ( 0x08-200mA, 0x09-300mA, 0x0A-400mA )
        WriteReg(0x63, 0x15); // set Main battery term charge current to 125mA
    
        WriteReg(0x14, 0x00); // set minimum system voltage to 4.1V (default 4.7V), for poor USB cables
        WriteReg(0x15, 0x00); // set input voltage limit to 3.88v, for poor USB cables
        WriteReg(0x16, 0x05); // set input current limit to 2000mA
    
        WriteReg(0x24, 0x01); // set Vsys for PWROFF threshold to 3.2V (default - 2.6V and kill battery)
        WriteReg(0x50, 0x14); // set TS pin to EXTERNAL input (not temperature)
    }

    void SetBacklightRegValue(uint8_t reg_val) {
        if (reg_val > 0x1F) { // 0x1F 是 31，ALDO的最大设置值
            reg_val = 0x1F;
        }
        WriteReg(0x93, reg_val);
    }

    void EnableBacklight(bool enable) {
        uint8_t ldo_en_ctrl = ReadReg(0x90);
        if (enable) {
            ldo_en_ctrl |= (1 << 1);
        } else {
            ldo_en_ctrl &= ~(1 << 1);
        }
        WriteReg(0x90, ldo_en_ctrl);
    }
};

class Aw9523b : public I2cDevice {
public:
    Aw9523b(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
        WriteReg(0x7F, 0x00); vTaskDelay(pdMS_TO_TICKS(10));
        WriteReg(0x11, 0x01); 
        vTaskDelay(pdMS_TO_TICKS(10));
        WriteReg(0x02, 0x00);  // P0 初始输出状态
        WriteReg(0x03, 0x00);  // P1 初始输出状态  
        WriteReg(0x04, 0x00);  // P0 配置为输出
        WriteReg(0x05, 0x00);  // P1 配置为输出
        WriteReg(0x12, 0x05);  // P0_0,P0_2 为GPIO模式（只使用这两个引脚）
        WriteReg(0x13, 0x00);  // P1 全部为GPIO模式
    }

    void SetOutputState(uint8_t bit, uint8_t level) {
        uint8_t reg = (bit < 8) ? 0x02 : 0x03;
        uint8_t pin = (bit < 8) ? bit : bit - 8;
        uint8_t data = ReadReg(reg);
        data = (data & ~(1 << pin)) | (level << pin);
        WriteReg(reg, data);
    }
};


class PmicBacklight : public Backlight {
private: 
    Pmic* pmic_;
    static constexpr const char* BL_TAG = "PmicBacklight"; 

public:
    PmicBacklight(Pmic* pmic) : pmic_(pmic) {}

    void SetBrightnessImpl(uint8_t brightness) override {
        if (!pmic_) {
            ESP_LOGE(BL_TAG, "PMIC not initialized!");
            return;
        }

        if (brightness == 0) {
            pmic_->EnableBacklight(false);
            ESP_LOGI(BL_TAG, "Backlight OFF");
            return;
        }
        
        pmic_->EnableBacklight(true);
        uint8_t reg_val = (brightness >> 5) + 20; // 计算寄存器值，范围从20到27
        pmic_->SetBacklightRegValue(reg_val); //计算出的值写入ALDO2的电压控制寄存器 (0x93)

        ESP_LOGI(BL_TAG, "Set brightness to %u", brightness);
    }
};


class LichuangDevPlusAudioCodec : public BoxAudioCodec {
private:
    Aw9523b* aw9523b_;

public:
    LichuangDevPlusAudioCodec(i2c_master_bus_handle_t i2c_bus, Aw9523b* aw9523b) 
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
          aw9523b_(aw9523b) {
    }

    virtual void EnableOutput(bool enable) override {
        BoxAudioCodec::EnableOutput(enable);
        if (enable) {
            aw9523b_->SetOutputState(0, 1);
        } else {
            aw9523b_->SetOutputState(0, 0);
        }
    }
};


class LichuangDevPlusBoard : public DualNetworkBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    Pmic* pmic_;
    Button boot_button_;
    LcdDisplay* display_;
    Aw9523b* aw9523b_;
    Esp32Camera* camera_;
    PowerSaveTimer* power_save_timer_;

    void InitializePowerSaveTimer() {
        power_save_timer_ = new PowerSaveTimer(-1, 60, 300);
        power_save_timer_->OnEnterSleepMode([this]() {
            ESP_LOGI(TAG, "Enabling sleep mode");
            auto display = GetDisplay();
            display->SetChatMessage("system", "");
            display->SetEmotion("sleepy");
            GetBacklight()->SetBrightness(30); });
        power_save_timer_->OnExitSleepMode([this]() {
            auto display = GetDisplay();
            display->SetChatMessage("system", "");
            display->SetEmotion("neutral");
            GetBacklight()->RestoreBrightness(); });
        power_save_timer_->OnShutdownRequest([this](){ 
            pmic_->PowerOff(); });
        power_save_timer_->SetEnabled(true);
    }

    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)1,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .flags = { .enable_internal_pullup = true },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));

        // Initialize Aw9523b
        aw9523b_ = new Aw9523b(i2c_bus_, 0x58);
    }

    void InitializePmic() { 
        pmic_ = new Pmic(i2c_bus_, 0x34); 
    }


    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_SCLK_PIN;
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
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
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
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, true);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        display_ = new SpiLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                    {&font_puhui_basic_20_4, &font_awesome_20_4});
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
                .swap_xy = DISPLAY_SWAP_XY,
                .mirror_x = DISPLAY_MIRROR_X,
                .mirror_y = DISPLAY_MIRROR_Y,
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
        // Open camera power
        aw9523b_->SetOutputState(2, 0);
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
        config.pin_sccb_sda = -1;
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
        if (!camera_) { 
            ESP_LOGE(TAG, "Camera initialization failed!"); 
            return;
        }
        
        // 获取底层的 sensor_t 对象
        sensor_t *s = esp_camera_sensor_get();
        if (s) {
            // 设置垂直翻转
            s->set_vflip(s, 1); 
            ESP_LOGI(TAG, "Camera vertical flip enabled.");

            // 可以同时开启水平翻转
            // s->set_hmirror(s, 1);
            // ESP_LOGI(TAG, "Camera horizontal mirror enabled.");
        } else {
            ESP_LOGE(TAG, "Failed to get camera sensor handle!");
        }
    }
    
public:
    LichuangDevPlusBoard() : DualNetworkBoard(ML307_TX_PIN, ML307_RX_PIN), boot_button_(BOOT_BUTTON_GPIO) {
        InitializePowerSaveTimer();
        InitializeI2c();
        vTaskDelay(pdMS_TO_TICKS(100));
        InitializePmic();
        GetBacklight()->RestoreBrightness();
        InitializeSpi();
        InitializeSt7789Display();
        InitializeButtons();
        InitializeTouch();
        InitializeCamera();
    }

    virtual Assets* GetAssets() override {
        static Assets assets(ASSETS_XIAOZHI_PUHUI_COMMON_20_4_EMOJI_64);
        return &assets;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static LichuangDevPlusAudioCodec audio_codec(i2c_bus_, aw9523b_);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Camera* GetCamera() override {
        return camera_;
    }

    virtual Backlight* GetBacklight() override {
        static PmicBacklight backlight(pmic_);
        return &backlight;
    }

    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        if (!pmic_) return false;
        static bool last_discharging = false;
        static bool initialized = false;
        
        charging = pmic_->IsCharging();
        discharging = pmic_->IsDischarging();
        
        // 添加调试信息
        static int debug_counter = 0;
        if (++debug_counter % 100 == 0) { // 每100次调用打印一次
            ESP_LOGI(TAG, "Battery status: charging=%d, discharging=%d, level=%d", 
                     charging, discharging, pmic_->GetBatteryLevel());
        }
        
        // 放电状态变化时，控制PowerSaveTimer
        if (!initialized || discharging != last_discharging) {
            if (discharging) {
                // 放电时启用省电模式
                power_save_timer_->SetEnabled(true);
                ESP_LOGI(TAG, "Discharging detected, enabling power save timer");
            } else {
                // 不在放电时禁用省电模式（充电或充电完成但插着充电器）
                power_save_timer_->SetEnabled(false);
                ESP_LOGI(TAG, "Not discharging, disabling power save timer");
            }
            last_discharging = discharging;
            initialized = true;
        }
        
        level = pmic_->GetBatteryLevel();
        return true;
    }


    virtual void SetPowerSaveMode(bool enabled) override {
        if (!enabled) {
            power_save_timer_->WakeUp();
        }
        DualNetworkBoard::SetPowerSaveMode(enabled);
    }
};

DECLARE_BOARD(LichuangDevPlusBoard);
