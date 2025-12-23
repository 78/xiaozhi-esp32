#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <driver/ledc.h>

#include "display/lcd_display.h"
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>

#include "assets/lang_config.h"
#include "anim_player.h"
// #include "emoji_display.h"
#include "led_strip.h"
#include "driver/rmt_tx.h"
#include "device_state_event.h"

#include "sdkconfig.h"
#include "puppy_movements.h"

#ifdef CONFIG_ESP_HI_WEB_CONTROL_ENABLED
#include "esp_hi_web_control.h"
#endif // CONFIG_ESP_HI_WEB_CONTROL_ENABLED

#define TAG "ESP_PUPPY_S3"

// Tail Servo Config
#define TAIL_SERVO_TIMER LEDC_TIMER_0
#define TAIL_SERVO_MODE LEDC_LOW_SPEED_MODE
#define TAIL_SERVO_CHANNEL LEDC_CHANNEL_0
#define TAIL_SERVO_DUTY_RES LEDC_TIMER_13_BIT // Set duty resolution to 13 bits
#define TAIL_SERVO_FREQUENCY 50               // Frequency in Hertz. Set frequency at 50 Hz

static const led_strip_config_t bsp_strip_config = {
    .strip_gpio_num = GPIO_NUM_48, // Onboard RGB LED on S3 DevKit
    .max_leds = 1,
    .led_model = LED_MODEL_WS2812,
    .flags = {
        .invert_out = false}};

static const led_strip_rmt_config_t bsp_rmt_config = {
    .clk_src = RMT_CLK_SRC_DEFAULT,
    .resolution_hz = 10 * 1000 * 1000,
    .flags = {
        .with_dma = false}};

struct OttoCommand
{
    int type; // 0: Walk, 1: Turn, 2: Home, 3: Stop
    int steps;
    int period;
    int dir;
};

class EspPuppyS3 : public WifiBoard
{
private:
    Button boot_button_;
    Button audio_wake_button_;
    SpiLcdDisplay *display_ = nullptr;
    bool web_server_initialized_ = false;
    led_strip_handle_t led_strip_;
    bool led_on_ = false;
    Puppy puppy_;
    QueueHandle_t puppy_queue_;

#ifdef CONFIG_ESP_HI_WEB_CONTROL_ENABLED
    static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data)
    {
        // if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED)
        // {

        //     xTaskCreate(
        //         [](void *arg)
        //         {
        //             EspPuppyS3 *instance = static_cast<EspPuppyS3 *>(arg);

        //             vTaskDelay(5000 / portTICK_PERIOD_MS);

        //             if (!instance->web_server_initialized_)
        //             {
        //                 ESP_LOGI(TAG, "WiFi connected, init web control server");
        //                 esp_err_t err = esp_hi_web_control_server_init();
        //                 if (err != ESP_OK)
        //                 {
        //                     ESP_LOGE(TAG, "Failed to initialize web control server: %d", err);
        //                 }
        //                 else
        //                 {
        //                     ESP_LOGI(TAG, "Web control server initialized");
        //                     instance->web_server_initialized_ = true;
        //                 }
        //             }

        //             vTaskDelete(NULL);
        //         },
        //         "web_server_init",
        //         1024 * 10, arg, 5, nullptr);
        // }
    }
#endif // CONFIG_ESP_HI_WEB_CONTROL_ENABLED

    void InitializeButtons()
    {
        boot_button_.OnClick([this]()
                             {
            auto &app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState(); });

        audio_wake_button_.OnPressDown([this]() {});

        audio_wake_button_.OnPressUp([this]() {});
    }

    void InitializeLed()
    {
        ESP_LOGI(TAG, "BLINK_GPIO setting %d", bsp_strip_config.strip_gpio_num);

        ESP_ERROR_CHECK(led_strip_new_rmt_device(&bsp_strip_config, &bsp_rmt_config, &led_strip_));
        led_strip_set_pixel(led_strip_, 0, 0x00, 0x00, 0x00);
        led_strip_refresh(led_strip_);
    }

    esp_err_t SetLedColor(uint8_t r, uint8_t g, uint8_t b)
    {
        esp_err_t ret = ESP_OK;
        ret |= led_strip_set_pixel(led_strip_, 0, r, g, b);
        ret |= led_strip_refresh(led_strip_);
        return ret;
    }

    void InitializeIot()
    {
        ESP_LOGI(TAG, "Initialize Iot");
        InitializeLed();
        SetLedColor(0x00, 0x00, 0x00);

#ifdef CONFIG_ESP_HI_WEB_CONTROL_ENABLED
        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED,
                                                   &wifi_event_handler, this));
#endif // CONFIG_ESP_HI_WEB_CONTROL_ENABLED
    }

    void InitializeSpi()
    {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_SDA;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_SCL;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeLcdDisplay()
    {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        // LCD IO Init
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS;
        io_config.dc_gpio_num = DISPLAY_DC;
        io_config.spi_mode = 3;
        io_config.pclk_hz = 80 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        // LCD Driver Init
        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RES;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));

        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));
        ESP_LOGI(TAG, "LCD panel create success, %p", panel);

        display_ = new SpiLcdDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);

        // Initialize Puppy
        puppy_.Init(FL_GPIO_NUM, FR_GPIO_NUM, BL_GPIO_NUM, BR_GPIO_NUM, TAIL_GPIO_NUM);
        puppy_.Home();

        // Create Puppy Task
        puppy_queue_ = xQueueCreate(10, sizeof(OttoCommand));
        xTaskCreate([](void *arg)
                    {
            EspPuppyS3 *instance = static_cast<EspPuppyS3 *>(arg);
            OttoCommand cmd;
            while (true) {
                if (xQueueReceive(instance->puppy_queue_, &cmd, portMAX_DELAY)) {
                    if (cmd.type == 0) { // Walk
                        instance->puppy_.Walk(cmd.steps, cmd.period, cmd.dir);
                    } else if (cmd.type == 1) { // Turn
                        instance->puppy_.Turn(cmd.steps, cmd.period, cmd.dir);
                    } else if (cmd.type == 2) { // Home
                        instance->puppy_.Home();
                    } else if (cmd.type == 3) { // Stop
                        instance->puppy_.Home();
                    }
                }
            } }, "PuppyTask", 4096, this, 5, NULL);

        StartupAnimation();
    }

    void StartupAnimation()
    {
        xTaskCreate([](void *arg)
                    {
            EspPuppyS3 *instance = static_cast<EspPuppyS3 *>(arg);
            
            instance->puppy_.Home();
            vTaskDelay(pdMS_TO_TICKS(500));
            instance->puppy_.Jump(1, 2000);
            vTaskDelay(pdMS_TO_TICKS(500));
            instance->puppy_.WagTail(500, 30);
            
            ESP_LOGI(TAG, "Startup animation finished");
            vTaskDelete(NULL); }, "StartupAnim", 4096, this, 5, NULL);
    }

    void InitializeTools()
    {
        auto &mcp_server = McpServer::GetInstance();

        // Basic Control
        mcp_server.AddTool("self.dog.basic_control", "Basic robot actions: forward, backward, turn_left, turn_right, stop, wag_tail",
                           PropertyList({
                               Property("action", kPropertyTypeString),
                           }),
                           [this](const PropertyList &properties) -> ReturnValue
                           {
                               const std::string &action = properties["action"].value<std::string>();
                               OttoCommand cmd;
                               if (action == "forward")
                               {
                                   cmd.type = 0;
                                   cmd.steps = 4;
                                   cmd.period = 1000;
                                   cmd.dir = FORWARD;
                                   xQueueSend(puppy_queue_, &cmd, 0);
                               }
                               else if (action == "backward")
                               {
                                   cmd.type = 0;
                                   cmd.steps = 4;
                                   cmd.period = 1000;
                                   cmd.dir = BACKWARD;
                                   xQueueSend(puppy_queue_, &cmd, 0);
                               }
                               else if (action == "turn_left")
                               {
                                   cmd.type = 1;
                                   cmd.steps = 4;
                                   cmd.period = 1000;
                                   cmd.dir = LEFT;
                                   xQueueSend(puppy_queue_, &cmd, 0);
                               }
                               else if (action == "turn_right")
                               {
                                   cmd.type = 1;
                                   cmd.steps = 4;
                                   cmd.period = 1000;
                                   cmd.dir = RIGHT;
                                   xQueueSend(puppy_queue_, &cmd, 0);
                               }
                               else if (action == "stop")
                               {
                                   cmd.type = 3;
                                   xQueueSend(puppy_queue_, &cmd, 0);
                               }
                               else if (action == "wag_tail")
                               {
                                   puppy_.WagTail(500, 30);
                               }
                               else
                               {
                                   return false;
                               }
                               return true;
                           });

        // Tail Control
        mcp_server.AddTool("self.dog.tail_control", "Control the tail servo angle (0-180)",
                           PropertyList({
                               Property("angle", kPropertyTypeInteger, 0, 180),
                           }),
                           [this](const PropertyList &properties) -> ReturnValue
                           {
                               int angle = properties["angle"].value<int>();
                               // Map 0-180 to -90 to 90
                               int pos = angle - 90;
                               puppy_.MoveSingle(pos, TAIL);
                               return true;
                           });

        // Light Control
        mcp_server.AddTool("self.light.get_power", "Get light status", PropertyList(), [this](const PropertyList &properties) -> ReturnValue
                           { return led_on_; });

        mcp_server.AddTool("self.light.turn_on", "Turn on light", PropertyList(), [this](const PropertyList &properties) -> ReturnValue
                           {
                    SetLedColor(0xFF, 0xFF, 0xFF);
                    led_on_ = true;
                    return true; });

        mcp_server.AddTool("self.light.turn_off", "Turn off light", PropertyList(), [this](const PropertyList &properties) -> ReturnValue
                           {
                    SetLedColor(0x00, 0x00, 0x00);
                    led_on_ = false;
                    return true; });

        mcp_server.AddTool("self.light.set_rgb", "Set RGB color", PropertyList({Property("r", kPropertyTypeInteger, 0, 255), Property("g", kPropertyTypeInteger, 0, 255), Property("b", kPropertyTypeInteger, 0, 255)}), [this](const PropertyList &properties) -> ReturnValue
                           {
                    int r = properties["r"].value<int>();
                    int g = properties["g"].value<int>();
                    int b = properties["b"].value<int>();

                    led_on_ = true;
                    SetLedColor(r, g, b);
                    return true; });
    }

public:
    EspPuppyS3() : boot_button_(BOOT_BUTTON_GPIO),
                   audio_wake_button_(AUDIO_WAKE_BUTTON_GPIO)
    {
        InitializeButtons();
        InitializeIot();
        InitializeSpi();
        InitializeLcdDisplay();
        InitializeTools();
        GetBacklight()->RestoreBrightness();
    }

    virtual AudioCodec *GetAudioCodec() override
    {
        static NoAudioCodecSimplex audio_codec(
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK,
            AUDIO_I2S_SPK_GPIO_LRCK,
            AUDIO_I2S_SPK_GPIO_DOUT,
            AUDIO_I2S_MIC_GPIO_SCK,
            AUDIO_I2S_MIC_GPIO_WS,
            AUDIO_I2S_MIC_GPIO_DIN);
        return &audio_codec;
    }

    virtual Display *GetDisplay() override
    {
        return display_;
    }

    virtual Backlight *GetBacklight() override
    {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }
};

DECLARE_BOARD(EspPuppyS3);
