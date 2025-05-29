#include "wifi_board.h"
#include "audio_codecs/no_audio_codec.h"
#include "button.h"
#include "config.h"
#include "iot/thing_manager.h"
#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/gpio.h>
#include "esp32_camera.h"
#include "esp_sleep.h"
#include "esp_check.h"

#define TAG "Esp32S3TotoMbBoard"
static void light_sleep_task(void *args) {
    while (true)
    {
        if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO) {
            ESP_LOGI(TAG, "wakeup: %d\n", gpio_get_level(GPIO_NUM_9));
            gpio_set_level(GPIO_NUM_8, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    vTaskDelete(NULL);
}
class Esp32S3TotoMbBoard : public WifiBoard {
private:
    Button boot_button_;
    Esp32Camera* camera_;
    

    void InitializeButtons() {
        boot_button_.OnClick([]() {
            auto screenLevel = gpio_get_level(GPIO_NUM_8);
            if (screenLevel == 0) {
                // opened
                ESP_LOGI(TAG, "sleeping: %d\n", gpio_get_level(GPIO_NUM_9));
                gpio_set_level(GPIO_NUM_8, 1);
                esp_light_sleep_start();
            }
        });
    }

    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("MatrixScreen"));
    }


    void InitializeGpio() {
        gpio_reset_pin(GPIO_NUM_8);
        gpio_config_t config = {
            .pin_bit_mask = BIT64(GPIO_NUM_8),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_ENABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&config);
        gpio_set_level(GPIO_NUM_8, 0);

        ESP_LOGI(TAG, "Enable gpio wakeup status: %d",gpio_wakeup_enable(GPIO_NUM_9, GPIO_INTR_LOW_LEVEL));
        ESP_LOGI(TAG, "Configure gpio as wakeup source status: %d",esp_sleep_enable_gpio_wakeup());
        xTaskCreate(light_sleep_task, "light_sleep_task", 2048, NULL, 5, NULL);

    }

    void InitializeCamera() {

        camera_config_t config = {};
        config.ledc_channel = LEDC_CHANNEL_2;   // LEDC通道选择  用于生成XCLK时钟 但是S3不用
        config.ledc_timer = LEDC_TIMER_2;       // LEDC timer选择  用于生成XCLK时钟 但是S3不用
        config.pin_d0 = CAMERA_PIN_D2;
        config.pin_d1 = CAMERA_PIN_D3;
        config.pin_d2 = CAMERA_PIN_D4;
        config.pin_d3 = CAMERA_PIN_D5;
        config.pin_d4 = CAMERA_PIN_D6;
        config.pin_d5 = CAMERA_PIN_D7;
        config.pin_d6 = CAMERA_PIN_D8;
        config.pin_d7 = CAMERA_PIN_D9;
        config.pin_xclk = CAMERA_PIN_XCLK;
        config.pin_pclk = CAMERA_PIN_PCLK;
        config.pin_vsync = CAMERA_PIN_VSYNC;
        config.pin_href = CAMERA_PIN_HREF;
        config.pin_sccb_sda = CAMERA_PIN_SIOD;  // 这里如果写-1 表示使用已经初始化的I2C接口
        config.pin_sccb_scl = CAMERA_PIN_SIOC;
        config.pin_pwdn = CAMERA_PIN_PWDN;
        config.pin_reset = CAMERA_PIN_RESET;
        config.xclk_freq_hz = XCLK_FREQ_HZ;
        config.pixel_format = PIXFORMAT_RGB565;
        config.frame_size = FRAMESIZE_VGA;
        config.jpeg_quality = 12;
        config.fb_count = 1;
        config.fb_location = CAMERA_FB_IN_PSRAM;
        config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
        camera_ = new Esp32Camera(config);
    }

public:
    Esp32S3TotoMbBoard() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeButtons();
        InitializeGpio();
        InitializeIot();
        InitializeCamera();
    }

    virtual AudioCodec* GetAudioCodec() override {
        #ifdef AUDIO_I2S_METHOD_SIMPLEX
            static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
        #else
            static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
        #endif
        return &audio_codec;
    }
    virtual Camera* GetCamera() override {
        return camera_;
    }
};

DECLARE_BOARD(Esp32S3TotoMbBoard);
