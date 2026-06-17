#include "wifi_board.h"
#include "display/lcd_display.h"
#include "esp_lcd_co5300.h"
#include "codecs/es8311_audio_codec.h"
#include "application.h"
#include "button.h"
#include "M5IOE1.h"
#include "M5PM1.h"
#include "config.h"
#include "assets/lang_config.h"
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/spi_master.h>
#include <wifi_manager.h>

#define TAG "M5StackStopwatch"

// CO5300 AMOLED: brightness via 0x51 in init only, no separate backlight control
static const co5300_lcd_init_cmd_t vendor_specific_init[] = {
    // {cmd, { data }, data_size, delay_ms}
    {0xFE, (uint8_t []){0x00}, 0, 0},
    {0xC4, (uint8_t []){0x80}, 1, 0},
    {0x3A, (uint8_t []){0x55}, 0, 10}, // RGB565
    {0x35, (uint8_t []){0x00}, 0, 10},
    {0x53, (uint8_t []){0x20}, 1, 10},
    {0x51, (uint8_t []){0xFF}, 1, 10},
    {0x63, (uint8_t []){0xFF}, 1, 10},
    {0x2A, (uint8_t []){0x00, 0x00, 0x01, 0xD1}, 4, 0}, // Column address: 0-465
    {0x2B, (uint8_t []){0x00, 0x00, 0x01, 0xD1}, 4, 0}, // Row address: 0-465
    {0x11, (uint8_t []){0x00}, 0, 120}, // Exit sleep
    {0x29, (uint8_t []){0x00}, 0, 20},  // Display on
};

class RoundLcdDisplay : public SpiLcdDisplay {
public:
    static void rounder_event_cb(lv_event_t* e) {
        lv_area_t* area = static_cast<lv_area_t*>(lv_event_get_param(e));
        area->x1 = (area->x1 >> 1) << 1;
        area->y1 = (area->y1 >> 1) << 1;
        area->x2 = ((area->x2 >> 1) << 1) + 1;
        area->y2 = ((area->y2 >> 1) << 1) + 1;
    }

    RoundLcdDisplay(esp_lcd_panel_io_handle_t io_handle,
                    esp_lcd_panel_handle_t panel_handle,
                    int width,
                    int height,
                    int offset_x,
                    int offset_y,
                    bool mirror_x,
                    bool mirror_y,
                    bool swap_xy)
        : SpiLcdDisplay(io_handle, panel_handle, width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy) {}

    void SetupUI() override {
        SpiLcdDisplay::SetupUI();
        DisplayLockGuard lock(this);

        // Horizontal inset so labels stay inside the round mask
        lv_obj_set_style_pad_left(status_bar_, LV_HOR_RES * 0.2, 0);
        lv_obj_set_style_pad_right(status_bar_, LV_HOR_RES * 0.2, 0);

        // Status bar: upper half, below top icon bar (not at screen bottom)
        lv_obj_align(status_bar_, LV_ALIGN_TOP_MID, 0, DISPLAY_STATUS_BAR_TOP_OFF);
        lv_obj_set_width(status_label_, LV_HOR_RES * 0.6);
        lv_obj_set_width(notification_label_, LV_HOR_RES * 0.6);
        lv_obj_align(status_label_, LV_ALIGN_CENTER, 0, 0);
        lv_obj_align(notification_label_, LV_ALIGN_CENTER, 0, 0);

        // Chat/subtitle text: a bit higher, away from bottom arc
        if (bottom_bar_ != nullptr) {
            lv_obj_align(bottom_bar_, LV_ALIGN_BOTTOM_MID, 0, -DISPLAY_CHAT_BAR_BOTTOM_OFF);
            lv_obj_set_width(chat_message_label_, LV_HOR_RES * 0.75);
        }

        // emoji_box_: keep default LV_ALIGN_CENTER from SpiLcdDisplay::SetupUI()

        // Top icons: move the top bar down inside the round display safe area
        if (top_bar_ != nullptr) {
            lv_obj_align(top_bar_, LV_ALIGN_TOP_MID, 0, DISPLAY_ROUND_EDGE_INSET / 2);
            lv_obj_set_style_pad_top(top_bar_, DISPLAY_ROUND_EDGE_INSET / 4, 0);
        }

        lv_display_add_event_cb(display_, rounder_event_cb, LV_EVENT_INVALIDATE_AREA, NULL);
    }
};

class M5StackStopwatchBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    M5PM1 pmic_;
    M5IOE1 ioe_;
    Button button1_;
    Button button2_;
    RoundLcdDisplay* display_;

    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {.enable_internal_pullup = 1},
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));

        if (ioe_.begin(i2c_bus_, M5IOE1_I2C_ADDR, M5IOE1_I2C_FREQ_100K, M5IOE1_INT_MODE_POLLING) != M5IOE1_OK) {
            ESP_LOGE(TAG, "M5IOE1 begin failed");
            return;
        }

        if (pmic_.begin(i2c_bus_, M5PM1_DEFAULT_ADDR, M5PM1_I2C_FREQ_100K) != M5PM1_OK) {
            ESP_LOGE(TAG, "M5PM1 begin failed");
            return;
        }

        pmic_.setChargeEnable(true);
        pmic_.setBoostEnable(true);
        pmic_.pinMode(PMIC_PIN_CHARGE_STATE, INPUT);
        pmic_.pinMode(PMIC_PIN_CHARGE_PROG, OUTPUT);
        pmic_.digitalWrite(PMIC_PIN_CHARGE_PROG, LOW); // Set charge current to 425mA

        ioe_.pinMode(IOE_PIN_LCD_POWER, OUTPUT);
        ioe_.setDriveMode(IOE_PIN_LCD_POWER, M5IOE1_DRIVE_PUSHPULL);
        ioe_.digitalWrite(IOE_PIN_LCD_POWER, HIGH);

        ioe_.pinMode(IOE_PIN_LCD_RST, OUTPUT);
        ioe_.setDriveMode(IOE_PIN_LCD_RST, M5IOE1_DRIVE_PUSHPULL);
        ioe_.digitalWrite(IOE_PIN_LCD_RST, HIGH);

        ioe_.pinMode(IOE_PIN_CODEC_POWER, OUTPUT);
        ioe_.setDriveMode(IOE_PIN_CODEC_POWER, M5IOE1_DRIVE_PUSHPULL);
        ioe_.digitalWrite(IOE_PIN_CODEC_POWER, HIGH);

        ioe_.pinMode(IOE_PIN_PA_EN, OUTPUT);
        ioe_.setDriveMode(IOE_PIN_PA_EN, M5IOE1_DRIVE_PUSHPULL);
        ioe_.digitalWrite(IOE_PIN_PA_EN, HIGH);

        ioe_.pinMode(IOE_PIN_MOTOR, OUTPUT);
        ioe_.setDriveMode(IOE_PIN_MOTOR, M5IOE1_DRIVE_PUSHPULL);
        ioe_.digitalWrite(IOE_PIN_MOTOR, LOW);

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.sclk_io_num = DISPLAY_QSPI_SCK;
        buscfg.data0_io_num = DISPLAY_QSPI_D0;
        buscfg.data1_io_num = DISPLAY_QSPI_D1;
        buscfg.data2_io_num = DISPLAY_QSPI_D2;
        buscfg.data3_io_num = DISPLAY_QSPI_D3;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        buscfg.flags = SPICOMMON_BUSFLAG_QUAD;
        ESP_ERROR_CHECK(spi_bus_initialize(DISPLAY_QSPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeDisplay() {
        ioe_.digitalWrite(IOE_PIN_LCD_RST, LOW);
        vTaskDelay(pdMS_TO_TICKS(10));
        ioe_.digitalWrite(IOE_PIN_LCD_RST, HIGH);
        vTaskDelay(pdMS_TO_TICKS(120));

        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        esp_lcd_panel_io_spi_config_t io_config = CO5300_PANEL_IO_QSPI_CONFIG(
            DISPLAY_QSPI_CS, nullptr, nullptr);
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(DISPLAY_QSPI_HOST, &io_config, &panel_io));

        co5300_vendor_config_t vendor_config = {
            .init_cmds = &vendor_specific_init[0],
            .init_cmds_size = sizeof(vendor_specific_init) / sizeof(co5300_lcd_init_cmd_t),
            .flags = {.use_qspi_interface = 1},
        };

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        panel_config.vendor_config = &vendor_config;
        ESP_ERROR_CHECK(esp_lcd_new_panel_co5300(panel_io, &panel_config, &panel));

        esp_lcd_panel_set_gap(panel, 0x06, 0);
        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, false);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        esp_lcd_panel_disp_on_off(panel, true);

        display_ = new RoundLcdDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                         DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
                                         DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeButtons() {
        // Button1: wake / toggle conversation
        button1_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiManager::GetInstance().IsConnected()) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });

        // Button2: volume 0 -> 10 -> ... -> 100 -> 0
        button2_.OnClick([this]() {
            auto* codec = GetAudioCodec();
            int volume = codec->output_volume() + 10;
            if (volume > 100) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(std::string(Lang::Strings::VOLUME) + ":" + std::to_string(volume) + "%");
        });
    }

public:
    M5StackStopwatchBoard()
        : i2c_bus_(nullptr),
          button1_(BUTTON1_GPIO),
          button2_(BUTTON2_GPIO),
          display_(nullptr) {
        InitializeI2c();
        InitializeSpi();
        InitializeDisplay();
        InitializeButtons();
    }

    AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(
            i2c_bus_,
            I2C_NUM_0,
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK,
            AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_GPIO_PA,
            AUDIO_CODEC_ES8311_ADDR,
            false);
        return &audio_codec;
    }

    Display* GetDisplay() override {
        return display_;
    }

    bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        uint16_t voltage_mv = 0;
        if (pmic_.readVbat(&voltage_mv) != M5PM1_OK) {
            return false;
        }

        int charge_state_level = pmic_.digitalRead(PMIC_PIN_CHARGE_STATE);
        if (charge_state_level < 0) {
            // Charge status read failed; leave charging/discharging unknown
            charging = false;
            discharging = false;
        } else {
            // M5PM1 charge status is active low: 0 means charging, 1 means not charging
            charging = (charge_state_level == 0);
            discharging = !charging;
        }

        const int BATTERY_MIN_VOLTAGE = 3400;
        const int BATTERY_MAX_VOLTAGE = 4200;
        if (voltage_mv < BATTERY_MIN_VOLTAGE) {
            level = 0;
        } else if (voltage_mv > BATTERY_MAX_VOLTAGE) {
            level = 100;
        } else {
            level = ((voltage_mv - BATTERY_MIN_VOLTAGE) * 100) / (BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE);
        }
        return true;
    }
};

DECLARE_BOARD(M5StackStopwatchBoard);
