#include "boards/wifi_board.h"
#include "audio_codecs/box_audio_codec.h"
#include "display/st7789_display.h"
#include "application.h"
#include "button.h"
#include "led.h"
#include "config.h"

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#define TAG "LichuangDevBoard"

class LichuangDevBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    i2c_master_dev_handle_t pca9557_handle_;
    Button boot_button_;

    void InitializeI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_1,
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

    void Pca9557ReadRegister(uint8_t addr, uint8_t* data) {
        uint8_t tmp[1] = {addr};
        ESP_ERROR_CHECK(i2c_master_transmit_receive(pca9557_handle_, tmp, 1, data, 1, 100));
    }

    void Pca9557WriteRegister(uint8_t addr, uint8_t data) {
        uint8_t tmp[2] = {addr, data};
        ESP_ERROR_CHECK(i2c_master_transmit(pca9557_handle_, tmp, 2, 100));
    }

    void Pca9557SetOutputState(uint8_t bit, uint8_t level) {
        uint8_t data;
        Pca9557ReadRegister(0x01, &data);
        data = (data & ~(1 << bit)) | (level << bit);
        Pca9557WriteRegister(0x01, data);
    }

    void InitializePca9557() {
        i2c_device_config_t pca9557_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = 0x19,
            .scl_speed_hz = 100000,
            .scl_wait_us = 0,
            .flags = {
                .disable_ack_check = 0,
            },
        };
        ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_, &pca9557_cfg, &pca9557_handle_));
        assert(pca9557_handle_ != NULL);
        Pca9557WriteRegister(0x01, 0x03);
        Pca9557WriteRegister(0x03, 0xf8);
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

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            Application::GetInstance().ToggleChatState();
        });
    }

public:
    LichuangDevBoard() : boot_button_(BOOT_BUTTON_GPIO) {
    }

    virtual void Initialize() override {
        ESP_LOGI(TAG, "Initializing LichuangDevBoard");
        InitializeI2c();
        InitializePca9557();
        InitializeSpi();
        InitializeButtons();
        WifiBoard::Initialize();
    }

    virtual Led* GetBuiltinLed() override {
        static Led led(GPIO_NUM_NC);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static BoxAudioCodec* audio_codec = nullptr;
        if (audio_codec == nullptr) {
            audio_codec = new BoxAudioCodec(i2c_bus_, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
                GPIO_NUM_NC, AUDIO_CODEC_ES8311_ADDR, AUDIO_CODEC_ES7210_ADDR, AUDIO_INPUT_REFERENCE);
            audio_codec->SetOutputVolume(AUDIO_DEFAULT_OUTPUT_VOLUME);
        }
        return audio_codec;
    }

    virtual Display* GetDisplay() override {
        static St7789Display* display = nullptr;
        if (display == nullptr) {
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
            Pca9557SetOutputState(0, 0);

            esp_lcd_panel_init(panel);
            esp_lcd_panel_invert_color(panel, true);
            esp_lcd_panel_swap_xy(panel, true);
            esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
            display = new St7789Display(panel_io, panel, GPIO_NUM_42, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_SWAP_XY, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, BACKLIGHT_INVERT);
        }
        return display;
    }
};

DECLARE_BOARD(LichuangDevBoard);
