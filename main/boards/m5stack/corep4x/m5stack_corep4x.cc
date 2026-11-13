#include "M5IOE1.h"
#include "M5PM1.h"
#include "application.h"
#include "backlight.h"
#include "codecs/box_audio_codec.h"
#include "config.h"
#include "display/lcd_display.h"
#include "esp_lcd_touch_cst3530.h"
#include "wifi_board.h"

#include <driver/i2c_master.h>
#include <esp_idf_version.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <initializer_list>

#include <esp_lvgl_port.h>
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st7102.h"
#include "esp_ldo_regulator.h"

#define TAG "M5StackCoreP4X"

static const st7102_lcd_init_cmd_t st7102_init_cmds[] = {
    {.cmd = 0x99, .data = (uint8_t[]){0x71, 0x02, 0xA2}, .data_bytes = 3, .delay_ms = 0},
    {.cmd = 0x99, .data = (uint8_t[]){0x71, 0x02, 0xA3}, .data_bytes = 3, .delay_ms = 0},
    {.cmd = 0x99, .data = (uint8_t[]){0x71, 0x02, 0xA4}, .data_bytes = 3, .delay_ms = 0},
    {.cmd = 0x78, .data = (uint8_t[]){0x21}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0x79, .data = (uint8_t[]){0xCF}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0xB0,
     .data = (uint8_t[]){0x22, 0x43, 0x1E, 0x43, 0x2F, 0x57, 0x57},
     .data_bytes = 7,
     .delay_ms = 0},
    {.cmd = 0xB7, .data = (uint8_t[]){0x7D, 0x7D}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = 0xBF, .data = (uint8_t[]){0x7A, 0x7A}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = 0xC8,
     .data =
         (uint8_t[]){0x00, 0x00, 0x13, 0x23, 0x3E, 0x00, 0x6A, 0x03, 0xB0, 0x06, 0x11, 0x0F, 0x07,
                     0x85, 0x03, 0x21, 0xD5, 0x01, 0x18, 0x00, 0x22, 0x56, 0x0F, 0x98, 0x0A, 0x32,
                     0xF8, 0x0D, 0x48, 0x0F, 0xF3, 0x80, 0x0F, 0xAC, 0xC1, 0x03, 0xC4},
     .data_bytes = 37,
     .delay_ms = 0},
    {.cmd = 0xC9,
     .data =
         (uint8_t[]){0x00, 0x00, 0x13, 0x23, 0x3E, 0x00, 0x6A, 0x03, 0xB0, 0x06, 0x11, 0x0F, 0x07,
                     0x85, 0x03, 0x21, 0xD5, 0x01, 0x18, 0x00, 0x22, 0x56, 0x0F, 0x98, 0x0A, 0x32,
                     0xF8, 0x0D, 0x48, 0x0F, 0xF3, 0x80, 0x0F, 0xAC, 0xC1, 0x03, 0xC4},
     .data_bytes = 37,
     .delay_ms = 0},
    {.cmd = 0xD7,
     .data = (uint8_t[]){0x10, 0x0C, 0x02, 0x19, 0x40, 0x40},
     .data_bytes = 6,
     .delay_ms = 0},
    {.cmd = 0xA3,
     .data = (uint8_t[]){0x40, 0x03, 0x80, 0xCF, 0x44, 0x00, 0x00, 0x00, 0x02, 0x05, 0x6F,
                         0x6F, 0x00, 0x1A, 0x00, 0x45, 0x05, 0x00, 0x00, 0x00, 0x00, 0x46,
                         0x00, 0x00, 0x02, 0x20, 0x52, 0x00, 0x05, 0x00, 0x00, 0xFF},
     .data_bytes = 32,
     .delay_ms = 0},
    {.cmd = 0xA6,
     .data = (uint8_t[]){0x02, 0x00, 0x24, 0x55, 0x35, 0x00, 0x38, 0x00, 0x97, 0x97, 0x00,
                         0x24, 0x55, 0x36, 0x00, 0x37, 0x00, 0x97, 0x97, 0x02, 0xAC, 0x51,
                         0x3A, 0x00, 0x00, 0x00, 0x97, 0x97, 0x00, 0xAC, 0x21, 0x00, 0x0B,
                         0x00, 0x00, 0x97, 0x97, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00},
     .data_bytes = 44,
     .delay_ms = 0},
    {.cmd = 0xA7,
     .data = (uint8_t[]){0x19, 0x19, 0x00, 0x64, 0x40, 0x07, 0x16, 0x40, 0x00, 0x04, 0x03, 0x97,
                         0x97, 0x00, 0x64, 0x40, 0x25, 0x34, 0x00, 0x00, 0x02, 0x01, 0x97, 0x97,
                         0x00, 0x64, 0x40, 0x4B, 0x5A, 0x00, 0x00, 0x02, 0x01, 0x97, 0x97, 0x00,
                         0x24, 0x40, 0x69, 0x78, 0x00, 0x00, 0x00, 0x00, 0x97, 0x97, 0x00, 0x44},
     .data_bytes = 48,
     .delay_ms = 0},
    {.cmd = 0xAC,
     .data =
         (uint8_t[]){0x11, 0x08, 0x13, 0x0A, 0x18, 0x1A, 0x1B, 0x00, 0x06, 0x03, 0x19, 0x1B, 0x1B,
                     0x1B, 0x18, 0x1B, 0x10, 0x09, 0x12, 0x0B, 0x18, 0x1A, 0x1B, 0x02, 0x06, 0x01,
                     0x19, 0x1B, 0x1B, 0x1B, 0x18, 0x1B, 0xFF, 0x67, 0xFF, 0x67, 0x00},
     .data_bytes = 37,
     .delay_ms = 0},
    {.cmd = 0xAD,
     .data = (uint8_t[]){0xCC, 0x40, 0x46, 0x11, 0x04, 0x6F, 0x6F},
     .data_bytes = 7,
     .delay_ms = 0},
    {.cmd = 0xE8,
     .data = (uint8_t[]){0x30, 0x07, 0x00, 0xB3, 0xB3, 0x9C, 0x00, 0xE2, 0x04, 0x00, 0x00, 0x00,
                         0x00, 0xEF},
     .data_bytes = 14,
     .delay_ms = 0},
    {.cmd = 0x75, .data = (uint8_t[]){0x03, 0x04}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = 0xE7,
     .data = (uint8_t[]){0x8B, 0x3C, 0x00, 0x0C, 0xF0, 0x5D, 0x00, 0x5D, 0x00, 0x5D, 0x00,
                         0x5D, 0x00, 0xFF, 0x00, 0x08, 0x7B, 0x00, 0x00, 0xC8, 0x6A, 0x5A,
                         0x08, 0x1A, 0x3C, 0x00, 0x71, 0x01, 0x8C, 0x01, 0x7F, 0xF0, 0x22},
     .data_bytes = 33,
     .delay_ms = 0},
    {.cmd = 0xE9,
     .data = (uint8_t[]){0x3C, 0x7F, 0x08, 0x07, 0x1A, 0x7A, 0x22, 0x1A, 0x33},
     .data_bytes = 9,
     .delay_ms = 0},
    {.cmd = 0x11, .data = NULL, .data_bytes = 0, .delay_ms = 20},
    {.cmd = 0x36, .data = (uint8_t[]){0b11}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0x29, .data = NULL, .data_bytes = 0, .delay_ms = 20},
    {.cmd = 0x35, .data = (uint8_t[]){0x00}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0x29, .data = NULL, .data_bytes = 0, .delay_ms = 0}};

static int BatteryLevelFromVoltage(uint16_t voltage_mv) {
    static constexpr uint16_t curve[][2] = {
        {3300, 0},  {3500, 5},  {3600, 10}, {3700, 20}, {3750, 30}, {3790, 40},
        {3830, 50}, {3870, 60}, {3920, 70}, {3980, 80}, {4070, 90}, {4200, 100},
    };

    if (voltage_mv <= curve[0][0]) {
        return 0;
    }
    for (size_t i = 1; i < sizeof(curve) / sizeof(curve[0]); ++i) {
        if (voltage_mv <= curve[i][0]) {
            const int voltage_range = curve[i][0] - curve[i - 1][0];
            const int level_range = curve[i][1] - curve[i - 1][1];
            return curve[i - 1][1] +
                   ((voltage_mv - curve[i - 1][0]) * level_range + voltage_range / 2) /
                       voltage_range;
        }
    }
    return 100;
}

class M5IoE1Backlight : public Backlight {
public:
    explicit M5IoE1Backlight(M5IOE1* ioe) : ioe_(ioe) {}

    void SetBrightnessImpl(uint8_t brightness) override {
        if (!ioe_) {
            return;
        }
        ioe_->setPwmDuty(M5IOE1_PWM_CH1, brightness, false, true);
        brightness_ = brightness;
    }

private:
    M5IOE1* ioe_;
};

class M5StackCoreP4XDisplay : public MipiLcdDisplay {
public:
    M5StackCoreP4XDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel)
        : MipiLcdDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X,
                         DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY) {
        if (display_ != nullptr && Lock(0)) {
            lv_display_set_rotation(display_, LV_DISPLAY_ROTATION_180);
            Unlock();
        }
    }

    void SetupUI() override {
        MipiLcdDisplay::SetupUI();
        DisplayLockGuard lock(this);

        // The stock chat UI has no touch action. Register the screen and its
        // full-size containers so a tap anywhere toggles the conversation.
        for (lv_obj_t* obj : {lv_screen_active(), container_, content_}) {
            if (obj == nullptr) {
                continue;
            }
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(obj, OnTap, LV_EVENT_CLICKED, nullptr);
        }

        // Chat bubbles are created after SetupUI(). Bubble their click events
        // to the chat container instead of letting them swallow the tap.
        if (content_ != nullptr) {
            lv_obj_add_event_cb(
                content_,
                [](lv_event_t* event) {
                    auto* chat = static_cast<lv_obj_t*>(lv_event_get_user_data(event));
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
    static void OnTap(lv_event_t*) {
        ESP_LOGI(TAG, "Touch tap: toggling chat state");
        Application::GetInstance().ToggleChatState();
    }
};

using TouchReadDataFn = esp_err_t (*)(esp_lcd_touch_handle_t);
static TouchReadDataFn cst3530_read_data = nullptr;

static esp_err_t ReadCst3530DataForLvgl(esp_lcd_touch_handle_t touch) {
    if (cst3530_read_data == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = cst3530_read_data(touch);
    if (err != ESP_OK) {
        // esp_lvgl_port aborts on any touch read error. A bad CST3530 frame is
        // recoverable, so discard it and wait for the next touch interrupt.
        portENTER_CRITICAL(&touch->data.lock);
        touch->data.points = 0;
        portEXIT_CRITICAL(&touch->data.lock);
    }
    return ESP_OK;
}

class M5StackCoreP4XBoard : public WifiBoard {
private:
    LcdDisplay* display_;
    i2c_master_bus_handle_t i2c_bus_;
    esp_lcd_panel_io_handle_t touch_io_;
    esp_lcd_touch_handle_t touch_;
    lv_indev_t* touch_indev_;
    M5IOE1 ioe_;
    M5PM1* pmic_;
    M5IoE1Backlight* backlight_;
    bool audio_power_initialized_;

    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = SYS_I2C_PORT,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags =
                {
                    .enable_internal_pullup = true,
                },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
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

    void InitializePm1() {
        // CoreP4X uses PM1 at M5PM1_DEFAULT_ADDR (0x6E) on the 100 kHz system I2C bus.
        ESP_LOGI(TAG, "M5Stack PMIC Init.");
        pmic_ = new M5PM1();
        if (pmic_->begin(i2c_bus_, M5PM1_DEFAULT_ADDR, M5PM1_I2C_FREQ_100K) != M5PM1_OK) {
            ESP_LOGE(TAG, "M5PM1 begin failed");
            delete pmic_;
            pmic_ = nullptr;
            return;
        }
        if (pmic_->setChargeEnable(true) != M5PM1_OK) {
            ESP_LOGE(TAG, "Failed to enable battery charging");
        }
        uint8_t device_id = 0, model = 0, hw_rev = 0, sw_rev = 0;
        if (pmic_->getDeviceId(&device_id) == M5PM1_OK &&
            pmic_->getDeviceModel(&model) == M5PM1_OK && pmic_->getHwVersion(&hw_rev) == M5PM1_OK &&
            pmic_->getSwVersion(&sw_rev) == M5PM1_OK) {
            ESP_LOGI(TAG, "PMIC: ID=0x%02X Model=0x%02X HW=0x%02X SW=0x%02X", device_id, model,
                     hw_rev, sw_rev);
        }
    }

    void InitializeIoExpander() {
        // CoreP4X uses the configurable M5IOE1 address 0x4F.
        if (ioe_.begin(i2c_bus_, 0x4F, M5IOE1_I2C_FREQ_DEFAULT, M5IOE1_INT_MODE_POLLING) !=
            M5IOE1_OK) {
            ESP_LOGE(TAG, "M5IOE1 begin failed");
            return;
        }

        // Order matches reference bsp_display_brightness_init + bsp_display_start_with_config
        // (display.cpp).

        // Charging status is active low on M5IOE1 G6.
        ioe_.pinMode(IOE1_PIN_CHARGE_STATE, INPUT_PULLUP);

        // 1) Backlight on G9 / PWM1
        ioe_.pinMode(IOE1_PIN_LCD_BL, OUTPUT);
        ioe_.setDriveMode(IOE1_PIN_LCD_BL, M5IOE1_DRIVE_PUSHPULL);
        ioe_.digitalWrite(IOE1_PIN_LCD_BL, HIGH);
        ioe_.setPwmFrequency(1000);
        ioe_.setPwmDuty(M5IOE1_PWM_CH1, 0, false, true);

        // 2) TP_RST G8
        ioe_.pinMode(IOE1_PIN_TOUCH_RST, OUTPUT);
        ioe_.setDriveMode(IOE1_PIN_TOUCH_RST, M5IOE1_DRIVE_PUSHPULL);
        ioe_.digitalWrite(IOE1_PIN_TOUCH_RST, LOW);
        vTaskDelay(pdMS_TO_TICKS(10));
        ioe_.digitalWrite(IOE1_PIN_TOUCH_RST, HIGH);
        vTaskDelay(pdMS_TO_TICKS(50));

        // 3) LCD_PWR G10, LCD_RST G11
        ioe_.pinMode(IOE1_PIN_LCD_PWR, OUTPUT);
        ioe_.setDriveMode(IOE1_PIN_LCD_PWR, M5IOE1_DRIVE_PUSHPULL);
        ioe_.digitalWrite(IOE1_PIN_LCD_PWR, HIGH);

        ioe_.pinMode(IOE1_PIN_LCD_RST, OUTPUT);
        ioe_.setDriveMode(IOE1_PIN_LCD_RST, M5IOE1_DRIVE_PUSHPULL);
        ioe_.digitalWrite(IOE1_PIN_LCD_RST, HIGH);

        vTaskDelay(pdMS_TO_TICKS(20));
    }

    void InitializeAudioPower() {
        // Match AiFlow CoreP4X: mute the PA while powering the codecs, then enable it after 20 ms.
        for (uint8_t pin : {uint8_t(IOE1_PIN_AUDIO_PWR), uint8_t(IOE1_PIN_PA_EN)}) {
            ioe_.pinMode(pin, OUTPUT);
            ioe_.setDriveMode(pin, M5IOE1_DRIVE_PUSHPULL);
        }
        ioe_.digitalWrite(IOE1_PIN_PA_EN, LOW);
        ioe_.digitalWrite(IOE1_PIN_AUDIO_PWR, HIGH);
        vTaskDelay(pdMS_TO_TICKS(20));
        ioe_.digitalWrite(IOE1_PIN_PA_EN, HIGH);
    }

    static esp_err_t bsp_enable_dsi_phy_power() {
        static esp_ldo_channel_handle_t phy_pwr_chan = nullptr;
        esp_ldo_channel_config_t ldo_cfg = {
            .chan_id = MIPI_DSI_PHY_PWR_LDO_CHAN,
            .voltage_mv = MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV,
        };
        esp_err_t ret = esp_ldo_acquire_channel(&ldo_cfg, &phy_pwr_chan);
        if (ret != ESP_OK) {
            return ret;
        }
        ESP_LOGI(TAG, "MIPI DSI PHY powered on");
        return ESP_OK;
    }

    void InitializeDisplay() {
        esp_lcd_panel_io_handle_t io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        esp_lcd_dsi_bus_handle_t mipi_dsi_bus = nullptr;

        ESP_ERROR_CHECK(bsp_enable_dsi_phy_power());

        /* create MIPI DSI bus first, it will initialize the DSI PHY as well */
        esp_lcd_dsi_bus_config_t bus_config = {
            .bus_id = 0,
            .num_data_lanes = DISPLAY_MIPI_LANE_NUM,
            .phy_clk_src = MIPI_DSI_PHY_PLLREF_CLK_SRC_DEFAULT,
            .lane_bit_rate_mbps = DISPLAY_MIPI_LANE_BITRATE_MBPS,
        };
        ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus));

        ESP_LOGI(TAG, "Install MIPI DSI LCD control panel");
        // we use DBI interface to send LCD commands and parameters
        esp_lcd_dbi_io_config_t dbi_config = {
            .virtual_channel = 0,
            .lcd_cmd_bits = 8,    // according to the LCD spec
            .lcd_param_bits = 8,  // according to the LCD spec
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &io));

        // Wait for MIPI DSI PHY to be ready
        vTaskDelay(pdMS_TO_TICKS(50));

        ESP_LOGI(TAG, "Install LCD driver of st7102");
        esp_lcd_dpi_panel_config_t dpi_config = {
            .virtual_channel = 0,
            .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
            .dpi_clock_freq_mhz = DISPLAY_PIXEL_CLOCK_MHZ,  // 480x480 60Hz RGB565
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
            .in_color_format = LCD_COLOR_FMT_RGB565,
            .out_color_format = LCD_COLOR_FMT_RGB565,
#else
            .pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565,  // RGB565 for 16 bits_per_pixel
            .in_color_format = LCD_COLOR_FMT_RGB565,
            .out_color_format = LCD_COLOR_FMT_RGB565,
#endif
            .num_fbs = 2,  // Double buffering for better performance
            .video_timing =
                {
                    .h_size = DISPLAY_WIDTH,
                    .v_size = DISPLAY_HEIGHT,
                    .hsync_pulse_width = DISPLAY_HSYNC_PW,
                    .hsync_back_porch = DISPLAY_HSYNC_BP,
                    .hsync_front_porch = DISPLAY_HSYNC_FP,
                    .vsync_pulse_width = DISPLAY_VSYNC_PW,
                    .vsync_back_porch = DISPLAY_VSYNC_BP,
                    .vsync_front_porch = DISPLAY_VSYNC_FP,
                },
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0)
            .flags = {.use_dma2d = true},
#endif
        };

        st7102_vendor_config_t vendor_config = {
            .init_cmds = st7102_init_cmds,
            .init_cmds_size = sizeof(st7102_init_cmds) / sizeof(st7102_init_cmds[0]),
            .mipi_config =
                {
                    .dsi_bus = mipi_dsi_bus,
                    .dpi_config = &dpi_config,
                },
            .flags =
                {
                    .use_mipi_interface = 1,
                },
        };

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.vendor_config = &vendor_config;

        ESP_ERROR_CHECK(esp_lcd_new_panel_st7102(io, &panel_config, &panel));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

        ESP_LOGI(TAG, "Display initialized with resolution %dx%d", DISPLAY_WIDTH, DISPLAY_HEIGHT);

        display_ = new M5StackCoreP4XDisplay(io, panel);
    }

    void InitializeTouch() {
        const esp_lcd_touch_config_t touch_config = {
            .x_max = DISPLAY_WIDTH,
            .y_max = DISPLAY_HEIGHT,
            .rst_gpio_num = GPIO_NUM_NC,
            .int_gpio_num = TOUCH_INT_PIN,
            .levels =
                {
                    .reset = 0,
                    .interrupt = 0,
                },
            .flags =
                {
                    .swap_xy = 0,
                    // The panel is rotated 180 degrees in LVGL. Apply the same
                    // transform to raw CST3530 coordinates.
                    .mirror_x = 1,
                    .mirror_y = 1,
                },
            // The registry driver sends the controller's 32-bit commands
            // directly through the new I2C master API.
            .driver_data = i2c_bus_,
        };

        // waveshare/esp_lcd_touch_cst3530 1.0.0 initializes dev_addr after
        // scl_speed_hz, which is rejected by this C++ build. Keep the vendor
        // values while following ESP-IDF's declaration order.
        esp_lcd_panel_io_i2c_config_t touch_io_config = {
            .dev_addr = ESP_LCD_TOUCH_IO_I2C_CST3530_ADDRESS,
            .scl_speed_hz = 100000,
            .control_phase_bytes = 1,
            .dc_bit_offset = 0,
            .lcd_cmd_bits = 8,
            .flags =
                {
                    .disable_control_phase = 1,
                },
        };
        esp_err_t err = esp_lcd_new_panel_io_i2c(i2c_bus_, &touch_io_config, &touch_io_);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create CST3530 I2C IO: %s", esp_err_to_name(err));
            return;
        }

        err = esp_lcd_touch_new_i2c_cst3530(touch_io_, &touch_config, &touch_);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize CST3530 touch: %s", esp_err_to_name(err));
            esp_lcd_panel_io_del(touch_io_);
            touch_io_ = nullptr;
            return;
        }

        cst3530_read_data = touch_->read_data;
        touch_->read_data = ReadCst3530DataForLvgl;

        lv_display_t* lv_display = lv_display_get_default();
        if (lv_display == nullptr) {
            ESP_LOGE(TAG, "Cannot register touch input without an LVGL display");
            return;
        }
        const lvgl_port_touch_cfg_t lv_touch_config = {
            .disp = lv_display,
            .handle = touch_,
        };
        touch_indev_ = lvgl_port_add_touch(&lv_touch_config);
        if (touch_indev_ == nullptr) {
            ESP_LOGE(TAG, "Failed to register CST3530 touch input");
            return;
        }
        ESP_LOGI(TAG, "CST3530 touch initialized at I2C address 0x%02X",
                 ESP_LCD_TOUCH_IO_I2C_CST3530_ADDRESS);
    }

public:
    M5StackCoreP4XBoard()
        : display_(nullptr),
          i2c_bus_(nullptr),
          touch_io_(nullptr),
          touch_(nullptr),
          touch_indev_(nullptr),
          pmic_(nullptr),
          backlight_(nullptr),
          audio_power_initialized_(false) {
        InitializeI2c();
        I2cDetect();
        InitializePm1();
        InitializeIoExpander();
        InitializeDisplay();
        InitializeTouch();
        backlight_ = new M5IoE1Backlight(&ioe_);
        backlight_->SetBrightness(90);
    }

    AudioCodec* GetAudioCodec() override {
        if (!audio_power_initialized_) {
            InitializeAudioPower();
            audio_power_initialized_ = true;
        }
        static BoxAudioCodec audio_codec(
            i2c_bus_, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE, AUDIO_I2S_GPIO_MCLK,
            AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_GPIO_PA, AUDIO_CODEC_ES8311_ADDR, AUDIO_CODEC_ES7210_ADDR,
            AUDIO_INPUT_REFERENCE);
        return &audio_codec;
    }

    Display* GetDisplay() override { return display_; }

    Backlight* GetBacklight() override { return backlight_; }

    bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        if (!pmic_) {
            return false;
        }

        uint16_t voltage_mv = 0;
        if (pmic_->readVbat(&voltage_mv) != M5PM1_OK) {
            return false;
        }

        const int charge_state = ioe_.digitalRead(IOE1_PIN_CHARGE_STATE);
        if (charge_state < 0) {
            return false;
        }
        charging = charge_state == LOW;

        m5pm1_pwr_src_t power_source = M5PM1_PWR_SRC_UNKNOWN;
        if (pmic_->getPowerSource(&power_source) == M5PM1_OK) {
            discharging = power_source == M5PM1_PWR_SRC_BAT;
        } else {
            discharging = !charging;
        }

        level = BatteryLevelFromVoltage(voltage_mv);
        ESP_LOGD(TAG, "Battery: %d%% (%u mV), charging=%s, discharging=%s", level, voltage_mv,
                 charging ? "yes" : "no", discharging ? "yes" : "no");
        return true;
    }
};

DECLARE_BOARD(M5StackCoreP4XBoard);
