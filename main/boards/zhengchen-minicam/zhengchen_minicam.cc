#include "wifi_board.h"
#include "codecs/es8388_audio_codec.h"
#include "display/lcd_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "led/single_led.h"
#include "esp32_camera.h"
#include "zhengchen_minicam_lcd_display.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/gpio.h>
#include <wifi_manager.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <driver/spi_common.h>
#include <driver/i2c_master.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#define TAG "zhengchen_minicam"

class zhengchen_minicam : public WifiBoard {
private:
    Button boot_button_;
    Button volume_up_button_;
    Button volume_down_button_;
    Esp32Camera* camera_;
    i2c_master_bus_handle_t codec_i2c_bus_;
    adc_oneshot_unit_handle_t adc_handle_;
    adc_cali_handle_t adc_cali_handle_;
    TaskHandle_t adc_task_handle_;
    TaskHandle_t volume_key_task_handle_;
    SemaphoreHandle_t adc_mutex_;
    Display* display_;
    static constexpr size_t kBatteryAverageWindowSize = 8;
    int battery_samples_mv_[kBatteryAverageWindowSize] = {0};
    int battery_ref_samples_mv_[kBatteryAverageWindowSize] = {0};
    size_t battery_sample_count_ = 0;
    size_t battery_sample_index_ = 0;
    int battery_voltage_mv_ = BATTERY_FULL_VOLTAGE_MV;
    int battery_percent_ = 100;
    int battery_sample_raw_ = 0;
    int battery_ref_raw_ = 0;
    int battery_sample_voltage_mv_ = 0;
    int battery_ref_voltage_mv_ = BATTERY_REF_VOLTAGE_MV;
    int volume_key_voltage_mv_ = 0;
    bool is_charging_ = false;
    int last_reported_battery_percent_ = -1;
    bool last_reported_is_charging_ = false;
    enum class AdcVolumeKeyState : uint8_t {
        None,
        VolumeDown,
        VolumeUp,
    };
    AdcVolumeKeyState volume_key_state_ = AdcVolumeKeyState::None;
    AdcVolumeKeyState volume_key_candidate_state_ = AdcVolumeKeyState::None;
    uint8_t volume_key_stable_count_ = 0;
    bool is_landscape_ = false;
    int charging_samples_[3] = {0, 0, 0};
    size_t charging_sample_count_ = 0;
    size_t charging_sample_index_ = 0;

    void UpdateStateOutput() {
        const auto state = Application::GetInstance().GetDeviceState();
        const int level = (state == kDeviceStateListening || state == kDeviceStateSpeaking) ? 0 : 1;
        gpio_set_level(STATE_OUTPUT_GPIO, level);
    }

    void InitializeStateOutput() {
        const gpio_config_t io_conf = {
            .pin_bit_mask = 1ULL << STATE_OUTPUT_GPIO,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&io_conf));
        gpio_set_level(STATE_OUTPUT_GPIO, 1);
    }

    ZhengchenMinicamLcdDisplay* GetMinicamDisplay() {
        return dynamic_cast<ZhengchenMinicamLcdDisplay*>(display_);
    }

    void EnterSettingsMenu() {
        auto* display = GetMinicamDisplay();
        if (display == nullptr) {
            return;
        }
        display->ShowSettingsMenu();
        display->ShowNotification("设置菜单");
    }

    void ToggleSettingsMenu() {
        auto* display = GetMinicamDisplay();
        if (display == nullptr) {
            return;
        }

        if (display->IsSettingsMenuVisible()) {
            display->HideSettingsMenu();
            display->ShowNotification("关闭设置");
        } else {
            display->ShowSettingsMenu();
            display->ShowNotification("设置菜单");
        }
    }

    void AdjustVolume(int delta) {
        auto* codec = GetAudioCodec();
        if (codec == nullptr) {
            return;
        }

        int volume = codec->output_volume() + delta;
        if (volume < 0) {
            volume = 0;
        }
        if (volume > 100) {
            volume = 100;
        }

        codec->SetOutputVolume(volume);
        GetDisplay()->ShowNotification(std::to_string(volume / 10));
    }

    void ToggleAec() {
        auto& app = Application::GetInstance();
        app.SetAecMode(app.GetAecMode() == kAecOff ? kAecOnDeviceSide : kAecOff);
    }

    void ToggleOrientation() {
        auto* display = GetMinicamDisplay();
        if (display == nullptr) {
            return;
        }
        display->ToggleOrientation();
        display->ShowNotification(display->IsLandscape() ? "横屏" : "竖屏");
    }

    void ReprovisionNetwork() {
        EnterWifiConfigMode();
    }

    void HandleAdcVolumeKey(int voltage_mv) {
        volume_key_voltage_mv_ = voltage_mv;

        AdcVolumeKeyState new_state = AdcVolumeKeyState::None;
        if (voltage_mv >= VOLUME_DOWN_KEY_MIN_MV && voltage_mv <= VOLUME_DOWN_KEY_MAX_MV) {
            new_state = AdcVolumeKeyState::VolumeDown;
        } else if (voltage_mv >= VOLUME_UP_KEY_MIN_MV && voltage_mv <= VOLUME_UP_KEY_MAX_MV) {
            new_state = AdcVolumeKeyState::VolumeUp;
        }

        if (new_state != volume_key_candidate_state_) {
            volume_key_candidate_state_ = new_state;
            volume_key_stable_count_ = 1;
            return;
        }

        if (volume_key_stable_count_ < 3) {
            ++volume_key_stable_count_;
            return;
        }

        if (new_state == volume_key_state_) {
            return;
        }

        switch (new_state) {
        case AdcVolumeKeyState::VolumeDown: {
            auto* display = GetMinicamDisplay();
            if (display != nullptr && display->IsSettingsMenuVisible()) {
                display->SelectNextMenuItem();
            } else {
                AdjustVolume(-10);
            }
            break;
        }
        case AdcVolumeKeyState::VolumeUp: {
            auto* display = GetMinicamDisplay();
            if (display != nullptr && display->IsSettingsMenuVisible()) {
                display->SelectPreviousMenuItem();
            } else {
                AdjustVolume(10);
            }
            break;
        }
        case AdcVolumeKeyState::None:
            break;
        }

        volume_key_state_ = new_state;
    }

    void UpdateBatteryState(int battery_raw, int battery_voltage, int ref_raw, int ref_voltage) {
        battery_sample_raw_ = battery_raw;
        battery_sample_voltage_mv_ = battery_voltage;
        battery_ref_raw_ = ref_raw;
        battery_ref_voltage_mv_ = ref_voltage;

        battery_samples_mv_[battery_sample_index_] = battery_voltage;
        battery_ref_samples_mv_[battery_sample_index_] = ref_voltage;
        battery_sample_index_ = (battery_sample_index_ + 1) % kBatteryAverageWindowSize;
        if (battery_sample_count_ < kBatteryAverageWindowSize) {
            ++battery_sample_count_;
        }

        int battery_voltage_sum = 0;
        int ref_voltage_sum = 0;
        for (size_t i = 0; i < battery_sample_count_; ++i) {
            battery_voltage_sum += battery_samples_mv_[i];
            ref_voltage_sum += battery_ref_samples_mv_[i];
        }
        battery_voltage = battery_voltage_sum / static_cast<int>(battery_sample_count_);
        ref_voltage = ref_voltage_sum / static_cast<int>(battery_sample_count_);

        is_charging_ = ref_raw > 2300;

        float battery_voltage_v = 0.0f;
        if (is_charging_) {
            battery_voltage_v = static_cast<float>(battery_voltage) * 2.0f / 1000.0f;
        } else {
            if (ref_voltage <= 0) {
                return;
            }
            battery_voltage_v = static_cast<float>(battery_voltage) * static_cast<float>(BATTERY_REF_VOLTAGE_MV) * 2.0f /
                (static_cast<float>(ref_voltage) * 1000.0f);
        }

        battery_voltage_mv_ = static_cast<int>(battery_voltage_v * 1000.0f + 0.5f);

        int percent = static_cast<int>(((battery_voltage_v - 3.4f) * 100.0f / 0.8f) + 0.5f);
        if (percent < 0) {
            percent = 0;
        }
        if (percent > 100) {
            percent = 100;
        }
        battery_percent_ = percent;

        if (display_ != nullptr && (battery_percent_ != last_reported_battery_percent_ || is_charging_ != last_reported_is_charging_)) {
            last_reported_battery_percent_ = battery_percent_;
            last_reported_is_charging_ = is_charging_;
            Application::GetInstance().Schedule([this]() {
                if (display_ != nullptr) {
                    display_->UpdateStatusBar(true);
                }
            });
        }
    }

    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
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
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));

        // 扫描 I2C 总线上的设备
        ESP_LOGI(TAG, "Scanning I2C bus...");
        for (uint8_t addr = 0x08; addr < 0x78; addr++) {
            i2c_device_config_t dev_cfg = {
                .dev_addr_length = I2C_ADDR_BIT_LEN_7,
                .device_address = addr,
                .scl_speed_hz = 100000,
            };

            i2c_master_dev_handle_t dev_handle;
            if (i2c_master_bus_add_device(codec_i2c_bus_, &dev_cfg, &dev_handle) == ESP_OK) {
                uint8_t data;
                if (i2c_master_receive(dev_handle, &data, 1, 100) == ESP_OK) {
                    ESP_LOGI(TAG, "Found I2C device at address 0x%02X", addr);
                }
                i2c_master_bus_rm_device(dev_handle);
            }
        }
        ESP_LOGI(TAG, "I2C scan complete");
    }

    static void adc_read_task(void* arg) {
        zhengchen_minicam* self = static_cast<zhengchen_minicam*>(arg);
        int battery_raw = 0;
        int battery_voltage = 0;
        int ref_raw = 0;
        int ref_voltage = 0;

        while (1) {
            self->UpdateStateOutput();
            if (xSemaphoreTake(self->adc_mutex_, portMAX_DELAY) == pdTRUE) {
                ESP_ERROR_CHECK(adc_oneshot_read(self->adc_handle_, BATTERY_ADC_CHANNEL, &battery_raw));
                ESP_ERROR_CHECK(adc_oneshot_read(self->adc_handle_, BATTERY_REF_ADC_CHANNEL, &ref_raw));
                xSemaphoreGive(self->adc_mutex_);
            }

            if (self->adc_cali_handle_) {
                ESP_ERROR_CHECK(adc_cali_raw_to_voltage(self->adc_cali_handle_, battery_raw, &battery_voltage));
                ESP_ERROR_CHECK(adc_cali_raw_to_voltage(self->adc_cali_handle_, ref_raw, &ref_voltage));
                self->UpdateBatteryState(battery_raw, battery_voltage, ref_raw, ref_voltage);
            }
            vTaskDelay(pdMS_TO_TICKS(30));
        }
    }

    static void volume_key_task(void* arg) {
        zhengchen_minicam* self = static_cast<zhengchen_minicam*>(arg);
        int volume_key_raw = 0;
        int volume_key_voltage = 0;

        while (1) {
            if (xSemaphoreTake(self->adc_mutex_, portMAX_DELAY) == pdTRUE) {
                ESP_ERROR_CHECK(adc_oneshot_read(self->adc_handle_, VOLUME_KEY_ADC_CHANNEL, &volume_key_raw));
                xSemaphoreGive(self->adc_mutex_);
            }

            if (self->adc_cali_handle_) {
                ESP_ERROR_CHECK(adc_cali_raw_to_voltage(self->adc_cali_handle_, volume_key_raw, &volume_key_voltage));
                self->HandleAdcVolumeKey(volume_key_voltage);
            } else {
                self->HandleAdcVolumeKey(0);
            }
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }

    void InitializeADC() {
        // 配置 ADC
        adc_oneshot_unit_init_cfg_t init_config = {
            .unit_id = ADC_UNIT_1,
            .ulp_mode = ADC_ULP_MODE_DISABLE,
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle_));

        // 配置 ADC 通道
        adc_oneshot_chan_cfg_t config = {
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle_, BATTERY_ADC_CHANNEL, &config));
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle_, BATTERY_REF_ADC_CHANNEL, &config));
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle_, VOLUME_KEY_ADC_CHANNEL, &config));

        // 尝试初始化校准 (ESP32-S3 使用 curve_fitting)
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = ADC_UNIT_1,
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle_);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "ADC calibration initialized");
        } else {
            ESP_LOGW(TAG, "ADC calibration not available");
            adc_cali_handle_ = nullptr;
        }

        adc_mutex_ = xSemaphoreCreateMutex();
        assert(adc_mutex_ != nullptr);

        // 创建 ADC 读取任务
        xTaskCreate(adc_read_task, "adc_read", 4096, this, 5, &adc_task_handle_);
        xTaskCreatePinnedToCore(volume_key_task, "volume_key", 4096, this, 6, &volume_key_task_handle_, 1);
        ESP_LOGI(TAG, "ADC monitoring task started for IO3/IO4 battery sampling");
        ESP_LOGI(TAG, "Volume key task started on CPU1");
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_CLK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeLcdDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;

        if (esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create panel IO");
            display_ = new NoDisplay();
            return;
        }

        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;

        if (esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create panel");
            display_ = new NoDisplay();
            return;
        }

        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);

        display_ = new ZhengchenMinicamLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                    DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
                                    DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y,
                                    DISPLAY_SWAP_XY);
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto* display = GetMinicamDisplay();
            if (display != nullptr && display->IsSettingsMenuVisible()) {
                display->ActivateSelectedMenuItem();
                return;
            }

            auto& app = Application::GetInstance();
            app.ToggleChatState();
        });

        boot_button_.OnDoubleClick([this]() {
            ToggleSettingsMenu();
        });
    }

    void InitializeCamera() {
        ESP_LOGI(TAG, "Waiting for camera power stabilization...");
        vTaskDelay(pdMS_TO_TICKS(2000));

        ESP_LOGI(TAG, "Initializing camera...");

        camera_config_t config = {};
        config.ledc_channel = LEDC_CHANNEL_2;
        config.ledc_timer   = LEDC_TIMER_2;
        config.pin_d0       = CAMERA_PIN_D0;
        config.pin_d1       = CAMERA_PIN_D1;
        config.pin_d2       = CAMERA_PIN_D2;
        config.pin_d3       = CAMERA_PIN_D3;
        config.pin_d4       = CAMERA_PIN_D4;
        config.pin_d5       = CAMERA_PIN_D5;
        config.pin_d6       = CAMERA_PIN_D6;
        config.pin_d7       = CAMERA_PIN_D7;
        config.pin_xclk     = CAMERA_PIN_XCLK;
        config.pin_pclk     = CAMERA_PIN_PCLK;
        config.pin_vsync    = CAMERA_PIN_VSYNC;
        config.pin_href     = CAMERA_PIN_HREF;
        config.pin_sccb_sda = -1;
        config.pin_sccb_scl = -1;
        config.sccb_i2c_port= I2C_NUM_0;
        config.pin_pwdn     = CAMERA_PIN_PWDN;
        config.pin_reset    = CAMERA_PIN_RESET;
        config.xclk_freq_hz = 20000000;
        config.pixel_format = PIXFORMAT_RGB565;
        config.frame_size   = FRAMESIZE_VGA;
        config.jpeg_quality = 12;
        config.fb_count     = 2;
        config.fb_location  = CAMERA_FB_IN_PSRAM;
        config.grab_mode    = CAMERA_GRAB_LATEST;

        camera_ = new Esp32Camera(config);

        sensor_t *s = esp_camera_sensor_get();
        if (s) {
            camera_sensor_info_t *info = esp_camera_sensor_get_info(&s->id);
            const char *sensor_name = info ? info->name : "Unknown";
            ESP_LOGI(TAG, "Camera initialized: %s, PID=0x%04x, VER=0x%02x", sensor_name, s->id.PID, s->id.VER);
        } else {
            ESP_LOGE(TAG, "Camera initialization failed - sensor not detected");
        }
    }

    void InitializeTools() {
        auto &mcp_server = McpServer::GetInstance();

        auto* minicam_display = GetMinicamDisplay();
        if (minicam_display != nullptr) {
            minicam_display->SetAecHandlers(
                []() -> bool {
                    return Application::GetInstance().GetAecMode() != kAecOff;
                },
                [this]() {
                    ToggleAec();
                });
            minicam_display->SetOrientationHandlers(
                [minicam_display]() -> bool {
                    return minicam_display->IsLandscape();
                },
                [this]() {
                    ToggleOrientation();
                });
            minicam_display->SetReprovisionCallback([this]() {
                ReprovisionNetwork();
            });
        }

        // 添加摄像头拍照工具
        mcp_server.AddTool("self.camera.capture",
            "Capture a photo with the camera and display it",
            PropertyList(),
            [this](const PropertyList& properties) {
                if (camera_ && camera_->Capture()) {
                    ESP_LOGI(TAG, "Camera capture successful");
                    return true;
                } else {
                    ESP_LOGE(TAG, "Camera capture failed");
                    return false;
                }
            });
    }

public:
    zhengchen_minicam()
        : boot_button_(BOOT_BUTTON_GPIO),
          volume_up_button_(VOLUME_UP_BUTTON_GPIO),
          volume_down_button_(VOLUME_DOWN_BUTTON_GPIO),
          camera_(nullptr),
          adc_handle_(nullptr),
          adc_cali_handle_(nullptr),
          adc_task_handle_(nullptr),
          volume_key_task_handle_(nullptr),
          adc_mutex_(nullptr),
          display_(nullptr) {
        InitializeStateOutput();
        InitializeI2c();
        InitializeADC();
        InitializeSpi();
        InitializeLcdDisplay();
        if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
            GetBacklight()->RestoreBrightness();
        }
        InitializeButtons();
        InitializeCamera();
        InitializeTools();
    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8388AudioCodec audio_codec(
            codec_i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE, AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8388_ADDR, AUDIO_INPUT_REFERENCE);
        return &audio_codec;
    }

    virtual Backlight* GetBacklight() override {
        if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
            static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
            return &backlight;
        }
        return nullptr;
    }

    virtual std::string GetDeviceStatusJson() override {
        auto root = cJSON_CreateObject();

        auto audio_speaker = cJSON_CreateObject();
        if (auto codec = GetAudioCodec()) {
            cJSON_AddNumberToObject(audio_speaker, "volume", codec->output_volume());
        }
        cJSON_AddItemToObject(root, "audio_speaker", audio_speaker);

        auto screen = cJSON_CreateObject();
        if (auto backlight = GetBacklight()) {
            cJSON_AddNumberToObject(screen, "brightness", backlight->brightness());
        }
        if (auto display = GetDisplay(); display && display->height() > 64) {
            if (auto theme = display->GetTheme()) {
                cJSON_AddStringToObject(screen, "theme", theme->name().c_str());
            }
        }
        cJSON_AddItemToObject(root, "screen", screen);

        auto network = cJSON_CreateObject();
        auto& wifi = WifiManager::GetInstance();
        cJSON_AddStringToObject(network, "type", "wifi");
        cJSON_AddStringToObject(network, "ssid", wifi.GetSsid().c_str());
        int rssi = wifi.GetRssi();
        const char* signal = rssi >= -60 ? "strong" : (rssi >= -70 ? "medium" : "weak");
        cJSON_AddStringToObject(network, "signal", signal);
        cJSON_AddItemToObject(root, "network", network);

        auto str = cJSON_PrintUnformatted(root);
        std::string result(str);
        cJSON_free(str);
        cJSON_Delete(root);
        return result;
    }

    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        level = battery_percent_;
        charging = is_charging_;
        discharging = !charging;
        return true;
    }

    virtual Camera* GetCamera() override {
        return camera_;
    }
};

DECLARE_BOARD(zhengchen_minicam);
