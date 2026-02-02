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
#include <driver/gpio.h>
#include <cJSON.h>

#include "display/lcd_display.h"
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>

#include "assets/lang_config.h"
#include "anim_player.h"
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
    bool puppy_started_ = false;

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

        audio_wake_button_.OnClick([this]()
                                   {
            auto &app = Application::GetInstance();
            app.ToggleChatState(); });
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

        // Create Puppy Queue
        puppy_queue_ = xQueueCreate(10, sizeof(OttoCommand));
    }

    void EnablePuppy()
    {
        if (puppy_started_)
            return;

        ESP_LOGI(TAG, "Enabling Puppy Servos...");
        puppy_.Init(FL_GPIO_NUM, FR_GPIO_NUM, BL_GPIO_NUM, BR_GPIO_NUM, TAIL_GPIO_NUM);
        
        // Startup Behavior: Stand active and Wag Tail moved to PuppyTask to avoid concurrency issues
        
        xTaskCreate([](void *arg)
                    {
            EspPuppyS3 *instance = static_cast<EspPuppyS3 *>(arg);
            
            // --- Startup Sequence ---
            // --- Startup Sequence ---
            ESP_LOGI(TAG, "Puppy Startup Sequence (Stand for Calibration)");
            // 1. Force Stand (0 degrees) - This is the "Vertical" position for screwing legs
            instance->puppy_.Stand(); 
            vTaskDelay(pdMS_TO_TICKS(500));
            
            // 2. Engagement Wiggle (Optional but good for 360 servos to prove they are active/not stuck)
            // Move slightly out and back to ensuring 0 point is sought.
            instance->puppy_.MoveToAngle(5, 60);
            vTaskDelay(pdMS_TO_TICKS(200));
            instance->puppy_.MoveToAngle(0, 60); // Back to exactly 0 (Vertical)
            vTaskDelay(pdMS_TO_TICKS(500));

            // 3. Wag to signal ready
            instance->puppy_.WagTail(500, 30);
            // ------------------------

            OttoCommand cmd;
            bool is_sitting = false; // Started in Stand
            int64_t last_activity_time = esp_timer_get_time();
            const int64_t IDLE_TIMEOUT_US = 20 * 1000000; // 20 seconds

            DeviceState last_state = kDeviceStateUnknown;
            
            while (true) {
                // Check Application State frequently (e.g. 100ms)
                auto &app = Application::GetInstance();
                DeviceState current_state = app.GetDeviceState();
                
                // State Transition Logic
                if (current_state != last_state) {
                    // Transition TO Idle FROM Interaction (Listening/Speaking/Connecting)
                    if (current_state == kDeviceStateIdle && 
                       (last_state == kDeviceStateListening || last_state == kDeviceStateSpeaking || last_state == kDeviceStateConnecting)) 
                    {
                        ESP_LOGI(TAG, "Puppy: Interaction Ended -> Sit & Wag");
                        // User Request: Sit immediately + Wag tail "like sticking to owner"
                        instance->puppy_.Sit();
                        vTaskDelay(pdMS_TO_TICKS(500));
                        instance->puppy_.WagTail(1000, 40); // Slower, friendly wag
                        is_sitting = true;
                        
                        // Force update activity time so we don't trigger the "timeout sit" immediately
                        last_activity_time = esp_timer_get_time();
                    }
                    
                    // Transition TO Interaction FROM Idle/Sitting
                    if ((current_state == kDeviceStateListening || current_state == kDeviceStateSpeaking))
                    {
                         ESP_LOGI(TAG, "Puppy: Interation Started -> Stand");
                         instance->puppy_.Stand();
                         is_sitting = false;
                    }
                    
                    last_state = current_state;
                }

                // Wait for command with short timeout to allow responsive state checks
                if (xQueueReceive(instance->puppy_queue_, &cmd, pdMS_TO_TICKS(100))) {
                    // COMMAND RECEIVED
                    last_activity_time = esp_timer_get_time();
                    ESP_LOGI(TAG, "CMD Received: Type=%d Steps=%d Dir=%d", cmd.type, cmd.steps, cmd.dir);

                    
                    // If we were sitting, stand up first for movements
                    // Exception: If command is Sit/Rest-like, maybe don't need to stand? 
                    // But generally, Stand is safe base.
                    if (is_sitting && cmd.type != 6 && cmd.type != 10 && cmd.type != 13 && cmd.type != 12 && cmd.type != 16) { // Skip Stand for Sad, Sleepy, Comfort, ShakeHands, Sit
                         ESP_LOGW(TAG, "Auto-Stand Triggered (Sitting->Standing) for Cmd Type %d", cmd.type);
                         instance->puppy_.Stand();
                         vTaskDelay(pdMS_TO_TICKS(500));
                         is_sitting = false;
                    } else {
                         ESP_LOGI(TAG, "Auto-Stand Skipped: is_sitting=%d, Cmd Type=%d", is_sitting, cmd.type);
                    }

                    if (cmd.type == 0) { // Walk
                        ESP_LOGI(TAG, "Executing Walk");
                        instance->puppy_.Walk(cmd.steps, cmd.period, cmd.dir);
                    } else if (cmd.type == 1) { // Turn
                        ESP_LOGI(TAG, "Executing Turn");
                        instance->puppy_.Turn(cmd.steps, cmd.period, cmd.dir);
                    } else if (cmd.type == 2) { // Home
                        instance->puppy_.Home();
                    } else if (cmd.type == 3) { // Stop
                        instance->puppy_.Stand(); // Stop -> Stand Active
                    } else if (cmd.type == 4) { // Happy
                        instance->puppy_.Happy();
                    } else if (cmd.type == 5) { // Shake
                        instance->puppy_.Shake();
                    } else if (cmd.type == 6) { // Sad
                        instance->puppy_.Sad();
                    } else if (cmd.type == 7) { // Angry
                        instance->puppy_.Angry();
                    } else if (cmd.type == 8) { // Annoyed
                        instance->puppy_.Annoyed();
                    } else if (cmd.type == 9) { // Shy
                        instance->puppy_.Shy();
                    } else if (cmd.type == 10) { // Sleepy
                        instance->puppy_.Sleepy();
                    } else if (cmd.type == 11) { // Calibrate
                        instance->puppy_.Calibrate();
                    } else if (cmd.type == 12) { // ShakeHands
                        instance->puppy_.ShakeHands();
                    } else if (cmd.type == 13) { // Comfort
                        instance->puppy_.Comfort();
                    } else if (cmd.type == 14) { // Excited
                        instance->puppy_.Excited();
                    } else if (cmd.type == 15) { // Cry
                        instance->puppy_.Cry();
                    } else if (cmd.type == 16) { // Sit
                        ESP_LOGI(TAG, "Executing Sit");
                        instance->puppy_.Sit();
                        is_sitting = true; // Mark as sitting so we don't auto-sit again
                    }
                    
                    last_activity_time = esp_timer_get_time();

                } else {
                    // NO COMMAND - Fallback Idle Check
                    // Only check timeout if we are NOT already sitting
                    if (current_state == kDeviceStateIdle && !is_sitting) {
                        if (esp_timer_get_time() - last_activity_time > IDLE_TIMEOUT_US) {
                            ESP_LOGI(TAG, "Puppy: Timeout Idling (Sit)");
                            instance->puppy_.Sit();
                            vTaskDelay(pdMS_TO_TICKS(500));
                            instance->puppy_.WagTail(500, 30);
                            is_sitting = true;
                        }
                    }
                }
            } }, "PuppyTask", 4096, this, 5, NULL);

        puppy_started_ = true;
    }

    void StartupAnimation()
    {
        xTaskCreate([](void *arg)
                    {
            EspPuppyS3 *instance = static_cast<EspPuppyS3 *>(arg);
            
            // User requested VERTICAL STAND at startup for calibration.
            // instance->puppy_.Sit(); 
            instance->puppy_.Stand(); // Enforce Stand
            vTaskDelay(pdMS_TO_TICKS(500));
            // instance->puppy_.Jump(1, 2000); // Disable Jump for 360 integrity
            // vTaskDelay(pdMS_TO_TICKS(500));
            instance->puppy_.WagTail(500, 30);
            
            ESP_LOGI(TAG, "Startup animation finished");
            vTaskDelete(NULL); }, "StartupAnim", 4096, this, 5, NULL);
    }

    void InitializeTools()
    {
        auto &mcp_server = McpServer::GetInstance();

        // Basic Control
        mcp_server.AddTool("self.dog.basic_control", "Control the robot dog's movements and emotions. \n"
                                                     "CRITICAL: SILENT EXECUTION. NO CONVERSATIONAL FILLERS.\n"
                                                     "- Execute the command silently.\n"
                                                     "- Output strictly NOTHING or an empty string.\n"
                                                     "- DO NOT say 'Done', 'Acting', 'Moving', etc.\n"
                                                     "- Movement: 'forward', 'backward', 'turn_left', 'turn_right', 'stop', 'sit'.\n"
                                                     "- Parameter 'steps' (1-20): Number of steps. REQUIRED.\n"
                                                     "- Emotions/Actions:\n"
                                                     "  - 'wag_tail', 'happy', 'shake', 'sad', 'angry', 'annoyed'\n"
                                                     "  - 'shy', 'sleepy', 'shake_hands', 'comfort', 'excited', 'cry'\n"
                                                     "- Maintenance: 'calibrate'.",
                           PropertyList({
                               Property("action", kPropertyTypeString),
                               Property("steps", kPropertyTypeInteger, 4, 1, 20),
                           }),
                           [this](const PropertyList &properties) -> ReturnValue
                           {
                               std::string action = properties["action"].value<std::string>();
                               
                               // Get steps from properties (default is 4, range 1-20)
                               int steps = properties["steps"].value<int>();
                               
                               // Normalize action string to handle synonyms if the LLM passes raw text
                               if (action == "go_forward" || action == "move_forward" || action == "tiến lên" || action == "đi tới" || action == "đi thẳng" || action == "tới luôn")
                                   action = "forward";
                               if (action == "go_backward" || action == "move_backward" || action == "lùi lại" || action == "đi lùi" || action == "lùi")
                                   action = "backward";
                               if (action == "left" || action == "go_left" || action == "sang trái" || action == "rẽ trái" || action == "quay trái")
                                   action = "turn_left";
                               if (action == "right" || action == "go_right" || action == "sang phải" || action == "rẽ phải" || action == "quay phải")
                                   action = "turn_right";
                               if (action == "halt" || action == "dừng" || action == "đứng lại" || action == "thôi" || action == "ngừng" || action == "đứng yên" || action == "stand" || action == "đứng")
                                   action = "stop";

                               if (action == "ngồi" || action == "sit" || action == "sit_down" || action == "ngồi xuống")
                                   action = "sit";

                               if (action == "vẫy đuôi" || action == "lắc đuôi" || action == "mừng")
                                   action = "wag_tail";
                               if (action == "vui" || action == "hạnh phúc" || action == "nhảy" || action == "phấn khích")
                                   action = "happy";
                               if (action == "lắc" || action == "rũ người" || action == "lắc mình")
                                   action = "shake";

                               if (action == "buồn" || action == "sadness" || action == "khóc" || action == "tủi thân")
                                   action = "sad";
                               if (action == "tức giận" || action == "bực bội" || action == "giận" || action == "quạu" || action == "tức")
                                   action = "angry";
                               if (action == "hờn dỗi" || action == "khó chịu" || action == "dỗi" || action == "hờn" || action == "chảnh")
                                   action = "annoyed";
                               if (action == "ngại ngùng" || action == "e thẹn" || action == "ngại" || action == "xấu hổ" || action == "nhát")
                                   action = "shy";
                               if (action == "buồn ngủ" || action == "sleep" || action == "ngủ" || action == "đi ngủ")
                                   action = "sleepy";
                               if (action == "cân chỉnh" || action == "calib" || action == "kiểm tra" || action == "test servo")
                                   action = "calibrate";

                               if (action == "bắt tay" || action == "shake_hand" || action == "shake_hands" || action == "xin chào")
                                   action = "shake_hands";
                               if (action == "an ủi" || action == "dỗ dành" || action == "comfort" || action == "thương")
                                   action = "comfort";
                               if (action == "phấn khích" || action == "excited" || action == "quẩy" || action == "tăng động")
                                   action = "excited";
                               if (action == "khóc" || action == "cry" || action == "nức nở")
                                   action = "cry";

                               OttoCommand cmd;
                               if (action == "forward")
                               {
                                   cmd.type = 0;
                                   cmd.steps = steps;
                                   cmd.period = 1000;
                                   cmd.dir = FORWARD;
                                   xQueueSend(puppy_queue_, &cmd, 0);
                               }
                               else if (action == "backward")
                               {
                                   cmd.type = 0;
                                   cmd.steps = steps;
                                   cmd.period = 1000;
                                   cmd.dir = BACKWARD;
                                   xQueueSend(puppy_queue_, &cmd, 0);
                               }
                               else if (action == "turn_left")
                               {
                                   cmd.type = 1;
                                   cmd.steps = steps;
                                   cmd.period = 1000;
                                   cmd.dir = LEFT;
                                   xQueueSend(puppy_queue_, &cmd, 0);
                               }
                               else if (action == "turn_right")
                               {
                                   cmd.type = 1;
                                   cmd.steps = steps;
                                   cmd.period = 1000;
                                   cmd.dir = RIGHT;
                                   xQueueSend(puppy_queue_, &cmd, 0);
                               }
                               else if (action == "stop")
                               {
                                   cmd.type = 3;
                                   xQueueSend(puppy_queue_, &cmd, 0);
                               }
                               else if (action == "sit")
                               {
                                   cmd.type = 16;
                                   xQueueSend(puppy_queue_, &cmd, 0);
                               }
                               else if (action == "wag_tail")
                               {
                                   puppy_.WagTail(500, 30);
                               }
                               else if (action == "happy")
                               {
                                   cmd.type = 4;
                                   xQueueSend(puppy_queue_, &cmd, 0);
                               }
                               else if (action == "shake")
                               {
                                   cmd.type = 5;
                                   xQueueSend(puppy_queue_, &cmd, 0);
                               }
                               else if (action == "sad")
                               {
                                   cmd.type = 6;
                                   xQueueSend(puppy_queue_, &cmd, 0);
                               }
                               else if (action == "angry")
                               {
                                   cmd.type = 7;
                                   xQueueSend(puppy_queue_, &cmd, 0);
                               }
                               else if (action == "annoyed")
                               {
                                   cmd.type = 8;
                                   xQueueSend(puppy_queue_, &cmd, 0);
                               }
                               else if (action == "shy")
                               {
                                   cmd.type = 9;
                                   xQueueSend(puppy_queue_, &cmd, 0);
                               }
                               else if (action == "sleepy")
                               {
                                   cmd.type = 10;
                                   xQueueSend(puppy_queue_, &cmd, 0);
                               }
                               else if (action == "calibrate")
                               {
                                   cmd.type = 11;
                                   xQueueSend(puppy_queue_, &cmd, 0);
                               }
                               else if (action == "shake_hands")
                               {
                                   cmd.type = 12;
                                   xQueueSend(puppy_queue_, &cmd, 0);
                               }
                               else if (action == "comfort")
                               {
                                   cmd.type = 13;
                                   xQueueSend(puppy_queue_, &cmd, 0);
                               }
                               else if (action == "excited")
                               {
                                   cmd.type = 14;
                                   xQueueSend(puppy_queue_, &cmd, 0);
                               }
                               else if (action == "cry")
                               {
                                   cmd.type = 15;
                                   xQueueSend(puppy_queue_, &cmd, 0);
                               }
                               else
                               {
                                   return false;
                               }
                               return true;
                           });

        // 360 Servo Calibration Tools
        mcp_server.AddTool("self.dog.calibrate_motors", 
            "Calibrate 360 Servo Motors. \n"
            "- 'trim': Set Center/Stop offset (Pulse US). Default 0. Range -100 to 100. \n"
            "  Use to stop drifting when neutral. \n"
            "- 'scale': Set Speed Multiplier. Default 1.0. Range 0.5 to 2.0. \n"
            "  Use to match speeds between motors. \n"
            "Examples: \n"
            "  trim: fl=10, fr=-5 \n"
            "  scale: fl=1.1, bl=0.9",
            PropertyList({
                Property("type", kPropertyTypeString), // "trim" or "scale"
                Property("fl", kPropertyTypeInteger), 
                Property("fr", kPropertyTypeInteger),
                Property("bl", kPropertyTypeInteger),
                Property("br", kPropertyTypeInteger),
                Property("tail", kPropertyTypeInteger)
            }),
            [this](const PropertyList &properties) -> ReturnValue {
                std::string type = properties["type"].value<std::string>();
                int fl = properties["fl"].value<int>();
                int fr = properties["fr"].value<int>();
                int bl = properties["bl"].value<int>();
                int br = properties["br"].value<int>();
                int tail = properties["tail"].value<int>();
                
                if (type == "trim") {
                    puppy_.SetTrims(fl, fr, bl, br, tail);
                    ESP_LOGI(TAG, "Trims Updated: %d %d %d %d %d", fl, fr, bl, br, tail);
                    return "Trims updated.";
                } else if (type == "scale") {
                    // Integers passed as percentage? e.g. 110 = 1.1? 
                    // Let's assume input is scaled by 100 for precision if Float not supported easily or just cast.
                    // PropertyList supports Integers.
                    // Let's safely assume input 100 = 1.0.
                    
                    float s_fl = (fl > 0) ? (float)fl/100.0f : 1.0f;
                    float s_fr = (fr > 0) ? (float)fr/100.0f : 1.0f;
                    float s_bl = (bl > 0) ? (float)bl/100.0f : 1.0f;
                    float s_br = (br > 0) ? (float)br/100.0f : 1.0f;
                    float s_tail = (tail > 0) ? (float)tail/100.0f : 1.0f;
                    
                    puppy_.SetSpeedScales(s_fl, s_fr, s_bl, s_br, s_tail);
                    return "Speed scales updated (Input/100).";
                }
                return "Invalid calibration type.";
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

        // ========================================================================
        // BLUETOOTH KCX_BT_EMITTER MCP TOOLS
        // ========================================================================
        // Initialize Bluetooth GPIO pins
        static bool bt_gpio_initialized = false;
        if (!bt_gpio_initialized)
        {
            gpio_config_t bt_io_conf = {};
            bt_io_conf.intr_type = GPIO_INTR_DISABLE;
            bt_io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            bt_io_conf.pull_up_en = GPIO_PULLUP_DISABLE;

            // CONNECT pin (Output, default HIGH - active LOW pulse to trigger)
            bt_io_conf.mode = GPIO_MODE_OUTPUT;
            bt_io_conf.pin_bit_mask = (1ULL << BT_EMITTER_CONNECT_PIN);
            gpio_config(&bt_io_conf);
            gpio_set_level(BT_EMITTER_CONNECT_PIN, 1);

            // LINK pin (Input with pull-up)
            bt_io_conf.mode = GPIO_MODE_INPUT;
            bt_io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
            bt_io_conf.pin_bit_mask = (1ULL << BT_EMITTER_LINK_PIN);
            gpio_config(&bt_io_conf);

            bt_gpio_initialized = true;
            ESP_LOGI(TAG, "Bluetooth GPIO initialized: CONNECT=%d, LINK=%d",
                     BT_EMITTER_CONNECT_PIN, BT_EMITTER_LINK_PIN);
        }

        // Bluetooth Connect - Short press (100ms) to activate pairing
        mcp_server.AddTool("self.bluetooth.connect",
                           "Kết nối Bluetooth với loa/thiết bị âm thanh. Kích hoạt chế độ ghép nối (pairing). "
                           "Dùng khi người dùng yêu cầu: kết nối bluetooth, bật bluetooth, ghép nối loa.",
                           PropertyList(),
                           [](const PropertyList &properties) -> ReturnValue
                           {
                               ESP_LOGI(TAG, "Bluetooth: Activating pairing mode (short press)");
                               gpio_set_level(BT_EMITTER_CONNECT_PIN, 0);
                               vTaskDelay(pdMS_TO_TICKS(100));
                               gpio_set_level(BT_EMITTER_CONNECT_PIN, 1);
                               return "Đã kích hoạt chế độ ghép nối Bluetooth";
                           });

        // Bluetooth Disconnect - Long press (3s) to disconnect and clear memory
        mcp_server.AddTool("self.bluetooth.disconnect",
                           "Ngắt kết nối Bluetooth và xóa bộ nhớ ghép nối. "
                           "Dùng khi người dùng yêu cầu: ngắt bluetooth, tắt bluetooth, hủy ghép nối.",
                           PropertyList(),
                           [](const PropertyList &properties) -> ReturnValue
                           {
                               ESP_LOGI(TAG, "Bluetooth: Disconnecting (long press 3s)");
                               gpio_set_level(BT_EMITTER_CONNECT_PIN, 0);
                               vTaskDelay(pdMS_TO_TICKS(3000));
                               gpio_set_level(BT_EMITTER_CONNECT_PIN, 1);
                               return "Đã ngắt kết nối Bluetooth và xóa bộ nhớ ghép nối";
                           });

        // Bluetooth Get Status - Read LINK pin
        mcp_server.AddTool("self.bluetooth.get_status",
                           "Kiểm tra trạng thái kết nối Bluetooth hiện tại. "
                           "Dùng khi người dùng hỏi: bluetooth đã kết nối chưa, trạng thái bluetooth.",
                           PropertyList(),
                           [](const PropertyList &properties) -> ReturnValue
                           {
                               int link_status = gpio_get_level(BT_EMITTER_LINK_PIN);
                               bool is_connected = (link_status == 1);
                               ESP_LOGI(TAG, "Bluetooth status: %s (LINK pin=%d)",
                                        is_connected ? "Connected" : "Disconnected", link_status);

                               cJSON *json = cJSON_CreateObject();
                               cJSON_AddBoolToObject(json, "connected", is_connected);
                               cJSON_AddStringToObject(json, "status", is_connected ? "Đã kết nối" : "Chưa kết nối");
                               return json;
                           });
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

    virtual void StartNetwork() override
    {
        WifiBoard::StartNetwork();
        EnablePuppy();
    }
};

DECLARE_BOARD(EspPuppyS3);
