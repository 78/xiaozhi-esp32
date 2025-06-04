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
#include "display/matrix_display.h"

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
    Display* display_;
    Button boot_button_;
    Esp32Camera* camera_;

    void InitializeDisplay() {
        display_ = new MatrixDisplay();
    }

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

        config.pin_pwdn = CAM_PIN_PWDN;
        config.pin_reset = CAM_PIN_RESET;
        config.pin_xclk = CAM_PIN_XCLK;
        config.pin_sccb_sda = CAM_PIN_SIOD;
        config.pin_sccb_scl = CAM_PIN_SIOC;

        config.pin_d7 = CAM_PIN_D9;
        config.pin_d6 = CAM_PIN_D8;
        config.pin_d5 = CAM_PIN_D7;
        config.pin_d4 = CAM_PIN_D6;
        config.pin_d3 = CAM_PIN_D5;
        config.pin_d2 = CAM_PIN_D4;
        config.pin_d1 = CAM_PIN_D3;
        config.pin_d0 = CAM_PIN_D2;
        config.pin_vsync = CAM_PIN_VSYNC;
        config.pin_href = CAM_PIN_HREF;
        config.pin_pclk = CAM_PIN_PCLK;

        /* XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental) */
        config.xclk_freq_hz = XCLK_FREQ_HZ;
        config.ledc_timer = LEDC_TIMER_1;
        config.ledc_channel = LEDC_CHANNEL_1;
        config.pixel_format = PIXFORMAT_RGB565;   /* YUV422,GRAYSCALE,RGB565,JPEG */
        config.frame_size = FRAMESIZE_HVGA;       /* QQVGA-UXGA, For ESP32, do not use sizes above QVGA when not JPEG. The performance of the ESP32-S series has improved a lot, but JPEG mode always gives better frame rates */
        config.jpeg_quality = 12;                 /* 0-63, for OV series camera sensors, lower number means higher quality */
        config.fb_count = 1;                      /* When jpeg mode is used, if fb_count more than one, the driver will work in continuous mode */
        config.fb_location = CAMERA_FB_IN_PSRAM;
        config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

        esp_err_t err = esp_camera_init(&config); // 测试相机是否存在
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Camera is not plugged in or not supported, error: %s", esp_err_to_name(err));
            // 如果摄像头初始化失败，设置 camera_ 为 nullptr
            camera_ = nullptr;
            return;
        } else {
            esp_camera_deinit();// 释放之前的摄像头资源,为正确初始化做准备
            camera_ = new Esp32Camera(config);
        }
    }

public:
    Esp32S3TotoMbBoard() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeButtons();
        InitializeGpio();
        InitializeIot();
        InitializeCamera();
        InitializeDisplay();
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
    virtual Display* GetDisplay() override {
        return display_;
    }
};

DECLARE_BOARD(Esp32S3TotoMbBoard);
