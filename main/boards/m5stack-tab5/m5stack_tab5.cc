#include "wifi_board.h"
#include "tab5_audio_codec.h"
#include "display/lcd_display.h"
#include "esp_lcd_ili9881c.h"
#include "esp_lcd_st7123.h"
#include "font_emoji.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "esp32_camera.h"
#include "esp_video_init.h"
#include "esp_cam_sensor_xclk.h"

#include <esp_log.h>
#include "esp_check.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_ldo_regulator.h"
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include "i2c_device.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_lcd_touch_st7123.h"
#include <cstring>

#define TAG "M5StackTab5Board"

#define AUDIO_CODEC_ES8388_ADDR ES8388_CODEC_DEFAULT_ADDR
#define LCD_MIPI_DSI_PHY_PWR_LDO_CHAN       3  // LDO_VO3 is connected to VDD_MIPI_DPHY
#define LCD_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV 2500
#define ST7123_TOUCH_I2C_ADDRESS            0x55


// PI4IO registers
#define PI4IO_REG_CHIP_RESET 0x01
#define PI4IO_REG_IO_DIR     0x03
#define PI4IO_REG_OUT_SET    0x05
#define PI4IO_REG_OUT_H_IM   0x07
#define PI4IO_REG_IN_DEF_STA 0x09
#define PI4IO_REG_PULL_EN    0x0B
#define PI4IO_REG_PULL_SEL   0x0D
#define PI4IO_REG_IN_STA     0x0F
#define PI4IO_REG_INT_MASK   0x11
#define PI4IO_REG_IRQ_STA    0x13

// Bit manipulation macros
#define setbit(x, bit)  ((x) |= (1U << (bit)))
#define clrbit(x, bit)  ((x) &= ~(1U << (bit)))

class Pi4ioe1 : public I2cDevice {
public:
    Pi4ioe1(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
        WriteReg(PI4IO_REG_CHIP_RESET, 0xFF);
        uint8_t data = ReadReg(PI4IO_REG_CHIP_RESET);
        WriteReg(PI4IO_REG_IO_DIR, 0b01111111);      // 0: input 1: output
        WriteReg(PI4IO_REG_OUT_H_IM, 0b00000000);    // 使用到的引脚关闭 High-Impedance
        WriteReg(PI4IO_REG_PULL_SEL, 0b01111111);    // pull up/down select, 0 down, 1 up
        WriteReg(PI4IO_REG_PULL_EN, 0b01111111);     // pull up/down enable, 0 disable, 1 enable
        WriteReg(PI4IO_REG_IN_DEF_STA, 0b10000000);  // P1, P7 默认高电平
        WriteReg(PI4IO_REG_INT_MASK, 0b01111111);    // P7 中断使能 0 enable, 1 disable
        WriteReg(PI4IO_REG_OUT_SET, 0b01110110);     // Output Port Register P1(SPK_EN), P2(EXT5V_EN), P4(LCD_RST), P5(TP_RST), P6(CAM)RST 输出高电平
    }

    uint8_t ReadOutSet() { return ReadReg(PI4IO_REG_OUT_SET); }
    void WriteOutSet(uint8_t value) { WriteReg(PI4IO_REG_OUT_SET, value); }
};

class Pi4ioe2 : public I2cDevice {
public:
    Pi4ioe2(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
        WriteReg(PI4IO_REG_CHIP_RESET, 0xFF);
        uint8_t data = ReadReg(PI4IO_REG_CHIP_RESET);
        WriteReg(PI4IO_REG_IO_DIR, 0b10111001);      // 0: input 1: output
        WriteReg(PI4IO_REG_OUT_H_IM, 0b00000110);    // 使用到的引脚关闭 High-Impedance
        WriteReg(PI4IO_REG_PULL_SEL, 0b10111001);    // pull up/down select, 0 down, 1 up
        WriteReg(PI4IO_REG_PULL_EN, 0b11111001);     // pull up/down enable, 0 disable, 1 enable
        WriteReg(PI4IO_REG_IN_DEF_STA, 0b01000000);  // P6 默认高电平
        WriteReg(PI4IO_REG_INT_MASK, 0b10111111);    // P6 中断使能 0 enable, 1 disable
        WriteReg(PI4IO_REG_OUT_SET, 0b10001001);     // Output Port Register P0(WLAN_PWR_EN), P3(USB5V_EN), P7(CHG_EN) 输出高电平
    }

    uint8_t ReadOutSet() { return ReadReg(PI4IO_REG_OUT_SET); }
    void WriteOutSet(uint8_t value) { WriteReg(PI4IO_REG_OUT_SET, value); }
};

class M5StackTab5Board : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    Button boot_button_;
    LcdDisplay* display_;
    Esp32Camera* camera_ = nullptr;
    Pi4ioe1* pi4ioe1_;
    Pi4ioe2* pi4ioe2_;
    esp_lcd_touch_handle_t touch_ = nullptr;

    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)1,
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

    static esp_err_t bsp_enable_dsi_phy_power() {
        esp_ldo_channel_handle_t ldo_mipi_phy        = NULL;
        esp_ldo_channel_config_t ldo_mipi_phy_config = {
            .chan_id    = LCD_MIPI_DSI_PHY_PWR_LDO_CHAN,
            .voltage_mv = LCD_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV,
        };
        return esp_ldo_acquire_channel(&ldo_mipi_phy_config, &ldo_mipi_phy);
    }

    void I2cDetect() {
        uint8_t address;
        printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\r\n");
        for (int i = 0; i < 128; i += 16) {
            printf("%02x: ", i);
            for (int j = 0; j < 16; j++) {
                fflush(stdout);
                address       = i + j;
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

    void InitializePi4ioe() {
        ESP_LOGI(TAG, "Init I/O Exapander PI4IOE");
        pi4ioe1_ = new Pi4ioe1(i2c_bus_, 0x43);
        pi4ioe2_ = new Pi4ioe2(i2c_bus_, 0x44);
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

    void InitializeGt911TouchPad() {
        ESP_LOGI(TAG, "Init GT911");
 
        /* Initialize Touch Panel */
        ESP_LOGI(TAG, "Initialize touch IO (I2C)");
        const esp_lcd_touch_config_t tp_cfg = {
            .x_max = DISPLAY_WIDTH,
            .y_max = DISPLAY_HEIGHT,
            .rst_gpio_num = GPIO_NUM_NC, 
            .int_gpio_num = TOUCH_INT_GPIO, 
            .levels = {
                .reset = 0,
                .interrupt = 0,
            },
            .flags = {
                .swap_xy = 0,
                .mirror_x = 0,
                .mirror_y = 0,
            },
        };
        esp_lcd_panel_io_handle_t tp_io_handle = NULL;
        esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
        tp_io_config.dev_addr = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP; // 更改 GT911 地址 
        tp_io_config.scl_speed_hz = 100000;
        esp_lcd_new_panel_io_i2c(i2c_bus_, &tp_io_config, &tp_io_handle);
        esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &touch_);
    }

    void InitializeIli9881cDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        ESP_LOGI(TAG, "Turn on the power for MIPI DSI PHY");
        esp_ldo_channel_handle_t ldo_mipi_phy = NULL;
        esp_ldo_channel_config_t ldo_mipi_phy_config = {
            .chan_id = LCD_MIPI_DSI_PHY_PWR_LDO_CHAN,
            .voltage_mv = LCD_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV,
        };
        ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo_mipi_phy_config, &ldo_mipi_phy));

        ESP_LOGI(TAG, "Install MIPI DSI LCD control panel");
        esp_lcd_dsi_bus_handle_t mipi_dsi_bus;
        esp_lcd_dsi_bus_config_t bus_config = {
            .bus_id = 0,
            .num_data_lanes = 2,
            .lane_bit_rate_mbps = 900, // 900MHz
        };
        ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus));

        ESP_LOGI(TAG, "Install panel IO");
        esp_lcd_dbi_io_config_t dbi_config = {
            .virtual_channel = 0,
            .lcd_cmd_bits = 8,
            .lcd_param_bits  = 8,
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &panel_io));

        ESP_LOGI(TAG, "Install LCD driver of ili9881c");
        esp_lcd_dpi_panel_config_t dpi_config = {
            .virtual_channel = 0,
            .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
            .dpi_clock_freq_mhz = 60,
            .pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565,
            .num_fbs = 2,
            .video_timing = {
                .h_size = DISPLAY_WIDTH,
                .v_size = DISPLAY_HEIGHT,
                .hsync_pulse_width = 40,
                .hsync_back_porch  = 140,
                .hsync_front_porch = 40,
                .vsync_pulse_width = 4,
                .vsync_back_porch  = 20,
                .vsync_front_porch = 20,
            },
            .flags = {
                .use_dma2d = false,
            },
        };

        ili9881c_vendor_config_t vendor_config = {
            .init_cmds = tab5_lcd_ili9881c_specific_init_code_default,
            .init_cmds_size = sizeof(tab5_lcd_ili9881c_specific_init_code_default) / sizeof(tab5_lcd_ili9881c_specific_init_code_default[0]),
            .mipi_config = {
                .dsi_bus = mipi_dsi_bus,
                .dpi_config = &dpi_config,
                .lane_num = 2,
            },
        };

        esp_lcd_panel_dev_config_t lcd_dev_config = {};
        lcd_dev_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        lcd_dev_config.reset_gpio_num = -1;
        lcd_dev_config.bits_per_pixel = 16;
        lcd_dev_config.vendor_config = &vendor_config;

        ESP_ERROR_CHECK(esp_lcd_new_panel_ili9881c(panel_io, &lcd_dev_config, &panel));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

        display_ = new MipiLcdDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X,
                                      DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeSt7123Display() {
        esp_err_t ret = ESP_OK;
        esp_lcd_panel_io_handle_t io = NULL;
        esp_lcd_panel_handle_t disp_panel = NULL;
        esp_lcd_dsi_bus_handle_t mipi_dsi_bus = NULL;
        
        // Declare all config structures at the top to avoid goto issues
        // Initialize with memset to avoid any initialization syntax that might confuse the compiler
        esp_lcd_dsi_bus_config_t bus_config;
        esp_lcd_dbi_io_config_t dbi_config;
        esp_lcd_dpi_panel_config_t dpi_config;
        st7123_vendor_config_t vendor_config;
        esp_lcd_panel_dev_config_t lcd_dev_config;
        
        memset(&bus_config, 0, sizeof(bus_config));
        memset(&dbi_config, 0, sizeof(dbi_config));
        memset(&dpi_config, 0, sizeof(dpi_config));
        memset(&vendor_config, 0, sizeof(vendor_config));
        memset(&lcd_dev_config, 0, sizeof(lcd_dev_config));

        ESP_ERROR_CHECK(bsp_enable_dsi_phy_power());

        /* create MIPI DSI bus first, it will initialize the DSI PHY as well */
        bus_config.bus_id = 0;
        bus_config.num_data_lanes = 2;  // ST7123 uses 2 data lanes
        bus_config.lane_bit_rate_mbps = 965;  // ST7123 lane bitrate
        ret = esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "New DSI bus init failed");
            goto err;
        }

        ESP_LOGI(TAG, "Install MIPI DSI LCD control panel for ST7123");
        // we use DBI interface to send LCD commands and parameters
        dbi_config.virtual_channel = 0;
        dbi_config.lcd_cmd_bits = 8;  // according to the LCD spec
        dbi_config.lcd_param_bits = 8;  // according to the LCD spec
        ret = esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &io);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "New panel IO failed");
            goto err;
        }

        ESP_LOGI(TAG, "Install LCD driver of ST7123");
        dpi_config.virtual_channel = 0;
        dpi_config.dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT;
        dpi_config.dpi_clock_freq_mhz = 70;  // ST7123 DPI clock frequency
        dpi_config.pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565;
        dpi_config.num_fbs = 1;
        dpi_config.video_timing.h_size = 720;
        dpi_config.video_timing.v_size = 1280;
        dpi_config.video_timing.hsync_pulse_width = 2;
        dpi_config.video_timing.hsync_back_porch = 40;
        dpi_config.video_timing.hsync_front_porch = 40;
        dpi_config.video_timing.vsync_pulse_width = 2;
        dpi_config.video_timing.vsync_back_porch = 8;
        dpi_config.video_timing.vsync_front_porch = 220;
        dpi_config.flags.use_dma2d = true;

        vendor_config.init_cmds = st7123_vendor_specific_init_default;
        vendor_config.init_cmds_size = sizeof(st7123_vendor_specific_init_default) / sizeof(st7123_vendor_specific_init_default[0]);
        vendor_config.mipi_config.dsi_bus = mipi_dsi_bus;
        vendor_config.mipi_config.dpi_config = &dpi_config;
        vendor_config.mipi_config.lane_num = 2;

        lcd_dev_config.reset_gpio_num = -1;
        lcd_dev_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        lcd_dev_config.data_endian = LCD_RGB_DATA_ENDIAN_LITTLE;
        lcd_dev_config.bits_per_pixel = 24;
        lcd_dev_config.vendor_config = &vendor_config;

        // 使用实际的 ST7123 驱动函数
        ret = esp_lcd_new_panel_st7123(io, &lcd_dev_config, &disp_panel);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "New LCD panel ST7123 failed");
            goto err;
        }

        ret = esp_lcd_panel_reset(disp_panel);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "LCD panel reset failed");
            goto err;
        }

        ret = esp_lcd_panel_init(disp_panel);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "LCD panel init failed");
            goto err;
        }

        ret = esp_lcd_panel_disp_on_off(disp_panel, true);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "LCD panel display on failed");
            goto err;
        }

        display_ = new MipiLcdDisplay(io, disp_panel, 720, 1280, DISPLAY_OFFSET_X,
                                      DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);

        ESP_LOGI(TAG, "ST7123 Display initialized with resolution %dx%d", 720, 1280);

        return;

    err:
        if (disp_panel) {
            esp_lcd_panel_del(disp_panel);
        }
        if (io) {
            esp_lcd_panel_io_del(io);
        }
        if (mipi_dsi_bus) {
            esp_lcd_del_dsi_bus(mipi_dsi_bus);
        }
        ESP_ERROR_CHECK(ret);
    }

    void InitializeSt7123TouchPad() {
        ESP_LOGI(TAG, "Init ST7123 Touch");
        
        /* Initialize Touch Panel */
        ESP_LOGI(TAG, "Initialize touch IO (I2C)");
        const esp_lcd_touch_config_t tp_cfg = {
            .x_max = 720,
            .y_max = 1280,
            .rst_gpio_num = GPIO_NUM_NC,
            .int_gpio_num = TOUCH_INT_GPIO,
            .levels = {
                .reset = 0,
                .interrupt = 0,
            },
            .flags = {
                .swap_xy = 0,
                .mirror_x = 0,
                .mirror_y = 0,
            },
        };
        esp_lcd_panel_io_handle_t tp_io_handle = NULL;
        esp_lcd_panel_io_i2c_config_t tp_io_config = {
            .dev_addr = 0x55,
            .control_phase_bytes = 1,
            .dc_bit_offset = 0,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .scl_speed_hz = 100000,
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus_, &tp_io_config, &tp_io_handle));
        ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_st7123(tp_io_handle, &tp_cfg, &touch_));
    }

    void InitializeDisplay() {
        // after tp reset, wait for 100ms to ensure the I2C bus is stable
        vTaskDelay(pdMS_TO_TICKS(100));
        // 检测 ST7123 触摸屏 (I2C地址 0x55)
        esp_err_t ret = i2c_master_probe(i2c_bus_, ST7123_TOUCH_I2C_ADDRESS, 200);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Detected ST7123 at 0x%02X, initializing ST7123 display", ST7123_TOUCH_I2C_ADDRESS);
            InitializeSt7123Display();
            InitializeSt7123TouchPad();
        } else {
            ESP_LOGI(TAG, "ST7123 not found at 0x%02X (ret=0x%x), using default ST7703+GT911", ST7123_TOUCH_I2C_ADDRESS, ret);
            InitializeIli9881cDisplay();
            InitializeGt911TouchPad();
        }
    }

    void InitializeCamera() {
        esp_cam_sensor_xclk_handle_t xclk_handle = NULL;
        esp_cam_sensor_xclk_config_t cam_xclk_config = {};

#if CONFIG_CAMERA_XCLK_USE_ESP_CLOCK_ROUTER
        if (esp_cam_sensor_xclk_allocate(ESP_CAM_SENSOR_XCLK_ESP_CLOCK_ROUTER, &xclk_handle) == ESP_OK) {
            cam_xclk_config.esp_clock_router_cfg.xclk_pin = CAMERA_MCLK;
            cam_xclk_config.esp_clock_router_cfg.xclk_freq_hz = 12000000; // 12MHz
            (void)esp_cam_sensor_xclk_start(xclk_handle, &cam_xclk_config);
        }
#elif CONFIG_CAMERA_XCLK_USE_LEDC
        if (esp_cam_sensor_xclk_allocate(ESP_CAM_SENSOR_XCLK_LEDC, &xclk_handle) == ESP_OK) {
            cam_xclk_config.ledc_cfg.timer = LEDC_TIMER_0;
            cam_xclk_config.ledc_cfg.clk_cfg = LEDC_AUTO_CLK;
            cam_xclk_config.ledc_cfg.channel = LEDC_CHANNEL_0;
            cam_xclk_config.ledc_cfg.xclk_freq_hz = 12000000; // 12MHz
            cam_xclk_config.ledc_cfg.xclk_pin = CAMERA_MCLK;
            (void)esp_cam_sensor_xclk_start(xclk_handle, &cam_xclk_config);
        }
#endif

        esp_video_init_sccb_config_t sccb_config = {
            .init_sccb = false,
            .i2c_handle = i2c_bus_,
            .freq = 400000,
        };

        esp_video_init_csi_config_t csi_config = {
            .sccb_config = sccb_config,
            .reset_pin = GPIO_NUM_NC,
            .pwdn_pin = GPIO_NUM_NC,
        };

        esp_video_init_config_t video_config = {
            .csi = &csi_config,
        };

        camera_ = new Esp32Camera(video_config);
    }

public:
    M5StackTab5Board() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();
        I2cDetect();
        InitializePi4ioe();
        InitializeDisplay();  // Auto-detect and initialize display + touch
        InitializeCamera();
        InitializeButtons();
        SetChargeQcEn(true);
        SetChargeEn(true);
        SetUsb5vEn(true);
        SetExt5vEn(true);
        GetBacklight()->RestoreBrightness();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Tab5AudioCodec audio_codec(i2c_bus_, 
                                        AUDIO_INPUT_SAMPLE_RATE, 
                                        AUDIO_OUTPUT_SAMPLE_RATE,
                                        AUDIO_I2S_GPIO_MCLK, 
                                        AUDIO_I2S_GPIO_BCLK, 
                                        AUDIO_I2S_GPIO_WS,
                                        AUDIO_I2S_GPIO_DOUT, 
                                        AUDIO_I2S_GPIO_DIN, 
                                        AUDIO_CODEC_PA_PIN,
                                        AUDIO_CODEC_ES8388_ADDR, 
                                        AUDIO_CODEC_ES7210_ADDR, 
                                        AUDIO_INPUT_REFERENCE);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Camera* GetCamera() override {
        return camera_;
    }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    // BSP power control functions
    void SetChargeQcEn(bool en) {
        if (pi4ioe2_) {
            uint8_t value = pi4ioe2_->ReadOutSet();
            if (en) {
                clrbit(value, 5);  // P5 = CHG_QC_EN (低电平使能)
            } else {
                setbit(value, 5);
            }
            pi4ioe2_->WriteOutSet(value);
        }
    }

    void SetChargeEn(bool en) {
        if (pi4ioe2_) {
            uint8_t value = pi4ioe2_->ReadOutSet();
            if (en) {
                setbit(value, 7);  // P7 = CHG_EN
            } else {
                clrbit(value, 7);
            }
            pi4ioe2_->WriteOutSet(value);
        }
    }

    void SetUsb5vEn(bool en) {
        if (pi4ioe2_) {
            uint8_t value = pi4ioe2_->ReadOutSet();
            if (en) {
                setbit(value, 3);  // P3 = USB5V_EN
            } else {
                clrbit(value, 3);
            }
            pi4ioe2_->WriteOutSet(value);
        }
    }

    void SetExt5vEn(bool en) {
        if (pi4ioe1_) {
            uint8_t value = pi4ioe1_->ReadOutSet();
            if (en) {
                setbit(value, 2);  // P2 = EXT5V_EN
            } else {
                clrbit(value, 2);
            }
            pi4ioe1_->WriteOutSet(value);
        }
    }
};


DECLARE_BOARD(M5StackTab5Board);

