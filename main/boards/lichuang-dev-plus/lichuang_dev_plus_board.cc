#include "dual_network_board.h" 
#include "audio_codecs/box_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
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
         // --- 核心修改：配置按键开关机行为 ---
        // REG 0x25: 配置N_OE引脚功能
        // bit 7=1: 使能长按关机; bit 2=1: 使能内部上拉; bit 1-0=11: VBUS或按键都能开机
        uint8_t target_noe_setup = (1 << 7) | (1 << 2) | 0b11; 
        WriteReg(0x25, target_noe_setup);
        vTaskDelay(pdMS_TO_TICKS(5)); // 短暂延时确保写入
        if (ReadReg(0x25) != target_noe_setup) {
            ESP_LOGE(TAG, "Failed to configure REG 0x25 for power button!");
        }

        // REG 0x26: 配置开关机时间
        // 高4位是关机时间: 0010 = 2秒
        // 低3位是开机时间: 001 = 128ms (点按)
        uint8_t target_noe_time = (0b0010 << 4) | 0b001; 
        WriteReg(0x26, target_noe_time);
        vTaskDelay(pdMS_TO_TICKS(5)); // 短暂延时确保写入
        if (ReadReg(0x26) != target_noe_time) {
            ESP_LOGE(TAG, "Failed to configure REG 0x26 for power timing!");
        }
        // 配置充电
        WriteReg(0x33, 0b11000100); // 建议使能充电 (bit 7=1), 如果不需要充电再设为 0b01000100
        
        // 配置VBUS
        WriteReg(0x30, 0b01100001); 
        
        // 配置TS
        WriteReg(0x34, 0x00);
        
        // 设置电压
        WriteReg(0x92, 0x1C); // ALDO1 -> 3.3V (给 AU_3V3)
        WriteReg(0x93, 0x17); // ALDO2 -> 2.8V (给 屏幕背光, 2.8V作为默认值)

        uint8_t ldo_onoff_ctrl0 = ReadReg(0x90); 
        ldo_onoff_ctrl0 |= (1 << 0) | (1 << 1); // 只使能 ALDO1 和 ALDO2
        ldo_onoff_ctrl0 &= ~(1 << 4);           // 明确地禁用 BLDO1，以防万一
        WriteReg(0x90, ldo_onoff_ctrl0);

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
class Aw9523b : public I2cDevice {public:
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
    /* void Init() {
        ESP_LOGI(TAG, "Initializing AW9523B (Final Pin-Corrected Version)...");
        // --- 基础初始化 ---
        WriteReg(0x7F, 0x00); // 切换到寄存器 Bank 0
        vTaskDelay(pdMS_TO_TICKS(10));
        WriteReg(0x11, 0x01); // GCR: 设置为推挽输出模式
        vTaskDelay(pdMS_TO_TICKS(10));

        // --- 步骤 1: 解决开机爆闪 ---
        // 在配置引脚模式之前，就将所有LED亮度寄存器预设为0。
        WriteReg(0x27, 0); // P0_3 (R) 亮度寄存器
        WriteReg(0x28, 0); // P0_4 (G) 亮度寄存器
        WriteReg(0x29, 0); // P0_5 (B) 亮度寄存器

        // --- 步骤 2: 精确配置每个引脚的模式 (0=LED, 1=GPIO) ---
        // P0 口模式寄存器 (0x12)
        uint8_t p0_mode_cfg = 0;         // 先假设所有P0口都是LED模式
        p0_mode_cfg |= (1 << 0);         // P0_0 (PA_EN) 设为 GPIO
        p0_mode_cfg |= (1 << 2);         // P0_2 (DVP_PWDN) 设为 GPIO
        p0_mode_cfg |= (1 << 6);         // P0_6 (LCD_CS) 设为 GPIO
        // P0_3, P0_4, P0_5 保持为0 (LED模式)
        WriteReg(0x12, p0_mode_cfg);

        // P1 口模式寄存器 (0x13)
        uint8_t p1_mode_cfg = 0;         // 先假设所有P1口都是LED模式
        p1_mode_cfg |= (1 << 1);         // P1_1 (PJ_SET) 设为 GPIO
        WriteReg(0x13, p1_mode_cfg);

        // --- 步骤 3: 为GPIO模式的引脚设置方向 (0=Output, 1=Input) ---
        // P0 口方向寄存器 (0x04)
        uint8_t p0_dir_cfg = 0xFF;       // 默认所有P0口为输入
        p0_dir_cfg &= ~((1 << 0) | (1 << 2) | (1 << 6)); // 将 PA_EN, DVP_PWDN, LCD_CS 设为输出
        WriteReg(0x04, p0_dir_cfg);

        // P1 口方向寄存器 (0x05)
        uint8_t p1_dir_cfg = 0xFF;       // 默认所有P1口为输入
        p1_dir_cfg &= ~((1 << 1));       // 将 PJ_SET 设为输出
        WriteReg(0x05, p1_dir_cfg);

        // --- 步骤 4: 为GPIO输出引脚设置默认电平 (拉高) ---
        // P0 口输出电平寄存器 (0x02)
        uint8_t p0_level_cfg = ReadReg(0x02);
        p0_level_cfg |= (1 << 0) | (1 << 2) | (1 << 6); // 将 PA_EN, DVP_PWDN, LCD_CS 拉高
        WriteReg(0x02, p0_level_cfg);

        // P1 口输出电平寄存器 (0x03)
        uint8_t p1_level_cfg = ReadReg(0x03);
        p1_level_cfg |= (1 << 1);        // 将 PJ_SET 拉高
        WriteReg(0x03, p1_level_cfg);
        
        ESP_LOGI(TAG, "AW9523B initialization complete. All required GPIOs set to HIGH.");
    } */
    // --- 最终的RGB亮度控制函数 ---
    void SetRgb(uint8_t r, uint8_t g, uint8_t b) {
        // 根据AW9523B数据手册，各引脚的亮度寄存器地址如下：
        // P0_3 (R) -> 0x27
        // P0_4 (G) -> 0x28
        // P0_5 (B) -> 0x29
        WriteReg(0x27, r); // 软件 Red -> 硬件 Red (P0_3)
        WriteReg(0x28, b); // 软件 Blue -> 硬件 Blue (P0_4) 
        WriteReg(0x29, g); // 软件 Green -> 硬件 Green (P0_5)
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
    bool led_on_;

 

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

    void InitializePmic() { 
        pmic_ = new Pmic(i2c_bus_, 0x34); 
    }

    void InitializeIoExpander() {
        aw9523b_ = new Aw9523b(i2c_bus_, 0x58);
        aw9523b_->Init();
    }
    void SetLedColor(uint8_t r, uint8_t g, uint8_t b) {
        if (aw9523b_) { aw9523b_->SetRgb(r, g, b); }
    }

    void InitializeTools() {
        auto& mcp_server = McpServer::GetInstance();
        mcp_server.AddTool("self.light.get_power", "获取灯是否打开", PropertyList(), [this](const PropertyList& properties) -> ReturnValue { return led_on_; });
        mcp_server.AddTool("self.light.turn_on", "打开灯", PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
            SetLedColor(255, 255, 255); led_on_ = true; return true;
        });
        mcp_server.AddTool("self.light.turn_off", "关闭灯", PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
            SetLedColor(0, 0, 0); led_on_ = false; return true;
        });
        mcp_server.AddTool("self.light.set_rgb", "设置RGB颜色", PropertyList({
            Property("r", kPropertyTypeInteger, 0, 255),
            Property("g", kPropertyTypeInteger, 0, 255),
            Property("b", kPropertyTypeInteger, 0, 255)
        }), [this](const PropertyList& properties) -> ReturnValue {
            int r = properties["r"].value<int>();
            int g = properties["g"].value<int>();
            int b = properties["b"].value<int>();
            led_on_ = (r > 0 || g > 0 || b > 0);
            SetLedColor(r, g, b);
            return true;
        });
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
        power_save_timer_ = new PowerSaveTimer(240, 60, 300);
        power_save_timer_->OnEnterSleepMode([this]() { 
            ESP_LOGI(TAG, "Entering sleep mode"); 
            if (GetBacklight()) GetBacklight()->SetBrightness(10); 
        });
        power_save_timer_->OnExitSleepMode([this]() { 
            ESP_LOGI(TAG, "Exiting sleep mode"); 
            if (GetBacklight()) GetBacklight()->RestoreBrightness(); 
        });
        power_save_timer_->OnShutdownRequest([this]() { 
            ESP_LOGI(TAG, "Shutdown requested"); 
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
        thing_manager.AddThing(iot::CreateThing("lamp"));
    }

public:
    LichuangDevPlusBoard() : DualNetworkBoard(ML307_TX_PIN, ML307_RX_PIN, ML307_RX_BUFFER_SIZE), boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2cBus();
        vTaskDelay(pdMS_TO_TICKS(100)); // 增加100毫秒的延时
        /*  // --- 步骤 2: 延时一小会，等待所有I2C设备上电稳定 ---
        vTaskDelay(pdMS_TO_TICKS(200));

        // --- 步骤 3: 执行I2C扫描，并打印结果 ---
        I2cScan();

        // --- 步骤 4: 继续执行正常的初始化流程 ---
        // (注意：即使扫描不到设备，程序也会继续，
        //  这样我们可以看到后续的初始化错误，进行对比)
        ESP_LOGI(TAG, "Continuing with normal board initialization..."); */

        InitializePmic(); 
        InitializeIoExpander();
        ESP_LOGI(TAG, "Restoring backlight brightness before display init...");
        GetBacklight()->RestoreBrightness();
        InitializeSpi();
        InitializeSt7789Display();
        InitializeButtons();
        InitializePowerSaveTimer();
        InitializeTouch();
        InitializeCamera();
        InitializeTools();
        InitializeIot();
        SetLedColor(0, 0, 0); // 初始状态关闭灯
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