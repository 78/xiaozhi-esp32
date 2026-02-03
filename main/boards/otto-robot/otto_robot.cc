#include <driver/i2c_master.h>
#include <driver/ledc.h>
#include <driver/spi_common.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>

#include "application.h"
#include "button.h"
#include "codecs/no_audio_codec.h"
#include "config.h"
#include "display/lcd_display.h"
#include "esp_video.h"
#include "lamp_controller.h"
#include "led/single_led.h"
#include "mcp_server.h"
#include "otto_emoji_display.h"
#include "power_manager.h"
#include "system_reset.h"
#include "websocket_control_server.h"
#include "wifi_board.h"

#define TAG "OttoRobot"

extern void InitializeOttoController(const HardwareConfig& hw_config);

class OttoRobot : public WifiBoard {
private:
    LcdDisplay* display_;
    PowerManager* power_manager_;
    Button boot_button_;
    WebSocketControlServer* ws_control_server_;
    HardwareConfig hw_config_;
    AudioCodec* audio_codec_;
    i2c_master_bus_handle_t i2c_bus_;
    EspVideo* camera_;
    bool has_camera_;
    OttoCameraType camera_type_;

    bool DetectHardwareVersion() {
        ledc_timer_config_t ledc_timer = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .duty_resolution = LEDC_TIMER_2_BIT,
            .timer_num = LEDC_TIMER,
            .freq_hz = CAMERA_XCLK_FREQ,
            .clk_cfg = LEDC_AUTO_CLK,
        };
        esp_err_t ret = ledc_timer_config(&ledc_timer);
        if (ret != ESP_OK) {
            return false;
        }

        ledc_channel_config_t ledc_channel = {
            .gpio_num = CAMERA_XCLK,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = LEDC_CHANNEL,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = LEDC_TIMER,
            .duty = 2,
            .hpoint = 0,
        };
        ret = ledc_channel_config(&ledc_channel);
        if (ret != ESP_OK) {
            return false;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = CAMERA_VERSION_CONFIG.i2c_sda_pin,
            .scl_io_num = CAMERA_VERSION_CONFIG.i2c_scl_pin,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags =
                {
                    .enable_internal_pullup = 1,
                },
        };

        ret = i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_);
        if (ret != ESP_OK) {
            ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL, 0);
            return false;
        }
        const uint8_t camera_addresses[] = {0x30, 0x3C, 0x21, 0x60};
        bool camera_found = false;
        uint16_t detected_pid = 0;

        for (size_t i = 0; i < sizeof(camera_addresses); i++) {
            uint8_t addr = camera_addresses[i];
            i2c_device_config_t dev_cfg = {
                .dev_addr_length = I2C_ADDR_BIT_LEN_7,
                .device_address = addr,
                .scl_speed_hz = 100000,
            };

            i2c_master_dev_handle_t dev_handle;
            ret = i2c_master_bus_add_device(i2c_bus_, &dev_cfg, &dev_handle);
            if (ret == ESP_OK) {
                uint8_t data[2] = {0, 0};

                uint8_t reg_addr_8bit = 0x0A;
                ret = i2c_master_transmit_receive(dev_handle, &reg_addr_8bit, 1, data, 2, 200);
                if (ret == ESP_OK && (data[0] != 0 || data[1] != 0)) {
                    detected_pid = (data[0] << 8) | data[1];
                    ESP_LOGI(TAG, "检测到摄像头 (OV2640方式) PID=0x%04X (地址=0x%02X)",
                             detected_pid, addr);
                    camera_found = true;
                    i2c_master_bus_rm_device(dev_handle);
                    break;
                }

                uint8_t reg_addr_high[2] = {0x30, 0x0A};
                uint8_t reg_addr_low[2] = {0x30, 0x0B};
                uint8_t pid_high = 0, pid_low = 0;

                ret = i2c_master_transmit_receive(dev_handle, reg_addr_high, 2, &pid_high, 1, 200);
                if (ret == ESP_OK) {
                    ret =
                        i2c_master_transmit_receive(dev_handle, reg_addr_low, 2, &pid_low, 1, 200);
                    if (ret == ESP_OK) {
                        detected_pid = (pid_high << 8) | pid_low;
                        if (detected_pid != 0) {
                            ESP_LOGI(TAG, "检测到摄像头 (OV3660方式) PID=0x%04X (地址=0x%02X)",
                                     detected_pid, addr);
                            camera_found = true;
                            i2c_master_bus_rm_device(dev_handle);
                            break;
                        }
                    }
                }

                i2c_master_bus_rm_device(dev_handle);
            }
        }

        if (!camera_found) {
            i2c_del_master_bus(i2c_bus_);
            i2c_bus_ = nullptr;
            ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL, 0);
            camera_type_ = OTTO_CAMERA_NONE;
        } else {
            // 根据 PID 判断摄像头类型
            if (detected_pid == OV2640_PID_1 || detected_pid == OV2640_PID_2) {
                camera_type_ = OTTO_CAMERA_OV2640;
                ESP_LOGI(TAG, "摄像头类型: OV2640 (PID=0x%04X)", detected_pid);
            } else if (detected_pid == OV3660_PID) {
                camera_type_ = OTTO_CAMERA_OV3660;
                ESP_LOGI(TAG, "摄像头类型: OV3660 (PID=0x%04X)", detected_pid);
            } else {
                camera_type_ = OTTO_CAMERA_UNKNOWN;
                ESP_LOGW(TAG, "未知摄像头类型，PID=0x%04X", detected_pid);
            }
        }
        return camera_found;
    }

    void InitializePowerManager() {
        power_manager_ = new PowerManager(hw_config_.power_charge_detect_pin,
                                          hw_config_.power_adc_unit, hw_config_.power_adc_channel);
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = hw_config_.display_mosi_pin;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = hw_config_.display_clk_pin;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeLcdDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = hw_config_.display_cs_pin;
        io_config.dc_gpio_num = hw_config_.display_dc_pin;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = hw_config_.display_rst_pin;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;

        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));

        esp_lcd_panel_reset(panel);

        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);

        display_ = new OttoEmojiDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                        DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X,
                                        DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
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

    void InitializeOttoController() { ::InitializeOttoController(hw_config_); }

public:
    const HardwareConfig& GetHardwareConfig() const { return hw_config_; }

    OttoCameraType GetCameraType() const { return camera_type_; }

private:
    void InitializeWebSocketControlServer() {
        ws_control_server_ = new WebSocketControlServer();
        if (!ws_control_server_->Start(8080)) {
            delete ws_control_server_;
            ws_control_server_ = nullptr;
        }
    }

    void StartNetwork() override {
        WifiBoard::StartNetwork();
        vTaskDelay(pdMS_TO_TICKS(1000));

        InitializeWebSocketControlServer();
    }

    bool InitializeCamera() {
        if (!has_camera_ || i2c_bus_ == nullptr) {
            return false;
        }

        try {
            static esp_cam_ctlr_dvp_pin_config_t dvp_pin_config = {
                .data_width = CAM_CTLR_DATA_WIDTH_8,
                .data_io =
                    {
                        [0] = CAMERA_D0,
                        [1] = CAMERA_D1,
                        [2] = CAMERA_D2,
                        [3] = CAMERA_D3,
                        [4] = CAMERA_D4,
                        [5] = CAMERA_D5,
                        [6] = CAMERA_D6,
                        [7] = CAMERA_D7,
                    },
                .vsync_io = CAMERA_VSYNC,
                .de_io = CAMERA_HSYNC,
                .pclk_io = CAMERA_PCLK,
                .xclk_io = CAMERA_XCLK,
            };

            esp_video_init_sccb_config_t sccb_config = {
                .init_sccb = false,
                .i2c_handle = i2c_bus_,
                .freq = 100000,
            };

            esp_video_init_dvp_config_t dvp_config = {
                .sccb_config = sccb_config,
                .reset_pin = CAMERA_RESET,
                .pwdn_pin = CAMERA_PWDN,
                .dvp_pin = dvp_pin_config,
                .xclk_freq = CAMERA_XCLK_FREQ,
            };

            esp_video_init_config_t video_config = {
                .dvp = &dvp_config,
            };

            camera_ = new EspVideo(video_config);

            // 根据摄像头类型设置不同的翻转参数
            switch (camera_type_) {
                case OTTO_CAMERA_OV3660:
                    camera_->SetVFlip(true);
                    camera_->SetHMirror(true);
                    ESP_LOGI(TAG, "OV3660: 设置 VFlip=true, HMirror=true");
                    break;
                case OTTO_CAMERA_OV2640:
                default:
                    camera_->SetVFlip(true);
                    camera_->SetHMirror(false);
                    ESP_LOGI(TAG, "OV2640: 设置 VFlip=true, HMirror=false");
                    break;
            }
            return true;
        } catch (...) {
            camera_ = nullptr;
            return false;
        }
    }

    void InitializeAudioCodec() {
        if (hw_config_.audio_use_simplex) {
            audio_codec_ = new NoAudioCodecSimplex(
                hw_config_.audio_input_sample_rate, hw_config_.audio_output_sample_rate,
                hw_config_.audio_i2s_spk_gpio_bclk, hw_config_.audio_i2s_spk_gpio_lrck,
                hw_config_.audio_i2s_spk_gpio_dout, hw_config_.audio_i2s_mic_gpio_sck,
                hw_config_.audio_i2s_mic_gpio_ws, hw_config_.audio_i2s_mic_gpio_din);
        } else {
            audio_codec_ = new NoAudioCodecDuplex(
                hw_config_.audio_input_sample_rate, hw_config_.audio_output_sample_rate,
                hw_config_.audio_i2s_gpio_bclk, hw_config_.audio_i2s_gpio_ws,
                hw_config_.audio_i2s_gpio_dout, hw_config_.audio_i2s_gpio_din);
        }
    }

public:
    OttoRobot()
        : boot_button_(BOOT_BUTTON_GPIO),
          audio_codec_(nullptr),
          i2c_bus_(nullptr),
          camera_(nullptr),
          has_camera_(false),
          camera_type_(OTTO_CAMERA_NONE) {
#if OTTO_HARDWARE_VERSION == OTTO_VERSION_AUTO
        // 自动检测硬件版本（同时检测摄像头类型）
        has_camera_ = DetectHardwareVersion();
        ESP_LOGI(TAG, "自动检测硬件版本: %s", has_camera_ ? "摄像头版" : "无摄像头版");
#elif OTTO_HARDWARE_VERSION == OTTO_VERSION_CAMERA
        // 强制使用摄像头版本，但仍检测具体摄像头类型
        has_camera_ = DetectHardwareVersion();
        if (!has_camera_) {
            // 检测失败时仍使用摄像头配置，但不知道具体类型
            has_camera_ = true;
            camera_type_ = OTTO_CAMERA_UNKNOWN;
            ESP_LOGW(TAG, "强制使用摄像头版本配置，但未能检测到摄像头类型");
            // 初始化 I2C 总线用于摄像头
            i2c_master_bus_config_t i2c_bus_cfg = {
                .i2c_port = I2C_NUM_0,
                .sda_io_num = CAMERA_VERSION_CONFIG.i2c_sda_pin,
                .scl_io_num = CAMERA_VERSION_CONFIG.i2c_scl_pin,
                .clk_source = I2C_CLK_SRC_DEFAULT,
                .glitch_ignore_cnt = 7,
                .intr_priority = 0,
                .trans_queue_depth = 0,
                .flags =
                    {
                        .enable_internal_pullup = 1,
                    },
            };
            i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_);
        } else {
            ESP_LOGI(TAG, "强制使用摄像头版本配置");
        }
#elif OTTO_HARDWARE_VERSION == OTTO_VERSION_NO_CAMERA
        // 强制使用无摄像头版本
        has_camera_ = false;
        camera_type_ = OTTO_CAMERA_NONE;
        ESP_LOGI(TAG, "强制使用无摄像头版本配置");
#else
#error \
    "OTTO_HARDWARE_VERSION 设置无效，请使用 OTTO_VERSION_AUTO, OTTO_VERSION_CAMERA 或 OTTO_VERSION_NO_CAMERA"
#endif

        if (has_camera_)
            hw_config_ = CAMERA_VERSION_CONFIG;
        else
            hw_config_ = NON_CAMERA_VERSION_CONFIG;

        InitializeSpi();
        InitializeLcdDisplay();
        InitializeButtons();
        InitializePowerManager();
        InitializeAudioCodec();

        if (has_camera_) {
            if (!InitializeCamera()) {
                has_camera_ = false;
            }
        }

        InitializeOttoController();
        ws_control_server_ = nullptr;
        GetBacklight()->RestoreBrightness();
    }

    virtual AudioCodec* GetAudioCodec() override { return audio_codec_; }

    virtual Display* GetDisplay() override { return display_; }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight* backlight = nullptr;
        if (backlight == nullptr) {
            backlight =
                new PwmBacklight(hw_config_.display_backlight_pin, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        }
        return backlight;
    }

    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        charging = power_manager_->IsCharging();
        discharging = !charging;
        level = power_manager_->GetBatteryLevel();
        return true;
    }

    virtual Camera* GetCamera() override { return has_camera_ ? camera_ : nullptr; }
};

DECLARE_BOARD(OttoRobot);
