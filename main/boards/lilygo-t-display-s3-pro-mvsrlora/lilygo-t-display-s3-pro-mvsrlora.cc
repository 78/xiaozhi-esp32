#include "wifi_board.h"
#include "tdisplays3promvsrlora_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "power_save_timer.h"
#include "i2c_device.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <wifi_station.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include "esp_lcd_st7796.h"

#define TAG "LilygoTDisplays3ProMVSRLoraBoard"

LV_FONT_DECLARE(font_puhui_16_4);
LV_FONT_DECLARE(font_awesome_16_4);

class Cst2xxse : public I2cDevice {
public:
    struct TouchPoint_t {
        int num = 0;
        int x = -1;
        int y = -1;
    };

    Cst2xxse(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
        uint8_t chip_id = ReadReg(0x06);
        ESP_LOGI(TAG, "Get cst2xxse chip ID: 0x%02X", chip_id);
        read_buffer_ = new uint8_t[6];
    }

    ~Cst2xxse() {
        delete[] read_buffer_;
    }

    void UpdateTouchPoint() {
        ReadRegs(0x00, read_buffer_, 6);
        tp_.num = read_buffer_[5] & 0x0F;
        tp_.x = (static_cast<int>(read_buffer_[1]) << 4) | (read_buffer_[3] & 0xF0);
        tp_.y = (static_cast<int>(read_buffer_[2]) << 4) | (read_buffer_[3] & 0x0F);
        // ESP_LOGI(TAG, "Touch num: %d x: %d y: %d", tp_.num,tp_.x,tp_.y);
    }

    const TouchPoint_t &GetTouchPoint() {
        return tp_;
    }

private:
    uint8_t *read_buffer_ = nullptr;
    TouchPoint_t tp_;
};

class Sy6970 : public I2cDevice {
public:

    Sy6970(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
        uint8_t chip_id = ReadReg(0x14);
        ESP_LOGI(TAG, "Get sy6970 chip ID: 0x%02X", (chip_id & 0B00111000));

        WriteReg(0x00,0B00001000); // Disable ILIM pin
        WriteReg(0x02,0B11011101); // Enable ADC measurement function
        WriteReg(0x07,0B10001101); // Disable watchdog timer feeding function

        #ifdef CONFIG_BOARD_TYPE_LILYGO_T_DISPLAY_S3_PRO_MVSRLORA_NO_BATTERY
        WriteReg(0x09,0B01100100); // Disable BATFET when battery is not needed
        #endif
    }
};

class LilygoTDisplays3ProMVSRLoraBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    Cst2xxse *cst226se_;
    Sy6970 *sy6970_;
    LcdDisplay *display_;
    Button boot_button_;
    PowerSaveTimer* power_save_timer_;

    void InitializePowerSaveTimer() {
        power_save_timer_ = new PowerSaveTimer(-1, 60, 300);
        power_save_timer_->OnEnterSleepMode([this]() {
            GetDisplay()->SetPowerSaveMode(true);
            GetBacklight()->SetBrightness(10);
        });
        power_save_timer_->OnExitSleepMode([this]() {
            GetDisplay()->SetPowerSaveMode(false);
            GetBacklight()->RestoreBrightness();
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
        auto &board = (LilygoTDisplays3ProMVSRLoraBoard&)Board::GetInstance();
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

    void InitCst226se() {
        ESP_LOGI(TAG, "Init Cst2xxse");
        cst226se_ = new Cst2xxse(i2c_bus_, 0x5A);
        xTaskCreate(touchpad_daemon, "tp", 4096, NULL, 5, NULL);
    }

    void InitSy6970() {
        ESP_LOGI(TAG, "Init Sy6970");
        sy6970_ = new Sy6970(i2c_bus_, 0x6A);
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

    void InitSt7796Display() {
        ESP_LOGI(TAG, "Init St7796");

        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS;
        io_config.dc_gpio_num = DISPLAY_DC;
        io_config.spi_mode = 0;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7796(panel_io, &panel_config, &panel));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, false));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, true, false));
        ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel, 49, 0));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

        display_ = new SpiLcdDisplay(panel_io, panel,
                                  DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X,
                                  DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                  {
                                      .text_font = &font_puhui_16_4,
                                      .icon_font = &font_awesome_16_4,
                                      .emoji_font = font_emoji_32_init(),
                                  });

        gpio_config_t config;
        config.pin_bit_mask = BIT64(DISPLAY_BL);
        config.mode = GPIO_MODE_OUTPUT;
        config.pull_up_en = GPIO_PULLUP_DISABLE;
        config.pull_down_en = GPIO_PULLDOWN_ENABLE;
        config.intr_type = GPIO_INTR_DISABLE;
#if SOC_GPIO_SUPPORT_PIN_HYS_FILTER
        config.hys_ctrl_mode = GPIO_HYS_SOFT_ENABLE;
#endif
        gpio_config(&config);
        gpio_set_level(DISPLAY_BL, 0);
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
    }

public:
    LilygoTDisplays3ProMVSRLoraBoard() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializePowerSaveTimer();
        InitI2c();
        I2cDetect();
        InitCst226se();
        InitSy6970();
        InitSpi();
        InitSt7796Display();
        InitializeButtons();
        GetBacklight()->RestoreBrightness();
    }

    virtual AudioCodec *GetAudioCodec() override {
        static Tdisplays3promvsrloraAudioCodec audio_codec(
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

    Cst2xxse *GetTouchpad() {
        return cst226se_;
    }
};

DECLARE_BOARD(LilygoTDisplays3ProMVSRLoraBoard);
