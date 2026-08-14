#include "wifi_board.h"
#include "codecs/es8311_audio_codec.h"
#include "display/oled_display.h"
#include "application.h"
#include "button.h"
#include "led/single_led.h"
#include "mcp_server.h"
#include "settings.h"
#include "config.h"
#include "power_save_timer.h"
#include "press_to_talk_mcp_tool.h"

#include <esp_log.h>
#include <esp_efuse_table.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>

#define TAG "XminiC3Board"

class XminiC3Board : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    Display* display_ = nullptr;
    Button boot_button_;
    PowerSaveTimer* power_save_timer_ = nullptr;
    PressToTalkMcpTool* press_to_talk_tool_ = nullptr;

    void InitializePowerSaveTimer() {
        power_save_timer_ = new PowerSaveTimer(160, 300);
        power_save_timer_->OnEnterSleepMode([this]() {
            GetDisplay()->SetPowerSaveMode(true);
        });
        power_save_timer_->OnExitSleepMode([this]() {
            GetDisplay()->SetPowerSaveMode(false);
        });
        power_save_timer_->SetEnabled(true);
    }

    void InitializeCodecI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
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
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));

        // This board burns ESP_EFUSE_VDD_SPI_AS_GPIO which permanently damages
        // incompatible boards, so we must be certain the ES8311 codec is really
        // present before continuing. i2c_master_probe() only checks for an ACK,
        // which can be a false positive on a wrong board (floating / weakly
        // pulled SDA). Instead, verify the ES8311 chip ID registers.
        if (!IsEs8311Present()) {
            while (true) {
                ESP_LOGE(TAG, "ES8311 not detected, please check if you have installed the correct firmware");
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }
        }
    }

    // Read the ES8311 chip ID registers (0xFD/0xFE should return 0x83/0x11).
    bool IsEs8311Present() {
        i2c_master_dev_handle_t dev = nullptr;
        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = 0x18,
            .scl_speed_hz = 100 * 1000,
        };
        if (i2c_master_bus_add_device(codec_i2c_bus_, &dev_cfg, &dev) != ESP_OK) {
            return false;
        }

        uint8_t reg = 0xFD;
        uint8_t id1 = 0, id2 = 0;
        esp_err_t err1 = i2c_master_transmit_receive(dev, &reg, 1, &id1, 1, 100);
        reg = 0xFE;
        esp_err_t err2 = i2c_master_transmit_receive(dev, &reg, 1, &id2, 1, 100);
        i2c_master_bus_rm_device(dev);

        ESP_LOGI(TAG, "ES8311 chip id: err=(%s,%s) id=0x%02X 0x%02X",
            esp_err_to_name(err1), esp_err_to_name(err2), id1, id2);
        return err1 == ESP_OK && err2 == ESP_OK && id1 == 0x83 && id2 == 0x11;
    }

    void InitializeSsd1306Display() {
        // SSD1306 config
        // IDF 5.5 and 6.0 declare these fields in a different order. Assign
        // them individually so C++ designated-initializer ordering is irrelevant.
        esp_lcd_panel_io_i2c_config_t io_config = {};
        io_config.dev_addr = 0x3C;
        io_config.scl_speed_hz = 400 * 1000;
        io_config.control_phase_bytes = 1;
        io_config.dc_bit_offset = 6;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        io_config.on_color_trans_done = nullptr;
        io_config.user_ctx = nullptr;
        io_config.flags.dc_low_on_data = 0;
        io_config.flags.disable_control_phase = 0;

        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(codec_i2c_bus_, &io_config, &panel_io_));

        ESP_LOGI(TAG, "Install SSD1306 driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.bits_per_pixel = 1;

        esp_lcd_panel_ssd1306_config_t ssd1306_config = {
            .height = static_cast<uint8_t>(DISPLAY_HEIGHT),
        };
        panel_config.vendor_config = &ssd1306_config;

        ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(panel_io_, &panel_config, &panel_));
        ESP_LOGI(TAG, "SSD1306 driver installed");

        // Reset the display
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        if (esp_lcd_panel_init(panel_) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize display");
            display_ = new NoDisplay();
            return;
        }

        // Set the display to on
        ESP_LOGI(TAG, "Turning display on");
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

        display_ = new OledDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            if (!press_to_talk_tool_ || !press_to_talk_tool_->IsPressToTalkEnabled()) {
                app.ToggleChatState();
            }
        });
        boot_button_.OnPressDown([this]() {
            if (power_save_timer_) {
                power_save_timer_->WakeUp();
            }
            if (press_to_talk_tool_ && press_to_talk_tool_->IsPressToTalkEnabled()) {
                Application::GetInstance().StartListening();
            }
        });
        boot_button_.OnPressUp([this]() {
            if (press_to_talk_tool_ && press_to_talk_tool_->IsPressToTalkEnabled()) {
                Application::GetInstance().StopListening();
            }
        });
    }

    void InitializeTools() {
        press_to_talk_tool_ = new PressToTalkMcpTool();
        press_to_talk_tool_->Initialize();
    }

public:
    XminiC3Board() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeCodecI2c();
        InitializeSsd1306Display();
        InitializeButtons();
        InitializePowerSaveTimer();
        InitializeTools();

        // 避免使用错误的固件，把 EFUSE 操作放在最后
        // 把 ESP32C3 的 VDD SPI 引脚作为普通 GPIO 口使用
        esp_efuse_write_field_bit(ESP_EFUSE_VDD_SPI_AS_GPIO);
    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(codec_i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }

    virtual void SetPowerSaveLevel(PowerSaveLevel level) override {
        if (level != PowerSaveLevel::LOW_POWER) {
            power_save_timer_->WakeUp();
        }
        WifiBoard::SetPowerSaveLevel(level);
    }
};

DECLARE_BOARD(XminiC3Board);
