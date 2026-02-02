/**************************************************************************** 
  
 !!!!!!!!!!!!!!!!!!!!! 接底座 USB 供电 !!!!!!!!!!!!!!!!!!!!!!
 !!!!!!!!!!!!!!!!!!!!! 接底座 USB 供电 !!!!!!!!!!!!!!!!!!!!!
 !!!!!!!!!!!!!!!!!!!!! 接底座 USB 供电 !!!!!!!!!!!!!!!!!!!!!

* i2c bus external(echo pyramid):
   - Custom Device STM32@0x1A
   - Smart K Audio Amplifier AW87559@0x5B
   - Clock Generator Si5351@0x60
   - ES8311@0x30
   - ES7210@0x80
* i2c bus internal(atoms3r):
   - Backlight Controller LP5562@0x30

***************************************************************************/
#include "board.h"
#include "wifi_board.h"
#include "audio/codecs/box_audio_codec.h"
#include "display/lcd_display.h"
#include "display/lvgl_display/lvgl_theme.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "i2c_device.h"
#include "echo_pyramid.h"
#include "assets/lang_config.h"
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <wifi_station.h>
#include <ssid_manager.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_gc9a01.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>
#include <atomic>
#include <esp_random.h>


#define TAG "AtomS3R+EchoPyramid"


LV_IMAGE_DECLARE(click);
LV_IMAGE_DECLARE(ec_left);
LV_IMAGE_DECLARE(ec_right);
LV_FONT_DECLARE(BUILTIN_TEXT_FONT);

/**
 * LP5562 Backlight Controller
 * I2C Address: 0x30
 */
class Lp5562 : public I2cDevice {
public:
    Lp5562(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
        WriteReg(0x00, 0B01000000); // Set chip_en to 1
        WriteReg(0x08, 0B00000001); // Enable internal clock
        WriteReg(0x70, 0B00000000); // Configure all LED outputs to be controlled from I2C registers

        // PWM clock frequency 558 Hz
        auto data = ReadReg(0x08);
        data = data | 0B01000000;
        WriteReg(0x08, data);
    }

    void SetBrightness(uint8_t brightness) {
        // Map 0~100 to 0~255
        brightness = brightness * 255 / 100;
        WriteReg(0x0E, brightness);
    }
};

class CustomBacklight : public Backlight {
public:
    CustomBacklight(Lp5562* lp5562) : lp5562_(lp5562) {}

    void SetBrightnessImpl(uint8_t brightness) override {
        if (lp5562_) {
            lp5562_->SetBrightness(brightness);
        } else {
            ESP_LOGE(TAG, "LP5562 not available");
        }
    }

private:
    Lp5562* lp5562_ = nullptr;
};

static const gc9a01_lcd_init_cmd_t gc9107_lcd_init_cmds[] = {
    //  {cmd, { data }, data_size, delay_ms}
    {0xfe, (uint8_t[]){0x00}, 0, 0},
    {0xef, (uint8_t[]){0x00}, 0, 0},
    {0xb0, (uint8_t[]){0xc0}, 1, 0},
    {0xb2, (uint8_t[]){0x2f}, 1, 0},
    {0xb3, (uint8_t[]){0x03}, 1, 0},
    {0xb6, (uint8_t[]){0x19}, 1, 0},
    {0xb7, (uint8_t[]){0x01}, 1, 0},
    {0xac, (uint8_t[]){0xcb}, 1, 0},
    {0xab, (uint8_t[]){0x0e}, 1, 0},
    {0xb4, (uint8_t[]){0x04}, 1, 0},
    {0xa8, (uint8_t[]){0x19}, 1, 0},
    {0xb8, (uint8_t[]){0x08}, 1, 0},
    {0xe8, (uint8_t[]){0x24}, 1, 0},
    {0xe9, (uint8_t[]){0x48}, 1, 0},
    {0xea, (uint8_t[]){0x22}, 1, 0},
    {0xc6, (uint8_t[]){0x30}, 1, 0},
    {0xc7, (uint8_t[]){0x18}, 1, 0},
    {0xf0,
    (uint8_t[]){0x1f, 0x28, 0x04, 0x3e, 0x2a, 0x2e, 0x20, 0x00, 0x0c, 0x06,
                0x00, 0x1c, 0x1f, 0x0f},
    14, 0},
    {0xf1,
    (uint8_t[]){0x00, 0x2d, 0x2f, 0x3c, 0x6f, 0x1c, 0x0b, 0x00, 0x00, 0x00,
                0x07, 0x0d, 0x11, 0x0f},
    14, 0},
};

class AtomS3rEchoPyramidBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_external_;
    i2c_master_bus_handle_t i2c_bus_internal_;
    EchoPyramid* echo_pyramid_ = nullptr;
    Si5351* si5351_ = nullptr;
    Aw87559* aw87559_ = nullptr;
    Lp5562* lp5562_ = nullptr;
    Display* display_ = nullptr;
    Button boot_button_;
    bool is_echo_pyramid_connected_ = false;
    
    // 启动教程状态
    std::atomic<bool> startup_tutorial_active_{false};
    std::atomic<int> startup_tutorial_step_{0}; // 0=未开始, 1=Step1, 2=Step2, 3=Step3
    std::atomic<bool> startup_tutorial_step_complete_{false};

    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_1,
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
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_external_));
        
        i2c_bus_cfg.i2c_port = I2C_NUM_0;
        i2c_bus_cfg.sda_io_num = GPIO_NUM_45;
        i2c_bus_cfg.scl_io_num = GPIO_NUM_0;
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_internal_));
    }

    void I2cDetect(i2c_master_bus_handle_t i2c_bus) {
        is_echo_pyramid_connected_ = false;
        uint8_t address;
        printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\r\n");
        for (int i = 0; i < 128; i += 16) {
            printf("%02x: ", i);
            for (int j = 0; j < 16; j++) {
                fflush(stdout);
                address = i + j;
                esp_err_t ret = i2c_master_probe(i2c_bus, address, 200); // 200ms timeout
                if (ret == ESP_OK) {
                    printf("%02x ", address);
                    if (address == ECHO_PYRAMID_DEVICE_ADDR) {
                        is_echo_pyramid_connected_ = true;
                    }
                } else if (ret == ESP_ERR_TIMEOUT) {
                    printf("UU ");
                } else {
                    printf("-- ");
                }
            }
            printf("\r\n");
        }
    }

    void CheckEchoPyramidConnection() {
        if (is_echo_pyramid_connected_) {
            return;
        }
        GetBacklight()->SetBrightness(100);
        display_->SetStatus(Lang::Strings::ERROR);
        display_->SetEmotion("triangle_exclamation");
        display_->SetChatMessage("system", "Echo Pyramid\nnot connected");
        while (1) {
            ESP_LOGE(TAG, "Echo Pyramid is disconnected");
            vTaskDelay(pdMS_TO_TICKS(500));
            I2cDetect(i2c_bus_external_);
            if (is_echo_pyramid_connected_) {
                ESP_LOGI(TAG, "Echo Pyramid is reconnected");
                esp_restart();
            }
        }
    }

    void InitializeLp5562() {
        ESP_LOGI(TAG, "Init LED Driver LP5562");
        lp5562_ = new Lp5562(i2c_bus_internal_, LED_DRIVER_LP5562_ADDR);
    }

    void InitializeEchoPyramid() {
        ESP_LOGI(TAG, "Init Echo Pyramid");
        echo_pyramid_ = new EchoPyramid(i2c_bus_external_, ECHO_PYRAMID_DEVICE_ADDR);
        si5351_ = new Si5351(i2c_bus_external_);
        aw87559_ = new Aw87559(i2c_bus_external_);
        echo_pyramid_->addTouchEventCallback([this](TouchEvent event) {
            OnTouchEvent(event);
        });
        echo_pyramid_->startTouchDetection();
    }

    void InitializeSpi() {
        ESP_LOGI(TAG, "Initialize SPI bus");
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = GPIO_NUM_21;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = GPIO_NUM_15;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeGc9107Display() {
        ESP_LOGI(TAG, "Init GC9107 display");

        ESP_LOGI(TAG, "Install panel IO");
        esp_lcd_panel_io_handle_t io_handle = NULL;
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = GPIO_NUM_14;
        io_config.dc_gpio_num = GPIO_NUM_42;
        io_config.spi_mode = 0;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &io_handle));
    
        ESP_LOGI(TAG, "Install GC9A01 panel driver");
        esp_lcd_panel_handle_t panel_handle = NULL;
        gc9a01_vendor_config_t gc9107_vendor_config = {
            .init_cmds = gc9107_lcd_init_cmds,
            .init_cmds_size = sizeof(gc9107_lcd_init_cmds) / sizeof(gc9a01_lcd_init_cmd_t),
        };
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_48; // Set to -1 if not use
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR;
#else
        panel_config.rgb_endian = LCD_RGB_ENDIAN_BGR;
#endif
        panel_config.bits_per_pixel = 16; // Implemented by LCD command `3Ah` (16/18)
        panel_config.vendor_config = &gc9107_vendor_config;

        ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true)); 

        display_ = new SpiLcdDisplay(io_handle, panel_handle,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, 
                                    DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
        
        auto dark_theme = LvglThemeManager::GetInstance().GetTheme("dark");
        if (dark_theme != nullptr) {    
            display_->SetTheme(dark_theme);
            ESP_LOGI(TAG, "Theme set to dark");
        } else {
            ESP_LOGW(TAG, "Dark theme not found");
        }
    }
    
    bool WaitForStepComplete(uint32_t timeout_ms) {
        uint32_t start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        while (!startup_tutorial_step_complete_) {
            if (xTaskGetTickCount() * portTICK_PERIOD_MS - start_time >= timeout_ms) {
                return false; // timeout
            }
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        startup_tutorial_step_complete_ = false;
        return true; // completed
    }

    void StartupTutorial() {
        ESP_LOGI(TAG, "Displaying startup tutorial");

        const uint32_t STEP_TIMEOUT_MS = 10000;
        startup_tutorial_active_ = true;
        startup_tutorial_step_ = 1;
        startup_tutorial_step_complete_ = false;
    
        lv_obj_t* scr_origin = lv_screen_active();
        lv_obj_t* scr_intro = nullptr;
        lv_obj_t* img_tip = nullptr;
        // lv_obj_t* label_tip = nullptr;
        
        {
            DisplayLockGuard lock(display_);
            scr_intro = lv_obj_create(NULL);
            auto dark_theme = LvglThemeManager::GetInstance().GetTheme("dark");
            lv_obj_set_style_bg_color(scr_intro, dark_theme->background_color(), 0);
            
            img_tip = lv_image_create(scr_intro);
            // label_tip = lv_label_create(scr_intro);
            // lv_obj_align(label_tip, LV_ALIGN_TOP_MID, 0, 3);
            // lv_obj_set_style_text_color(label_tip, lv_color_hex(0x1296DB), 0);
            // lv_obj_set_style_text_font(label_tip, &BUILTIN_TEXT_FONT, 0);
            lv_screen_load(scr_intro);
        }

        struct TutorialStep {
            const lv_image_dsc_t* img;
            const char* text;
        };
        TutorialStep steps[] = {
            {&ec_left, "左滑切换灯效"},
            {&ec_right, "右滑调节音量"},
            {&click, "点击唤醒对话"},
        };

        for (int i = 0; i < 3; i++) {
            startup_tutorial_step_ = i + 1;
            {
                DisplayLockGuard lock(display_);
                lv_obj_set_size(img_tip, 128, 128);
                lv_image_set_src(img_tip, steps[i].img);
                lv_obj_align(img_tip, LV_ALIGN_CENTER, 0, 0);
                lv_refr_now(nullptr);
            }
            if (!WaitForStepComplete(STEP_TIMEOUT_MS)) {
                ESP_LOGI(TAG, "Startup tutorial Step %d timeout", i + 1);
            }
        }
        
        // Cleanup
        startup_tutorial_active_ = false;
        startup_tutorial_step_ = 0;
        {
            DisplayLockGuard lock(display_);
            if (scr_origin) lv_screen_load(scr_origin);
            if (scr_intro) lv_obj_delete(scr_intro);
            lv_refr_now(nullptr);
        }
        
        ESP_LOGI(TAG, "Startup tutorial completed");
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            if (startup_tutorial_active_ && startup_tutorial_step_ == 3) {
                startup_tutorial_step_complete_ = true;
                Application::GetInstance().PlaySound(Lang::Sounds::OGG_D3);
                ESP_LOGI(TAG, "Startup tutorial Step 3 completed by button click");
                return;
            }

            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState();
        });

        // 长按退出对话
        boot_button_.OnLongPress([this]() {
            auto& app = Application::GetInstance();
            auto state = app.GetDeviceState();
            if (state == kDeviceStateListening || state == kDeviceStateSpeaking) {
                ESP_LOGI(TAG, "Long press: Exit chat");
                app.SetDeviceState(kDeviceStateIdle);
                if (display_) {
                    display_->SetChatMessage("system", "");
                }
            }
        });
    }

    /**
     * 获取下一个灯效模式
     */
    LightMode GetNextLightMode(LightMode current_mode) {
        int mode = static_cast<int>(current_mode);
        int next_mode = (mode + 1) % 5; // 5种模式循环
        return static_cast<LightMode>(next_mode);
    }

    /**
     * 获取上一个灯效模式
     */
    LightMode GetPreviousLightMode(LightMode current_mode) {
        int mode = static_cast<int>(current_mode);
        int prev_mode = (mode - 1 + 5) % 5; // 5种模式循环
        return static_cast<LightMode>(prev_mode);
    }

    /**
     * 触摸事件处理
     */
    void OnTouchEvent(TouchEvent event) {
        if (echo_pyramid_ == nullptr) return;
        if (startup_tutorial_active_) {
            if (startup_tutorial_step_ == 1 && 
                (event == TouchEvent::LEFT_SLIDE_UP || event == TouchEvent::LEFT_SLIDE_DOWN)) {
                startup_tutorial_step_complete_ = true;
                echo_pyramid_->setLightMode(LightMode::RAINBOW);
                Application::GetInstance().PlaySound(Lang::Sounds::OGG_D3);
                ESP_LOGI(TAG, "Startup tutorial Step 1 completed by left swipe");
                return;
            }
            if (startup_tutorial_step_ == 2 && 
                (event == TouchEvent::RIGHT_SLIDE_UP || event == TouchEvent::RIGHT_SLIDE_DOWN)) {
                startup_tutorial_step_complete_ = true;
                Application::GetInstance().PlaySound(Lang::Sounds::OGG_SUCCESS);
                ESP_LOGI(TAG, "Startup tutorial Step 2 completed by right swipe");
                return;
            }
        }

        // 左侧滑动：切换灯效
        if (event == TouchEvent::LEFT_SLIDE_UP || event == TouchEvent::LEFT_SLIDE_DOWN) {
            LightMode current_mode = echo_pyramid_->getLightMode();
            LightMode new_mode;
            if (event == TouchEvent::LEFT_SLIDE_UP) {
                new_mode = GetPreviousLightMode(current_mode);
                ESP_LOGI(TAG, "Left slide up: Switch to next light mode");
            } else {
                new_mode = GetNextLightMode(current_mode);
                ESP_LOGI(TAG, "Left slide down: Switch to previous light mode");
            }
            echo_pyramid_->setLightMode(new_mode);
            if (display_) {
                const char* mode_names[] = {"OFF", "STATIC", "BREATHE", "RAINBOW", "CHASE"};
                int mode_index = static_cast<int>(new_mode);
            }
            if (Application::GetInstance().GetDeviceState() != kDeviceStateSpeaking) {
                Application::GetInstance().PlaySound(Lang::Sounds::OGG_D3);
            }
            return;
        }

        // 右侧滑动：控制音量
        if (event == TouchEvent::RIGHT_SLIDE_UP || event == TouchEvent::RIGHT_SLIDE_DOWN) {
            auto codec = GetAudioCodec();
            if (codec == nullptr) return;
            int current_volume = codec->output_volume();
            int new_volume = current_volume;
            if (event == TouchEvent::RIGHT_SLIDE_UP) {
                new_volume = current_volume - 10;
                if (new_volume < 0) new_volume = 0;
                codec->SetOutputVolume(new_volume);
                ESP_LOGI(TAG, "Right slide up: Volume %d → %d", current_volume, new_volume);
                if (Application::GetInstance().GetDeviceState() != kDeviceStateSpeaking) {
                    Application::GetInstance().PlaySound(Lang::Sounds::OGG_D2);
                }
            } else {
                new_volume = current_volume + 10;
                if (new_volume > 100) new_volume = 100;
                codec->SetOutputVolume(new_volume);
                ESP_LOGI(TAG, "Right slide down: Volume %d → %d", current_volume, new_volume);
                if (Application::GetInstance().GetDeviceState() != kDeviceStateSpeaking) {
                    Application::GetInstance().PlaySound(Lang::Sounds::OGG_SUCCESS);
                }
            }
            if (display_) {
                display_->ShowNotification("Volume: " + std::to_string(new_volume));
            }
    
            return;
        }
    }

public:
    AtomS3rEchoPyramidBoard() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();
        I2cDetect(i2c_bus_external_);
        InitializeLp5562();
        InitializeSpi();
        InitializeGc9107Display();
        CheckEchoPyramidConnection();
        InitializeEchoPyramid();
        InitializeButtons();
        GetBacklight()->RestoreBrightness();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static BoxAudioCodec audio_codec(
            i2c_bus_external_, 
            AUDIO_INPUT_SAMPLE_RATE, 
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, 
            AUDIO_I2S_GPIO_BCLK, 
            AUDIO_I2S_GPIO_WS, 
            AUDIO_I2S_GPIO_DOUT, 
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_GPIO_PA, 
            AUDIO_CODEC_ES8311_ADDR, 
            AUDIO_CODEC_ES7210_ADDR,
            AUDIO_INPUT_REFERENCE); // input_reference (AEC)
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight *GetBacklight() override {
        static CustomBacklight backlight(lp5562_);
        return &backlight;
    }

    virtual void OnAudioServiceReady() override {
        Application::GetInstance().PlaySound(Lang::Sounds::OGG_WELCOME);
        auto ssid_list = SsidManager::GetInstance().GetSsidList();
        if (ssid_list.empty()) {
            auto codec = GetAudioCodec();
            if (codec) {
                codec->SetOutputVolume(30);
            }
            StartupTutorial();
        }
    }
};

DECLARE_BOARD(AtomS3rEchoPyramidBoard);
