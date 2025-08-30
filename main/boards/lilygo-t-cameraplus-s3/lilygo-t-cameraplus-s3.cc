#include "wifi_board.h"
#include "tcamerapluss3_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "power_save_timer.h"
#include "i2c_device.h"
#include "sy6970.h"
#include "pin_config.h"
#include "esp32_camera.h"
#include "ir_filter_controller.h"

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <wifi_station.h>

#define TAG "LilygoTCameraPlusS3Board"

LV_FONT_DECLARE(font_puhui_16_4);
LV_FONT_DECLARE(font_awesome_16_4);

class Cst816x : public I2cDevice {
public:
    struct TouchPoint_t {
        int num = 0;
        int x = -1;
        int y = -1;
    };

    Cst816x(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
        uint8_t chip_id = ReadReg(0xA7);
        ESP_LOGI(TAG, "Get chip ID: 0x%02X", chip_id);
        read_buffer_ = new uint8_t[6];
    }

    ~Cst816x() {
        delete[] read_buffer_;
    }

    void UpdateTouchPoint() {
        ReadRegs(0x02, read_buffer_, 6);
        tp_.num = read_buffer_[0] & 0x0F;
        tp_.x = ((read_buffer_[1] & 0x0F) << 8) | read_buffer_[2];
        tp_.y = ((read_buffer_[3] & 0x0F) << 8) | read_buffer_[4];
    }

    const TouchPoint_t &GetTouchPoint() {
        return tp_;
    }

private:
    uint8_t *read_buffer_ = nullptr;
    TouchPoint_t tp_;
};

class Pmic : public Sy6970 {
public:

    Pmic(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : Sy6970(i2c_bus, addr) {
        uint8_t chip_id = ReadReg(0x14);
        ESP_LOGI(TAG, "Get sy6970 chip ID: 0x%02X", (chip_id & 0B00111000));

        WriteReg(0x00, 0B00001000); // Disable ILIM pin
        WriteReg(0x02, 0B11011101); // Enable ADC measurement function
        WriteReg(0x07, 0B10001101); // Disable watchdog timer feeding function
    }
};

class LilygoTCameraPlusS3Board : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    Cst816x *cst816d_;
    Pmic* pmic_;
    LcdDisplay *display_;
    Button boot_button_;
    Button key1_button_;
    PowerSaveTimer* power_save_timer_;
    Esp32Camera* camera_;

    void InitializePowerSaveTimer() {
        power_save_timer_ = new PowerSaveTimer(-1, 60, -1);
        power_save_timer_->OnEnterSleepMode([this]() {
            GetDisplay()->SetPowerSaveMode(true);
            GetBacklight()->SetBrightness(10);
        });
        power_save_timer_->OnExitSleepMode([this]() {
            GetDisplay()->SetPowerSaveMode(false);
            GetBacklight()->RestoreBrightness();
        });
        power_save_timer_->OnShutdownRequest([this]() {
            pmic_->PowerOff();
        });
        power_save_timer_->SetEnabled(true);
    }

    void InitI2c(){
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_config = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = TOUCH_I2C_SDA_PIN,
            .scl_io_num = TOUCH_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            }
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &i2c_bus_));
    }

    void I2cDetect() {
        uint8_t address;
        printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\r\n");
        for (int i = 0; i < 128; i += 16) {
            printf("%02x: ", i);
            for (int j = 0; j < 16; j++) {
                fflush(stdout);
                address = i + j;
                esp_err_t ret = i2c_master_probe(i2c_bus_, address, pdMS_TO_TICKS(200));
                if (ret == ESP_OK) {
                    printf("%02x ", address);
                } else if (ret == ESP_ERR_TIMEOUT) {
                    printf("UU ");
                } else {
                    printf("-- ");
                }
            }
            printf("\r\n");
        }
    }

    static void touchpad_daemon(void *param) {
        vTaskDelay(pdMS_TO_TICKS(2000));
        auto &board = (LilygoTCameraPlusS3Board&)Board::GetInstance();
        auto touchpad = board.GetTouchpad();
        bool was_touched = false;
        while (1) {
            touchpad->UpdateTouchPoint();
            if (touchpad->GetTouchPoint().num > 0){
                // On press
                if (!was_touched) {
                    was_touched = true;
                    Application::GetInstance().ToggleChatState();
                }
            }
            // On release
            else if (was_touched) {
                was_touched = false;
            }
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        vTaskDelete(NULL);
    }

    void InitCst816d() {
        ESP_LOGI(TAG, "Init CST816x");
        cst816d_ = new Cst816x(i2c_bus_, CST816_ADDRESS);
        xTaskCreate(touchpad_daemon, "tp", 2048, NULL, 5, NULL);
    }

    void InitSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_SCLK;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitSy6970() {
        ESP_LOGI(TAG, "Init Sy6970");
        pmic_ = new Pmic(i2c_bus_, SY6970_ADDRESS);
    }

    void InitializeSt7789Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = LCD_CS;
        io_config.dc_gpio_num = LCD_DC;
        io_config.spi_mode = 0;
        io_config.pclk_hz = 60 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片ST7789
        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = LCD_RST;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));

        display_ = new SpiLcdDisplay(panel_io, panel,
                                     DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                     {
                                         .text_font = &font_puhui_16_4,
                                         .icon_font = &font_awesome_16_4,
                                         .emoji_font = font_emoji_32_init(),
                                     });
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            power_save_timer_->WakeUp();
            app.ToggleChatState();
        });
        key1_button_.OnClick([this]() {
            if (camera_) {
                camera_->Capture();
            }
        });
    }

    void InitializeCamera() {
        camera_config_t config = {};
        config.ledc_channel = LEDC_CHANNEL_2;   // LEDC通道选择  用于生成XCLK时钟 但是S3不用
        config.ledc_timer = LEDC_TIMER_2;       // LEDC timer选择  用于生成XCLK时钟 但是S3不用
        config.pin_d0 = Y2_GPIO_NUM;
        config.pin_d1 = Y3_GPIO_NUM;
        config.pin_d2 = Y4_GPIO_NUM;
        config.pin_d3 = Y5_GPIO_NUM;
        config.pin_d4 = Y6_GPIO_NUM;
        config.pin_d5 = Y7_GPIO_NUM;
        config.pin_d6 = Y8_GPIO_NUM;
        config.pin_d7 = Y9_GPIO_NUM;
        config.pin_xclk = XCLK_GPIO_NUM;
        config.pin_pclk = PCLK_GPIO_NUM;
        config.pin_vsync = VSYNC_GPIO_NUM;
        config.pin_href = HREF_GPIO_NUM;
#ifdef CONFIG_BOARD_TYPE_LILYGO_T_CAMERAPLUS_S3_V1_0_V1_1
        config.pin_sccb_sda = -1;   // 这里如果写-1 表示使用已经初始化的I2C接口
        config.pin_sccb_scl = SIOC_GPIO_NUM;
        config.sccb_i2c_port = 0;   //  这里如果写0 默认使用I2C0
#elif defined CONFIG_BOARD_TYPE_LILYGO_T_CAMERAPLUS_S3_V1_2
        config.pin_sccb_sda = SIOD_GPIO_NUM;  
        config.pin_sccb_scl = SIOC_GPIO_NUM;
        config.sccb_i2c_port = 1; 
#endif
        config.pin_pwdn = PWDN_GPIO_NUM;
        config.pin_reset = RESET_GPIO_NUM;
        config.xclk_freq_hz = XCLK_FREQ_HZ;
        config.pixel_format = PIXFORMAT_RGB565;
        config.frame_size = FRAMESIZE_240X240;
        config.jpeg_quality = 12;
        config.fb_count = 1;
        config.fb_location = CAMERA_FB_IN_PSRAM;
        config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

        camera_ = new Esp32Camera(config);
        camera_->SetVFlip(1);
        camera_->SetHMirror(1);
    }

    void InitializeTools() {
        static IrFilterController irFilter(AP1511B_GPIO);
    }

public:
    LilygoTCameraPlusS3Board() : boot_button_(BOOT_BUTTON_GPIO), key1_button_(KEY1_BUTTON_GPIO) {
        InitializePowerSaveTimer();
        InitI2c();
        InitSy6970();
        InitCst816d();
        I2cDetect();
        InitSpi();
        InitializeSt7789Display();
        InitializeButtons();
        InitializeCamera();
        InitializeTools();
        GetBacklight()->RestoreBrightness();
    }

    virtual AudioCodec *GetAudioCodec() override {
        static Tcamerapluss3AudioCodec audio_codec(
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_MIC_I2S_GPIO_BCLK,
            AUDIO_MIC_I2S_GPIO_WS,
            AUDIO_MIC_I2S_GPIO_DATA,
            AUDIO_SPKR_I2S_GPIO_BCLK,
            AUDIO_SPKR_I2S_GPIO_LRCLK,
            AUDIO_SPKR_I2S_GPIO_DATA,
            AUDIO_INPUT_REFERENCE);
        return &audio_codec;
    }

    virtual Display *GetDisplay() override{
        return display_;
    }

    virtual bool GetBatteryLevel(int &level, bool& charging, bool& discharging) override {
        static bool last_discharging = false;
        charging = pmic_->IsCharging();
        bool is_power_good = pmic_->IsPowerGood();
        discharging = !charging && is_power_good;
        if (discharging != last_discharging) {
            power_save_timer_->SetEnabled(discharging);
            last_discharging = discharging;
        }

        level = pmic_->GetBatteryLevel();
        return true;
    }

    virtual void SetPowerSaveMode(bool enabled) override {
        if (!enabled) {
            power_save_timer_->WakeUp();
        }
        WifiBoard::SetPowerSaveMode(enabled);
    }
    
    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    Cst816x *GetTouchpad() {
        return cst816d_;
    }

    virtual Camera* GetCamera() override {
        return camera_;
    }
};

DECLARE_BOARD(LilygoTCameraPlusS3Board);
