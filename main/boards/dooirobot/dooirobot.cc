#include "wifi_board.h"
#include "codecs/es8311_audio_codec.h"
#include "display/display.h"

#include "esp_video_init.h"
#include "esp_cam_sensor_xclk.h"

#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"

#include "esp_ldo_regulator.h" 
#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>

#include "dooi_audio_codec.h"
#include "dooi_camera.h"
#include "dooi_display.h"
#include "app_ui.h"
#include "app_periphral.h"
#include "app_mcp_tool.h"
#include "app_ui_logic.h"
#include "esp_private/brownout.h"


#define TAG "DooiRobotBoard"

/* I2C0 总线句柄 挂多个设备 */
i2c_master_bus_handle_t i2c_bus_;

class WifiBoard;
static WifiBoard* g_board = nullptr;
extern "C" void dooi_enter_wifi_config_mode(void)
{
    if (g_board) {
        g_board->EnterWifiConfigMode();
    } else {
        ESP_LOGW(TAG, "board not ready");
    }
}
extern "C" void dooi_trigger_chat(void)
{
    auto& app = Application::GetInstance();
    app.ToggleChatState();
}

class DooiRobotBoard : public WifiBoard {
private:
    Button boot_button_;
    DooiCamera* camera_ = nullptr;
    Display* display_ = new DooiDisplay();
    PwmBacklight* backlight_ = nullptr;

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

    void InitializeCamera() {
        gpio_config_t io = {
            .pin_bit_mask = 1ULL << CAMERA_PIN_PWDN,
            .mode = GPIO_MODE_OUTPUT,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&io));
        vTaskDelay(pdMS_TO_TICKS(10)); 
        gpio_set_level(CAMERA_PIN_PWDN, 1);

        vTaskDelay(pdMS_TO_TICKS(100));      // 等待摄像头上电稳定

        esp_video_init_csi_config_t base_csi_config = {
            .sccb_config = {
                .init_sccb = false,
                .i2c_handle = i2c_bus_,
                .freq = 100000,
            },
            .reset_pin = GPIO_NUM_NC,
            .pwdn_pin  = GPIO_NUM_NC,
        };

        esp_video_init_config_t cam_config = {
            .csi = &base_csi_config,
        };

        camera_ = new DooiCamera(cam_config);
    }

    esp_err_t bsp_enable_sdio_power(void)
    {
        static esp_ldo_channel_handle_t sdio_chan = NULL;
        esp_ldo_channel_config_t ldo_cfg = {
            .chan_id = 4,
            .voltage_mv = 3300,
        };
        esp_ldo_acquire_channel(&ldo_cfg, &sdio_chan);
        ESP_LOGI(TAG, "SDIO Powered on");

        return ESP_OK;
    }

public:
    DooiRobotBoard() : boot_button_(BOOT_BUTTON_GPIO)
    {
        g_board = this;

        bsp_enable_sdio_power(); // 开启SDIO电源，否则wifi无法正常工作； For gpio 组电压

        backlight_ = new PwmBacklight(LCD_PIN_BL, false);
        backlight_->RestoreBrightness();

        InitializeI2c();
        InitializeButtons();
        InitializeCamera();

        app_mcp_tool_register();
        app_periphral_init();
        app_ui_init();
        app_ui_logic_init();
        
        // app_ble_control_start();
        esp_brownout_disable();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static DooiAudioCodec audio_codec(
            i2c_bus_,
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK,
            AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN,
            AUDIO_CODEC_ES8311_ADDR,
            AUDIO_CODEC_ES7243E_ADDR,
            AUDIO_INPUT_REFERENCE);

        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Camera* GetCamera() override {
        return camera_;
    }

    virtual Backlight* GetBacklight() override
    {
        return backlight_;
    }

    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        charging = power_manager_is_charging();
        discharging = !charging;
        level = power_manager_get_level();
        return true;
    }
};

DECLARE_BOARD(DooiRobotBoard);