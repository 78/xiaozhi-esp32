#include "adc_battery_monitor.h"
#include "application.h"
#include "button.h"
#include "codecs/no_audio_codec.h"
#include "config.h"
#include "display/lcd_display.h"
#include "esp_lcd_nv3041a.h"
#include "wifi_board.h"

#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_touch_gt911.h>
#include <esp_log.h>
#include <esp_lvgl_port.h>

#define TAG "JC4827W543Board"

// The stock chat UI registers no touch handler of its own, and the only GPIO
// button on this board is the BOOT switch - which shares IO0 with the panel's
// TE line and sits where a cased device cannot reach it. Without this the
// screen is a display and nothing else: there is no way to start talking.
class JC4827W543Display : public SpiLcdDisplay {
public:
    using SpiLcdDisplay::SpiLcdDisplay;

    void SetupUI() override {
        SpiLcdDisplay::SetupUI();
        DisplayLockGuard lock(this);

        // A tap has to count wherever it lands. LVGL delivers a click to the
        // topmost clickable object under the finger, and the chat area fills
        // the screen, so a handler on the container alone never fires. Cover
        // the screen, the container and the chat area. The emoji and the logo
        // sit on the screen but are labels, which are not clickable, so taps
        // there fall through to what is underneath.
        for (lv_obj_t* obj : {lv_screen_active(), container_, content_}) {
            if (obj == nullptr) {
                continue;
            }
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(obj, OnTap, LV_EVENT_CLICKED, nullptr);
        }

        // Chat bubbles are created one per message, after this runs, and each
        // one would otherwise swallow the taps that land on it. Have them pass
        // their events up to the chat area instead.
        if (content_ != nullptr) {
            lv_obj_add_event_cb(
                content_,
                [](lv_event_t* e) {
                    lv_obj_t* chat = static_cast<lv_obj_t*>(lv_event_get_user_data(e));
                    uint32_t count = lv_obj_get_child_cnt(chat);
                    if (count > 0) {
                        lv_obj_add_flag(lv_obj_get_child(chat, count - 1),
                                        LV_OBJ_FLAG_EVENT_BUBBLE);
                    }
                },
                LV_EVENT_CHILD_CREATED, content_);
        }
    }

private:
    static void OnTap(lv_event_t* e) {
        ESP_LOGI(TAG, "tap: toggling chat state");
        // ToggleChatState only sets an event bit, so the LVGL task may call it.
        Application::GetInstance().ToggleChatState();
    }
};

class JC4827W543Board : public WifiBoard {
private:
    Button boot_button_;
    i2c_master_bus_handle_t touch_i2c_bus_ = nullptr;
    LcdDisplay* display_ = nullptr;

#ifdef CONFIG_JC4827W543_EXTERNAL_AMP
    // The on-board NS4168 is always powered and has no enable pin. This variant
    // drives an external amplifier instead, so the NS4168's I2S lines are never
    // driven - and floating next to the QSPI display they pick up enough noise
    // for it to play a steady hiss. Park them low.
    void SilenceOnboardAmplifier() {
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << AUDIO_NS4168_GPIO_LRCK) | (1ULL << AUDIO_NS4168_GPIO_DOUT) |
                            (1ULL << AUDIO_NS4168_GPIO_BCLK),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&io_conf));
        gpio_set_level(AUDIO_NS4168_GPIO_LRCK, 0);
        gpio_set_level(AUDIO_NS4168_GPIO_DOUT, 0);
        gpio_set_level(AUDIO_NS4168_GPIO_BCLK, 0);
    }
#endif

    void InitializeSpi() {
        ESP_LOGI(TAG, "Initialize QSPI bus");
        spi_bus_config_t bus_config = {};
        bus_config.sclk_io_num = QSPI_PIN_NUM_LCD_PCLK;
        bus_config.data0_io_num = QSPI_PIN_NUM_LCD_DATA0;
        bus_config.data1_io_num = QSPI_PIN_NUM_LCD_DATA1;
        bus_config.data2_io_num = QSPI_PIN_NUM_LCD_DATA2;
        bus_config.data3_io_num = QSPI_PIN_NUM_LCD_DATA3;
        bus_config.max_transfer_sz = DISPLAY_WIDTH * 80 * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(QSPI_LCD_HOST, &bus_config, SPI_DMA_CH_AUTO));
    }

    void InitializeDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        ESP_LOGI(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = QSPI_PIN_NUM_LCD_CS;
        io_config.dc_gpio_num = GPIO_NUM_NC;
        io_config.spi_mode = 0;
        io_config.pclk_hz = QSPI_LCD_PCLK_HZ;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 32;
        io_config.lcd_param_bits = 8;
        io_config.flags.quad_mode = true;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)QSPI_LCD_HOST,
                                                 &io_config, &panel_io));

        ESP_LOGI(TAG, "Install NV3041A panel driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = QSPI_PIN_NUM_LCD_RST;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = QSPI_LCD_BIT_PER_PIXEL;
        ESP_ERROR_CHECK(esp_lcd_new_panel_nv3041a(panel_io, &panel_config, &panel));

        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));
        // IPS glass: the picture is inverted without this.
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR));

        display_ = new JC4827W543Display(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                         DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X,
                                         DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeTouchI2c() {
        i2c_master_bus_config_t bus_config = {};
        bus_config.i2c_port = TOUCH_I2C_PORT;
        bus_config.sda_io_num = TOUCH_PIN_NUM_SDA;
        bus_config.scl_io_num = TOUCH_PIN_NUM_SCL;
        bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
        bus_config.glitch_ignore_cnt = 7;
        bus_config.flags.enable_internal_pullup = 1;
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &touch_i2c_bus_));
    }

    bool TryInitializeTouchAt(uint8_t dev_addr) {
        esp_lcd_panel_io_handle_t tp_io_handle = nullptr;
        esp_lcd_panel_io_i2c_config_t tp_io_config = {};
        tp_io_config.dev_addr = dev_addr;
        tp_io_config.control_phase_bytes = 1;
        tp_io_config.dc_bit_offset = 0;
        tp_io_config.lcd_cmd_bits = 16;
        tp_io_config.flags.disable_control_phase = 1;
        tp_io_config.scl_speed_hz = 400000;
        if (esp_lcd_new_panel_io_i2c(touch_i2c_bus_, &tp_io_config, &tp_io_handle) != ESP_OK) {
            return false;
        }

        // Without driver_data the GT911 driver skips its address-selection
        // sequence entirely, resets the chip with INT left floating, and the
        // address it comes up on is then whatever the pin happened to read.
        // That fails on this board. Passing the address here makes the driver
        // drive INT during reset, which is what actually picks the address.
        static esp_lcd_touch_io_gt911_config_t gt911_config = {};
        gt911_config.dev_addr = dev_addr;

        esp_lcd_touch_config_t tp_cfg = {};
        tp_cfg.x_max = DISPLAY_WIDTH;
        tp_cfg.y_max = DISPLAY_HEIGHT;
        tp_cfg.rst_gpio_num = TOUCH_PIN_NUM_RST;
        tp_cfg.int_gpio_num = TOUCH_PIN_NUM_INT;
        tp_cfg.levels.reset = 0;
        tp_cfg.levels.interrupt = 0;
        tp_cfg.flags.swap_xy = DISPLAY_SWAP_XY;
        tp_cfg.flags.mirror_x = DISPLAY_MIRROR_X;
        tp_cfg.flags.mirror_y = DISPLAY_MIRROR_Y;
        tp_cfg.driver_data = &gt911_config;

        esp_lcd_touch_handle_t tp = nullptr;
        esp_err_t err = esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &tp);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "No GT911 at 0x%02X: %s", dev_addr, esp_err_to_name(err));
            esp_lcd_panel_io_del(tp_io_handle);
            return false;
        }

        lvgl_port_touch_cfg_t touch_cfg = {};
        touch_cfg.disp = lv_display_get_default();
        touch_cfg.handle = tp;
        if (touch_cfg.disp == nullptr) {
            ESP_LOGE(TAG, "LVGL display is not initialized, touch stays inactive");
            return false;
        }
        lv_indev_t* indev = lvgl_port_add_touch(&touch_cfg);
        if (indev == nullptr) {
            ESP_LOGE(TAG, "lvgl_port_add_touch failed, touch stays inactive");
            return false;
        }

        ESP_LOGI(TAG, "GT911 ready at 0x%02X", dev_addr);
        return true;
    }

    void InitializeTouch() {
        // The address is latched from INT while RESET is released: low selects
        // 0x5D, high selects 0x14. Driving it should settle the question, but
        // probe the other address too rather than trust the pin: touch dying
        // silently while the display keeps working is hard to diagnose.
        if (TryInitializeTouchAt(ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS)) {
            return;
        }
        if (TryInitializeTouchAt(ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP)) {
            return;
        }
        // Not fatal: the display is on QSPI and stays usable. Say so loudly
        // instead of taking the whole board down with it.
        ESP_LOGE(TAG, "GT911 not found at 0x5D or 0x14, touch disabled");
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
    }

public:
    JC4827W543Board() : boot_button_(BOOT_BUTTON_GPIO) {
#ifdef CONFIG_JC4827W543_EXTERNAL_AMP
        SilenceOnboardAmplifier();
#endif
        InitializeSpi();
        InitializeDisplay();
        InitializeTouchI2c();
        InitializeTouch();
        InitializeButtons();
        GetBacklight()->RestoreBrightness();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                               AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK,
                                               AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK,
                                               AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override { return display_; }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        static AdcBatteryMonitor battery_monitor(BATTERY_ADC_UNIT, BATTERY_ADC_CHANNEL,
                                                 BATTERY_UPPER_RESISTOR, BATTERY_LOWER_RESISTOR);
        level = battery_monitor.GetBatteryLevel();
        charging = battery_monitor.IsCharging();
        discharging = battery_monitor.IsDischarging();
        return true;
    }
};

DECLARE_BOARD(JC4827W543Board);
