#include "wifi_board.h"
#include "codecs/es8311_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "settings.h"

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <cstring>

#include <nvs_flash.h>
#include "system_reset.h"

#include "esp32_camera.h"

#define TAG "chd_esp32s3_eye"

class ChdEsp32s3EyeBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    Button boot_button_;
    Display* display_;
    Esp32Camera* camera_;

    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)BSP_I2C_PORT,
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

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_GPIO;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_CLK_GPIO;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void ResetNvsFlash() {
        ESP_LOGI(TAG, "Resetting NVS flash");
        esp_err_t ret = nvs_flash_erase();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to erase NVS flash");
        }
        ret = nvs_flash_init();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize NVS flash");
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        esp_restart();
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
        boot_button_.OnMultipleClick([this]() {
            ESP_LOGD(TAG, "OnMultipleClick");
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateWifiConfiguring) {      // 测试相机，测试完要重启一次
                if (camera_&& camera_->Capture()) {
                    ESP_LOGI(TAG, "chd camera capture OK");
                }
                else {
                    ESP_LOGE(TAG, "chd camera capture ERROR");
                }
            }
            else {
                ResetNvsFlash();
            }
            ESP_LOGD(TAG, "OnMultipleClick");
         }, 5);
    }

    void InitializeDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_GPIO;
        io_config.dc_gpio_num = DISPLAY_DC_GPIO;
        io_config.spi_mode = 0;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片
        ESP_LOGD(TAG, "Install LCD driver");

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));

        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, true);
        esp_lcd_panel_disp_on_off(panel, true);
        display_ = new SpiLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeCamera() {
        camera_config_t camera_config = {
            .pin_pwdn     = CAMERA_PIN_PWDN,
            .pin_reset    = CAMERA_PIN_RESET,
            .pin_xclk     = CAMERA_PIN_XCLK,
            .pin_sscb_sda = BSP_I2C_SDA,
            .pin_sscb_scl = BSP_I2C_SCL,

            .pin_d7    = CAMERA_PIN_D7,
            .pin_d6    = CAMERA_PIN_D6,
            .pin_d5    = CAMERA_PIN_D5,
            .pin_d4    = CAMERA_PIN_D4,
            .pin_d3    = CAMERA_PIN_D3,
            .pin_d2    = CAMERA_PIN_D2,
            .pin_d1    = CAMERA_PIN_D1,
            .pin_d0    = CAMERA_PIN_D0,
            .pin_vsync = CAMERA_PIN_VSYNC,
            .pin_href  = CAMERA_PIN_HSYNC,
            .pin_pclk  = CAMERA_PIN_PCLK,

            .xclk_freq_hz = CAMERA_XCLK_FREQ,
            .ledc_timer   = LEDC_TIMER_2,
            .ledc_channel = LEDC_CHANNEL_0,
            .pixel_format = PIXFORMAT_RGB565,
            .frame_size   = FRAMESIZE_VGA,//FRAMESIZE_240X240,

            // .pixel_format = PIXFORMAT_JPEG,
            // .frame_size   = FRAMESIZE_QXGA,     // 2048x1536 FRAMESIZE_UXGA,     // 1600x1200  FRAMESIZE_UXGA,     // 1600x1200        FRAMESIZE_QVGA,     // 320x240
            .jpeg_quality = 8,
            .fb_count     = 1,
            .fb_location  = CAMERA_FB_IN_PSRAM,
            .grab_mode    = CAMERA_GRAB_WHEN_EMPTY,
            .sccb_i2c_port = BSP_I2C_PORT,
        };

        // initialize the camera sensor
        camera_ = new Esp32Camera(camera_config);
        // Get the sensor object, and then use some of its functions to adjust the parameters when taking a photo.
        // Note: Do not call functions that set resolution, set picture format and PLL clock,
        // If you need to reset the appeal parameters, please reinitialize the sensor.
        sensor_t* s = esp_camera_sensor_get();
        if (s == NULL) {
            ESP_LOGE(TAG, "chd camera esp_camera_sensor_get ERROR");
            return;
        }
        // initial sensors are flipped vertically and colors are a bit saturated
        if (s->id.PID == OV3660_PID) {
            s->set_brightness(s, 1);   // up the blightness just a bit
            s->set_saturation(s, -2);  // lower the saturation
        }

        Settings settings("chd_esp_eye", false); // 考虑有的批次摄像头需要翻转
        bool camera_flipped = static_cast<bool>(settings.GetInt("camera-flipped", 0));
        camera_->SetHMirror(camera_flipped);
        camera_->SetVFlip(camera_flipped);

        camera_fb_t *fb = esp_camera_fb_get(); // 第一帧的数据不使用
        if (fb) {
            ESP_LOGI(TAG, "chd camera fb get OK");
            esp_camera_fb_return(fb);
        }
        else {
            ESP_LOGE(TAG, "chd camera fb get ERROR");
        }
    }

    void InitializeTools() {
        auto& mcp_server = McpServer::GetInstance();

        mcp_server.AddTool("self.camera.set_camera_flipped", "翻转摄像头图像方向", PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
            Settings settings("chd_esp_eye", true);
            // 考虑到部分复刻使用了不可动摄像头的设计，默认启用翻转
            bool flipped = !static_cast<bool>(settings.GetInt("camera-flipped", 1));
            camera_->SetHMirror(flipped);
            camera_->SetVFlip(flipped);
            settings.SetInt("camera-flipped", flipped ? 1 : 0);
            return true;
        });
    }

    void I2cDetect() {
        uint8_t address;
        ESP_LOGI(TAG, "I2C device scan:");
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

public:
    ChdEsp32s3EyeBoard() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();
        // I2cDetect();
        InitializeSpi();
        InitializeDisplay();
        InitializeButtons();
        InitializeCamera();
        InitializeTools();
        GetBacklight()->RestoreBrightness();
    }

    virtual AudioCodec* GetAudioCodec() override {
         static Es8311AudioCodec audio_codec(i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
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

DECLARE_BOARD(ChdEsp32s3EyeBoard);
