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
        ESP_LOGI(TAG, "--- PMIC Minimal & Correct Configuration ---");

        // =================================================================
        // Part 1: 保留你原始代码中所有安全且必要的配置
        // =================================================================
        // 配置充电
        WriteReg(0x33, 0b11000100); // 建议使能充电 (bit 7=1), 如果不需要充电再设为 0b01000100
        
        // 配置VBUS
        WriteReg(0x30, 0b01100001); 
        
        // 配置TS
        WriteReg(0x34, 0x00);
        
        // =================================================================
        // Part 2: 只设置和开启硬件上确实用到的 LDO
        // =================================================================
        // 设置电压
        WriteReg(0x92, 0x1C); // ALDO1 -> 3.3V (给 AU_3V3)
        WriteReg(0x93, 0x17); // ALDO2 -> 2.8V (给 屏幕背光, 2.8V作为默认值)

        // **关键修正：只开启 ALDO1 和 ALDO2**
        uint8_t ldo_onoff_ctrl0 = ReadReg(0x90); 
        ldo_onoff_ctrl0 |= (1 << 0) | (1 << 1); // 只使能 ALDO1 和 ALDO2
        ldo_onoff_ctrl0 &= ~(1 << 4);           // 明确地禁用 BLDO1，以防万一
        WriteReg(0x90, ldo_onoff_ctrl0);

        // =================================================================
        // Part 3: 不再触碰任何开关机相关的寄存器 (0x25, 0x26, 0x27)
        // 让芯片使用它自己的、工作正常的出厂默认设置！
        // =================================================================
        ESP_LOGI(TAG, "PMIC configuration complete, relying on factory defaults for power on/off.");
    }
    
    // PowerOff 函数依然保留，用于软件主动关机
    void PowerOff() {
        uint8_t val = ReadReg(0x32);
        WriteReg(0x32, val | (1 << 7));
    }

    // 背光控制函数 (使用修正后的版本)
    void SetBacklightVoltage(float voltage) {
        if (voltage < 2.5f) voltage = 2.5f;
        if (voltage > 3.3f) voltage = 3.3f;
        uint8_t reg_val = (uint8_t)(((voltage * 1000) - 500) / 100.0f);
        WriteReg(0x93, reg_val);
    }

    void EnableBacklight(bool enable) {
        // 使用正确的 ALDO2 开关: REG 0x90, bit 1
        uint8_t ldo_en_ctrl = ReadReg(0x90);
        if (enable) {
            ldo_en_ctrl |= (1 << 1);
        } else {
            ldo_en_ctrl &= ~(1 << 1);
        }
        WriteReg(0x90, ldo_en_ctrl);
    }
};

// ======================================================================
//                      AW9523B (纯IO扩展)
// ======================================================================
class Aw9523b : public I2cDevice {
public:
    Aw9523b(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {}
    void Init() {
        ESP_LOGI(TAG, "Initializing AW9523B...");
        WriteReg(0x7F, 0x00); vTaskDelay(pdMS_TO_TICKS(10));
        WriteReg(0x11, 0x01); 
        vTaskDelay(pdMS_TO_TICKS(10));
        // 只配置实际用到的GPIO
        uint16_t gpio_mode_mask = (1 << AW9523B_PIN_PA_EN) | (1 << AW9523B_PIN_DVP_PWDN);
        WriteReg(0x12, (uint8_t)(gpio_mode_mask & 0xFF));
        WriteReg(0x13, 0x00); // P1口没有用到
        WriteReg(0x04, 0x00); // 全部设为输出
        WriteReg(0x05, 0x00);
    }
    void SetGpio(uint8_t pin, bool level) {
        uint8_t reg = (pin < 8) ? 0x02 : 0x03; uint8_t bit = (pin < 8) ? pin : pin - 8;
        uint8_t data = ReadReg(reg);
        if (level) { data |= (1 << bit); } else { data &= ~(1 << bit); }
        WriteReg(reg, data);
    }
};

// ======================================================================
//              背光控制类: 通过PMIC调节ALDO2电压
// ======================================================================
class PmicBacklight : public Backlight {
private: 
    Pmic* pmic_;
public:
    PmicBacklight(Pmic* pmic) : pmic_(pmic) {}
    void SetBrightnessImpl(uint8_t brightness) override {
        if (!pmic_) return;

        if (brightness == 0) {
            pmic_->EnableBacklight(false);
            ESP_LOGI(TAG, "Backlight OFF");
            return;
        }
        
        // 确保背光电源是开启的
        pmic_->EnableBacklight(true);

        // 亮度 10-100 映射到电压 2.5V - 3.3V
        const float min_volt = 2.5f;
        const float max_volt = 3.3f;
        const uint8_t min_brightness = 10;
        const uint8_t max_brightness = 100;

        // 对输入亮度进行钳位
        if (brightness < min_brightness) {
            brightness = min_brightness;
        }
        if (brightness > max_brightness) {
            brightness = max_brightness;
        }

        // 线性映射
        float target_volt = min_volt + ((float)(brightness - min_brightness) / (max_brightness - min_brightness)) * (max_volt - min_volt);
        
        pmic_->SetBacklightVoltage(target_volt);
    }
};

// ======================================================================
//                         音频 CODEC
// ======================================================================
class LichuangDevPlusAudioCodec : public BoxAudioCodec {
private:
    Aw9523b* expander_; 
public:
    LichuangDevPlusAudioCodec(i2c_master_bus_handle_t i2c_bus, Aw9523b* expander) 
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
          expander_(expander) {}
    virtual void EnableOutput(bool enable) override {
        BoxAudioCodec::EnableOutput(enable);
        if (expander_) { expander_->SetGpio(AW9523B_PIN_PA_EN, enable); }
    }
};

// ======================================================================
//                          主板类定义
// ======================================================================
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

    // 辅助函数: I2C扫描
/*     void I2cScan() {
        ESP_LOGI(TAG, "================ I2C SCAN START ================");
        ESP_LOGI(TAG, "Scanning I2C bus (Port:%d, SDA:%d, SCL:%d)...", I2C_MASTER_PORT, I2C_MASTER_SDA_PIN, I2C_MASTER_SCL_PIN);
        for (uint8_t i = 1; i < 127; i++) {
            esp_err_t ret = i2c_master_probe(i2c_bus_, i, 100 / portTICK_PERIOD_MS);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, ">>> I2C device found at address 0x%02X (%d)", i, i);
            } else if (ret != ESP_ERR_TIMEOUT) {
                ESP_LOGW(TAG, "Error probing address 0x%02X: %s", i, esp_err_to_name(ret));
            }
        }
        ESP_LOGI(TAG, "================ I2C SCAN END ================");
    } */
    
    // 初始化函数
    void InitializeI2cBus() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)I2C_MASTER_PORT,
            .sda_io_num = I2C_MASTER_SDA_PIN,
            .scl_io_num = I2C_MASTER_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .flags = { .enable_internal_pullup = true },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
        ESP_LOGI(TAG, "I2C master bus created.");
    }

    void InitializePmic() { pmic_ = new Pmic(i2c_bus_, AXP2101_I2C_ADDR); }

    void InitializeIoExpander() {
        aw9523b_ = new Aw9523b(i2c_bus_, AW9523B_I2C_ADDR);
        aw9523b_->Init();
    }
    

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
        io_config.cs_gpio_num = DISPLAY_SPI_CS;
        io_config.dc_gpio_num = DISPLAY_SPI_DC;
        io_config.spi_mode = 2; io_config.pclk_hz = 80 * 1000 * 1000;
        io_config.trans_queue_depth = 10; io_config.lcd_cmd_bits = 8; io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((spi_host_device_t)DISPLAY_SPI_HOST, &io_config, &panel_io));
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
        esp_lcd_panel_reset(panel);
        /* esp_lcd_panel_reset(panel); // 1. 软件复位
        if (aw9523b_) {
            aw9523b_->SetGpio(AW9523B_PIN_LCD_CS, false); // 2. 通过IO扩展芯片将CS拉低
        } */
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

    void InitializeTouch() {
        esp_lcd_touch_handle_t tp = nullptr;
        esp_lcd_touch_config_t tp_cfg = { .x_max = DISPLAY_WIDTH, .y_max = DISPLAY_HEIGHT, .rst_gpio_num = GPIO_NUM_NC, .int_gpio_num = GPIO_NUM_NC, .levels = { .reset = 0, .interrupt = 0, }, .flags = { .swap_xy = DISPLAY_SWAP_XY, .mirror_x = DISPLAY_MIRROR_X, .mirror_y = DISPLAY_MIRROR_Y, }, };
        esp_lcd_panel_io_handle_t tp_io_handle = NULL;
        esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
        tp_io_config.scl_speed_hz = 400000;
        esp_lcd_new_panel_io_i2c(i2c_bus_, &tp_io_config, &tp_io_handle);
        esp_err_t ret = esp_lcd_touch_new_i2c_ft5x06(tp_io_handle, &tp_cfg, &tp);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Touch controller initialization failed!");
            return;
        }
        const lvgl_port_touch_cfg_t touch_cfg = { .disp = lv_display_get_default(), .handle = tp, };
        lvgl_port_add_touch(&touch_cfg);
    }

    void InitializePowerSaveTimer() {
        power_save_timer_ = new PowerSaveTimer(240, 60, 300);
        power_save_timer_->OnEnterSleepMode([this]() { ESP_LOGI(TAG, "Entering sleep mode"); if (GetBacklight()) GetBacklight()->SetBrightness(0); });
        power_save_timer_->OnExitSleepMode([this]() { ESP_LOGI(TAG, "Exiting sleep mode"); if (GetBacklight()) GetBacklight()->RestoreBrightness(); });
        power_save_timer_->OnShutdownRequest([this]() { ESP_LOGI(TAG, "Shutdown requested"); if (pmic_) pmic_->PowerOff(); });
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
        if (aw9523b_) { aw9523b_->SetGpio(AW9523B_PIN_DVP_PWDN, false); }
        camera_config_t config = {};
        config.ledc_channel = LEDC_CHANNEL_2; config.ledc_timer = LEDC_TIMER_2;
        config.pin_d0 = CAMERA_PIN_D0; config.pin_d1 = CAMERA_PIN_D1; config.pin_d2 = CAMERA_PIN_D2; config.pin_d3 = CAMERA_PIN_D3;
        config.pin_d4 = CAMERA_PIN_D4; config.pin_d5 = CAMERA_PIN_D5; config.pin_d6 = CAMERA_PIN_D6; config.pin_d7 = CAMERA_PIN_D7;
        config.pin_xclk = CAMERA_PIN_XCLK; config.pin_pclk = CAMERA_PIN_PCLK;
        config.pin_vsync = CAMERA_PIN_VSYNC; config.pin_href = CAMERA_PIN_HREF;
        config.pin_sccb_sda = -1; config.pin_sccb_scl = 1;
        config.sccb_i2c_port = I2C_MASTER_PORT;
        config.pin_pwdn = CAMERA_PIN_PWDN; config.pin_reset = CAMERA_PIN_RESET;
        config.xclk_freq_hz = XCLK_FREQ_HZ; config.pixel_format = PIXFORMAT_RGB565;
        config.frame_size = FRAMESIZE_VGA; config.jpeg_quality = 12; config.fb_count = 1;
        config.fb_location = CAMERA_FB_IN_PSRAM; config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
        camera_ = new Esp32Camera(config);
        if (!camera_) { ESP_LOGE(TAG, "Camera initialization failed!"); }
    }
    
    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("Screen"));
        thing_manager.AddThing(iot::CreateThing("Battery"));
    }

public:
    LichuangDevPlusBoard() : DualNetworkBoard(ML307_TX_PIN, ML307_RX_PIN, ML307_RX_BUFFER_SIZE), boot_button_(BOOT_BUTTON_GPIO) {
        // 关键修改：在初始化PMIC前，增加一个短暂的延时
        // 目的是等待 DCDC1 输出的 3.3V 彻底稳定
        vTaskDelay(pdMS_TO_TICKS(50)); // 等待50毫秒

        InitializeI2cBus();
        
        // 关键修改：再次增加一个延时，给I2C总线和PMIC自身稳定留出时间
        vTaskDelay(pdMS_TO_TICKS(50)); // 再等待50毫秒

        // 关键修改：增加I2C设备扫描，用于调试
        // 看看在电池冷启动时，到底能不能扫描到PMIC
        ESP_LOGI(TAG, "================ I2C SCAN START ================");
        esp_err_t ret = i2c_master_probe(i2c_bus_, AXP2101_I2C_ADDR, pdMS_TO_TICKS(100));
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, ">>> PMIC (AXP2101) found at address 0x%02X. Good!", AXP2101_I2C_ADDR);
        } else {
            ESP_LOGE(TAG, "!!! FAILED to find PMIC at 0x%02X. This is the ROOT CAUSE. Error: %s", AXP2101_I2C_ADDR, esp_err_to_name(ret));
            // 如果找不到PMIC，后续操作无意义，可以直接死循环或重启
            while(1) { vTaskDelay(1000); }
        }
        ESP_LOGI(TAG, "================ I2C SCAN END ================");


        InitializePmic(); // 只有在I2C扫描成功后，才执行PMIC初始化
        
        vTaskDelay(pdMS_TO_TICKS(100));
        InitializeIoExpander();
        InitializeSpi();
        InitializeSt7789Display();
        InitializeButtons();
        InitializePowerSaveTimer();
        GetBacklight()->RestoreBrightness(); 
    }

    virtual ~LichuangDevPlusBoard() {
        if (display_) { delete display_; }
        if (pmic_) { delete pmic_; }
        if (aw9523b_) { delete aw9523b_; }
        if (power_save_timer_) { delete power_save_timer_; }
        //if (camera_) { delete camera_; }
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
        charging = pmic_->IsCharging();
        discharging = pmic_->IsDischarging();
        if (discharging != last_discharging) {
            power_save_timer_->SetEnabled(discharging);
            last_discharging = discharging;
        }
        level = pmic_->GetBatteryLevel();
        return true;
    }
    
    virtual bool GetTemperature(float& temp) override {
        return false;
    }

	virtual void SetPowerSaveMode(bool enabled) override {
        if (!enabled) {
            power_save_timer_->WakeUp();
        }
        DualNetworkBoard::SetPowerSaveMode(enabled);
    }
};

DECLARE_BOARD(LichuangDevPlusBoard);