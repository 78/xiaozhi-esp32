#include "wifi_board.h"
#include "codecs/box_audio_codec.h"
#include "display/lcd_display.h"
#include "display/emote_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "i2c_device.h"
#include "esp32_camera.h"
#include "assets/lang_config.h"
#include "mcp_server.h"

#include <esp_log.h>
#include <esp_system.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <esp_lcd_touch_ft5x06.h>
#include <esp_lvgl_port.h>
#include <lvgl.h>
#include <thread>
#include <atomic>

#define TAG "LichuangDevBoard"

class Pca9557 : public I2cDevice {
public:
    Pca9557(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
        WriteReg(0x01, 0x03);
        WriteReg(0x03, 0xf8);
    }
    
    void SetOutputState(uint8_t bit, uint8_t level) {
        uint8_t data = ReadReg(0x01);
        data = (data & ~(1 << bit)) | (level << bit);
        WriteReg(0x01, data);
    }
};

class CustomAudioCodec : public BoxAudioCodec {
private:
    Pca9557* pca9557_;

public:
    CustomAudioCodec(i2c_master_bus_handle_t i2c_bus, Pca9557* pca9557) 
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
          pca9557_(pca9557) {
    }

    virtual void EnableOutput(bool enable) override {
        BoxAudioCodec::EnableOutput(enable);
        if (enable) {
            pca9557_->SetOutputState(1, 1);
        } else {
            pca9557_->SetOutputState(1, 0);
        }
    }
};

/**
 * @brief PCF8575 I/O 扩展芯片，地址为 0x20。
 */
class Pcf8575 : public I2cDevice {
private:
    uint16_t data_ = 0x0000;
    bool initialized_ = false;

public:
    Pcf8575(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
        // 复位所有位
        if (i2c_master_transmit(i2c_device_, (uint8_t*)&data_, 2, 100) != ESP_OK) {
            initialized_ = false;
        } else {
            initialized_ = true;
        }
    }

    void SetBit(uint8_t bit, uint8_t level) {
        if (initialized_) {
            data_ = (data_ & ~(1 << bit)) | (level << bit);
            ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_transmit(i2c_device_, (uint8_t*)&data_, 2, 100));
        }
    }

    bool IsInitialized() const {
        return initialized_;
    }
};

class LichuangDevBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    i2c_master_dev_handle_t pca9557_handle_;
    Button boot_button_;
    LcdDisplay* display_;
    Pca9557* pca9557_ = nullptr;
    Pcf8575* pcf8575_ = nullptr;
    Esp32Camera* camera_;
    std::atomic<bool> bed_operating_{false};

    /**
     * @brief 控制床的某个功能
     * @param bit 要控制的位
     * @param duration_ms 持续时间，单位毫秒
     * @return 返回值
     */
    ReturnValue ControlBed(int bit, int duration_ms = 12000) {
        if (bed_operating_) {
            throw std::runtime_error("Bed is already operating");
        }

        ESP_LOGI(TAG, "ControlBed(%d, %d)", bit, duration_ms);
        bed_operating_ = true;
        std::thread([this, bit, duration_ms]() {
            // High level to trigger
            pcf8575_->SetBit(bit, 1);
            // Duration in milliseconds
            int count = duration_ms / 100;
            for (int i = 0; i < count && bed_operating_; i++) {
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            // Low level to stop
            pcf8575_->SetBit(bit, 0);
            bed_operating_ = false;
        }).detach();

        return "{\"success\": true, \"message\": \"Bed is operating now\"}";
    }

    void InitializeTools() {
        // 初始化工具
        auto& mcp_server = McpServer::GetInstance();
        mcp_server.AddTool("bed.adjust", "床位调整\n"
            "Args: \n"
            "   action: 动作，支持以下动作：raise_back（升高靠背），lower_back（降低靠背），raise_leg（升高腿部），lower_leg（降低腿部），lean_left（靠左倾斜），lean_right（靠右倾斜）\n"
            "   full_adjust: 是否为完整调整（持续12秒），否则为单次调整\n",
            PropertyList({
                Property("action", kPropertyTypeString),
                Property("full_adjust", kPropertyTypeBoolean, false),
            }), [this](const PropertyList& properties) -> ReturnValue {
                auto action = properties["action"].value<std::string>();
                auto full_adjust = properties["full_adjust"].value<bool>();
                int duration_ms = 2000;
                if (full_adjust) {
                    duration_ms = 12000;
                }
                if (action == "raise_back") {
                    return ControlBed(0, duration_ms);
                } else if (action == "lower_back") {
                    return ControlBed(1, duration_ms);
                } else if (action == "raise_leg") {
                    return ControlBed(2, duration_ms);
                } else if (action == "lower_leg") {
                    return ControlBed(3, duration_ms);
                } else if (action == "lean_left") {
                    return ControlBed(4, duration_ms);
                } else if (action == "lean_right") {
                    return ControlBed(5, duration_ms);
                } else {
                    throw std::runtime_error("Invalid action: " + action);
                }
            });
        mcp_server.AddTool("bed.open_toilet", "便盆打开", PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
            return ControlBed(6);
        });
        mcp_server.AddTool("bed.close_toilet", "便盆关闭", PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
            return ControlBed(7);
        });
        mcp_server.AddTool("bed.auto_flip_a", "自动翻身A", PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
            return ControlBed(8, 1000);
        });
        mcp_server.AddTool("bed.auto_flip_b", "自动翻身B", PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
            return ControlBed(9, 1000);
        });
        mcp_server.AddTool("bed.stop", "停止操作。如用户要求停下来或取消当前操作，必须先调用后回答", PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
            if (!bed_operating_) {
                return "{\"success\": false, \"message\": \"No operation is in progress\"}";
            }
            bed_operating_ = false;
            return "{\"success\": true, \"message\": \"Operation cancelled\"}";
        });
    }

    void InitializeI2c() {
        // Initialize I2C peripheral
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

        // Initialize PCA9557
        pca9557_ = new Pca9557(i2c_bus_, 0x19);
        pcf8575_ = new Pcf8575(i2c_bus_, 0x20);
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
            auto& app = Application::GetInstance();
            // During startup (before connected), pressing BOOT button enters Wi-Fi config mode without reboot
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });

#if CONFIG_USE_DEVICE_AEC
        boot_button_.OnDoubleClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateIdle) {
                app.SetAecMode(app.GetAecMode() == kAecOff ? kAecOnDeviceSide : kAecOff);
            }
        });
#endif
    }

    void InitializeSt7789Display() {
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
        pca9557_->SetOutputState(0, 0);

        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, true);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        esp_lcd_panel_disp_on_off(panel, true);

#if CONFIG_USE_EMOTE_MESSAGE_STYLE
        display_ = new emote::EmoteDisplay(panel, panel_io, DISPLAY_WIDTH, DISPLAY_HEIGHT);
#else
        display_ = new SpiLcdDisplay(panel_io, panel,
            DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
#endif
    }

    void InitializeTouch()
    {
        esp_lcd_touch_handle_t tp;
        esp_lcd_touch_config_t tp_cfg = {
            .x_max = DISPLAY_HEIGHT,
            .y_max = DISPLAY_WIDTH,
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

        if(touch_cfg.disp) {
            lvgl_port_add_touch(&touch_cfg);
        } else {
            ESP_LOGE(TAG, "Touch display is not initialized");
        }
    }

    void InitializeCamera() {
        // Open camera power
        pca9557_->SetOutputState(2, 0);

        static esp_cam_ctlr_dvp_pin_config_t dvp_pin_config = {
            .data_width = CAM_CTLR_DATA_WIDTH_8,
            .data_io = {
                [0] = CAMERA_PIN_D0,
                [1] = CAMERA_PIN_D1,
                [2] = CAMERA_PIN_D2,
                [3] = CAMERA_PIN_D3,
                [4] = CAMERA_PIN_D4,
                [5] = CAMERA_PIN_D5,
                [6] = CAMERA_PIN_D6,
                [7] = CAMERA_PIN_D7,
            },
            .vsync_io = CAMERA_PIN_VSYNC,
            .de_io = CAMERA_PIN_HREF,
            .pclk_io = CAMERA_PIN_PCLK,
            .xclk_io = CAMERA_PIN_XCLK,
        };

        esp_video_init_sccb_config_t sccb_config = {
            .init_sccb = false,
            .i2c_handle = i2c_bus_,
            .freq = 100000,
        };

        esp_video_init_dvp_config_t dvp_config = {
            .sccb_config = sccb_config,
            .reset_pin = CAMERA_PIN_RESET,
            .pwdn_pin = CAMERA_PIN_PWDN,
            .dvp_pin = dvp_pin_config,
            .xclk_freq = XCLK_FREQ_HZ,
        };

        esp_video_init_config_t video_config = {
            .dvp = &dvp_config,
        };

        camera_ = new Esp32Camera(video_config);
    }

public:
    LichuangDevBoard() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();
        InitializeSpi();
        InitializeSt7789Display();
        InitializeTouch();
        InitializeButtons();
        InitializeCamera();
        GetBacklight()->RestoreBrightness();

        if (pcf8575_->IsInitialized()) {
            InitializeTools();
        } else {
            // PCF8575 initialization failed, show error and reboot after 30 seconds
            ESP_LOGE(TAG, "PCF8575 initialization failed, will reboot in 30 seconds");
            display_->SetStatus(Lang::Strings::ERROR);
            display_->SetEmotion("triangle_exclamation");
            display_->SetChatMessage("system", "PCF8575 not connected\nReboot in 30s...");
            vTaskDelay(pdMS_TO_TICKS(30000));
            esp_restart();
        }
    }

    virtual AudioCodec* GetAudioCodec() override {
        static CustomAudioCodec audio_codec(
            i2c_bus_, 
            pca9557_);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }
    
    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    virtual Camera* GetCamera() override {
        return camera_;
    }
};

DECLARE_BOARD(LichuangDevBoard);
