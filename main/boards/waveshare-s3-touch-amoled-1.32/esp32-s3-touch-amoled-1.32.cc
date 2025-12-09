#include "application.h"
#include "button.h"
#include "codecs/es8311_audio_codec.h"
#include "config.h"
#include "wifi_board.h"

#include "display/lcd_display.h"
#include "esp_lcd_sh8601.h"
#include "lvgl.h"
#include "mcp_server.h"
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>

#define TAG "waveshare_s3_amoled_1_32"

static const sh8601_lcd_init_cmd_t lcd_init_cmds[] = {
    {0xFE, (uint8_t[]) {0x00}, 1, 0},
    {0xC4, (uint8_t[]) {0x80}, 1, 0},
    {0x3A, (uint8_t[]) {0x55}, 1, 0}, // 0x55 for RGB565, 0x77 for RGB888
    {0x35, (uint8_t[]) {0x00}, 1, 0},
    {0x53, (uint8_t[]) {0x20}, 1, 0},
    {0x51, (uint8_t[]) {0xFF}, 1, 0}, // Brightness
    {0x63, (uint8_t[]) {0xFF}, 1, 0},
    {0x2A, (uint8_t[]) {0x00, 0x06, 0x01, 0xD7}, 4, 0},
    {0x2B, (uint8_t[]) {0x00, 0x00, 0x01, 0xD1}, 4, 0},
    {0x11, (uint8_t[]) {0x00}, 0, 100},
    {0x29, (uint8_t[]) {0x00}, 0, 0},
};

class CustomLcdDisplay : public SpiLcdDisplay {
  private:
    esp_lcd_panel_io_handle_t io_handle_;

  public:
    static void my_draw_event_cb(lv_event_t *e) {
        lv_area_t *area = (lv_area_t *) lv_event_get_param(e);

        uint16_t x1 = area->x1;
        uint16_t x2 = area->x2;
        uint16_t y1 = area->y1;
        uint16_t y2 = area->y2;

        // Round area for better performance
        area->x1 = (x1 >> 1) << 1;
        area->y1 = (y1 >> 1) << 1;
        area->x2 = ((x2 >> 1) << 1) + 1;
        area->y2 = ((y2 >> 1) << 1) + 1;
    }

    void SetMIRROR_XY(uint8_t mirror) {
        uint32_t lcd_cmd = 0x36;
        lcd_cmd &= 0xff;
        lcd_cmd <<= 8;
        lcd_cmd |= 0x02 << 24;
        uint8_t param = mirror;
        esp_lcd_panel_io_tx_param(io_handle_, lcd_cmd, &param, 1);
    }

    CustomLcdDisplay(esp_lcd_panel_io_handle_t io_handle, esp_lcd_panel_handle_t panel_handle, int width, int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y, bool swap_xy) : 
    SpiLcdDisplay(io_handle, panel_handle, width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy), 
    io_handle_(io_handle) {
        DisplayLockGuard lock(this);
        lv_display_add_event_cb(display_, my_draw_event_cb, LV_EVENT_INVALIDATE_AREA, NULL);
        SetMIRROR_XY(0xC0); // Rotate 180 degrees
        lv_obj_invalidate(lv_screen_active());
    }
};

class CustomBoard : public WifiBoard {
  private:
    i2c_master_bus_handle_t   i2c_bus_;
    Button                    boot_button_;
    Button                    pwr_button_;
    esp_lcd_panel_handle_t    panel_handle = NULL;
    esp_lcd_panel_io_handle_t io_handle    = NULL;
    CustomLcdDisplay         *display_;
    lv_indev_t               *touch_indev           = NULL;
    i2c_master_dev_handle_t   disp_touch_dev_handle = NULL;

    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg      = {};
        i2c_bus_cfg.i2c_port                     = I2C_NUM_0;
        i2c_bus_cfg.sda_io_num                   = AUDIO_CODEC_I2C_SDA_PIN;
        i2c_bus_cfg.scl_io_num                   = AUDIO_CODEC_I2C_SCL_PIN;
        i2c_bus_cfg.clk_source                   = I2C_CLK_SRC_DEFAULT;
        i2c_bus_cfg.glitch_ignore_cnt            = 7;
        i2c_bus_cfg.intr_priority                = 0;
        i2c_bus_cfg.trans_queue_depth            = 0;
        i2c_bus_cfg.flags.enable_internal_pullup = 1;
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    void SetDispbacklight(uint8_t backlight) {
        uint32_t lcd_cmd = 0x51;
        lcd_cmd &= 0xff;
        lcd_cmd <<= 8;
        lcd_cmd |= 0x02 << 24;
        uint8_t param = backlight;
        esp_lcd_panel_io_tx_param(io_handle, lcd_cmd, &param, 1);
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto &app = Application::GetInstance();
            // During startup (before connected), pressing BOOT button enters Wi-Fi config mode without reboot
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });

        pwr_button_.OnLongPress([this]() {
            GetDisplay()->SetChatMessage("system", "OFF");
            vTaskDelay(pdMS_TO_TICKS(1000));
            gpio_set_level(PWR_EN_GPIO, 0);
        });
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.data0_io_num     = LCD_D0;
        buscfg.data1_io_num     = LCD_D1;
        buscfg.sclk_io_num      = LCD_PCLK;
        buscfg.data2_io_num     = LCD_D2;
        buscfg.data3_io_num     = LCD_D3;
        buscfg.max_transfer_sz  = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeLcdDisplay() {
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num                   = LCD_CS;
        io_config.dc_gpio_num                   = -1;
        io_config.spi_mode                      = 0;
        io_config.pclk_hz                       = 40 * 1000 * 1000;
        io_config.trans_queue_depth             = 8;
        io_config.on_color_trans_done           = NULL;
        io_config.user_ctx                      = NULL;
        io_config.lcd_cmd_bits                  = 32;
        io_config.lcd_param_bits                = 8;
        io_config.flags.quad_mode               = true;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &io_handle));

        sh8601_vendor_config_t vendor_config    = {};
        vendor_config.init_cmds                 = lcd_init_cmds;
        vendor_config.init_cmds_size            = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]);
        vendor_config.flags.use_qspi_interface = 1;

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num             = LCD_RST;
        panel_config.rgb_ele_order              = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel             = 16;
        panel_config.vendor_config              = &vendor_config;

        ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(io_handle, &panel_config, &panel_handle));
        esp_lcd_panel_set_gap(panel_handle, 0x06, 0x00);
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

        display_ = new CustomLcdDisplay(io_handle, panel_handle, EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeTools() {
        auto &mcp_server = McpServer::GetInstance();
        mcp_server.AddTool("self.disp.setbacklight", "设置屏幕亮度", PropertyList({Property("level", kPropertyTypeInteger, 0, 255)}), [this](const PropertyList &properties) -> ReturnValue {
            int level = properties["level"].value<int>();
            ESP_LOGI("setbacklight", "%d", level);
            SetDispbacklight(level);
            return true;
        });

        mcp_server.AddTool("self.disp.network", "重新配网", PropertyList(), [this](const PropertyList &) -> ReturnValue {
            EnterWifiConfigMode();
            return true;
        });
    }

    void CheckPowerKeyState() {
        gpio_config_t gpio_conf = {};
        gpio_conf.intr_type     = GPIO_INTR_DISABLE;
        gpio_conf.mode          = GPIO_MODE_OUTPUT;
        gpio_conf.pin_bit_mask  = (0x1ULL << PWR_EN_GPIO);
        gpio_conf.pull_down_en  = GPIO_PULLDOWN_DISABLE;
        gpio_conf.pull_up_en    = GPIO_PULLUP_ENABLE;
        ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&gpio_conf));
        gpio_set_level(PWR_EN_GPIO, 1);
        do {
            vTaskDelay(pdMS_TO_TICKS(10));
        } while (!gpio_get_level(PWR_BUTTON_GPIO));
    }

  public:
    CustomBoard() : boot_button_(BOOT_BUTTON_GPIO), pwr_button_(PWR_BUTTON_GPIO) {
        CheckPowerKeyState();
        InitializeI2c();
        InitializeSpi();
        InitializeLcdDisplay();
        InitializeButtons();
        InitializeTools();
    }

    virtual AudioCodec *GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE, AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN, AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }

    virtual Display *GetDisplay() override {
        return display_;
    }
};

DECLARE_BOARD(CustomBoard);
