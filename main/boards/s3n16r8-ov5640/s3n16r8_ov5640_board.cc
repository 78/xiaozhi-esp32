#include "wifi_board.h"
#include "audio_codecs/no_audio_codec.h"
#include "display/lcd_display.h"
#include "esp_log.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "i2s_std.h"
#include "esp_camera.h"

static const char* TAG = "S3N16R8-OV5640";

#define DISPLAY_SPI_SCK     19
#define DISPLAY_SPI_SDA     20
#define DISPLAY_RST         21
#define DISPLAY_DC          47
#define DISPLAY_CS          45
#define DISPLAY_BL          48
#define DISPLAY_WIDTH       160
#define DISPLAY_HEIGHT      128

#define AUDIO_I2S_DIN       41
#define AUDIO_I2S_BCLK      14
#define AUDIO_I2S_LRCK      46

#define MIC_I2S_SD          42
#define MIC_I2S_SCK         2
#define MIC_I2S_WS          1

#define CAM_PIN_PWDN        -1
#define CAM_PIN_RESET       -1
#define CAM_PIN_XCLK        15
#define CAM_PIN_SIOD        4
#define CAM_PIN_SIOC        5
#define CAM_PIN_D7          16
#define CAM_PIN_D6          17
#define CAM_PIN_D5          18
#define CAM_PIN_D4          12
#define CAM_PIN_D3          10
#define CAM_PIN_D2          8
#define CAM_PIN_D1          9
#define CAM_PIN_D0          11
#define CAM_PIN_VSYNC       6
#define CAM_PIN_HREF        7
#define CAM_PIN_PCLK        13

static esp_err_t camera_init_custom() {
    camera_config_t config = {};
    config.ledc_channel     = LEDC_CHANNEL_0;
    config.ledc_timer       = LEDC_TIMER_0;
    config.pin_d0           = CAM_PIN_D0;
    config.pin_d1           = CAM_PIN_D1;
    config.pin_d2           = CAM_PIN_D2;
    config.pin_d3           = CAM_PIN_D3;
    config.pin_d4           = CAM_PIN_D4;
    config.pin_d5           = CAM_PIN_D5;
    config.pin_d6           = CAM_PIN_D6;
    config.pin_d7           = CAM_PIN_D7;
    config.pin_xclk         = CAM_PIN_XCLK;
    config.pin_pclk         = CAM_PIN_PCLK;
    config.pin_vsync        = CAM_PIN_VSYNC;
    config.pin_href         = CAM_PIN_HREF;
    config.pin_sccb_sda     = CAM_PIN_SIOD;
    config.pin_sccb_scl     = CAM_PIN_SIOC;
    config.pin_pwdn         = CAM_PIN_PWDN;
    config.pin_reset        = CAM_PIN_RESET;
    config.xclk_freq_hz     = 20000000;
    config.pixel_format     = PIXFORMAT_JPEG;
    config.frame_size       = FRAMESIZE_VGA;
    config.jpeg_quality     = 12;
    config.fb_count         = 2;
    config.fb_location      = CAMERA_FB_IN_PSRAM;
    config.grab_mode        = CAMERA_GRAB_WHEN_EMPTY;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed: 0x%x", err);
        return err;
    }
    sensor_t* s = esp_camera_sensor_get();
    if (s != NULL) {
        s->set_whitebal(s, 1);
        s->set_awb_gain(s, 1);
        s->set_exposure_ctrl(s, 1);
        s->set_gain_ctrl(s, 1);
        s->set_raw_gma(s, 1);
        s->set_lenc(s, 1);
    }
    ESP_LOGI(TAG, "Camera init OK");
    return ESP_OK;
}

class S3N16R8Ov5640Board : public WifiBoard {
private:
    LcdDisplay* display_;

    void InitializeDisplay() {
        display_ = new SpiLcdDisplay(
            DISPLAY_SPI_SCK, DISPLAY_SPI_SDA, -1,
            DISPLAY_CS, DISPLAY_DC, DISPLAY_RST, DISPLAY_BL,
            DISPLAY_WIDTH, DISPLAY_HEIGHT,
            0, 0, false, false, false
        );
    }

    void InitializeCamera() {
        camera_init_custom();
    }

public:
    S3N16R8Ov5640Board() : display_(nullptr) {}

    void Initialize() override {
        ESP_LOGI(TAG, "=== S3N16R8 OV5640 Board Init ===");
        InitializeDisplay();
        InitializeCamera();
        WifiBoard::Initialize();
    }

    AudioCodec* GetAudioCodec() override {
        static NoAudioCodecDuplex* codec = new NoAudioCodecDuplex(
            I2S_NUM_0,
            AUDIO_I2S_BCLK,
            AUDIO_I2S_LRCK,
            AUDIO_I2S_DIN,
            MIC_I2S_SD,
            MIC_I2S_WS,
            MIC_I2S_SCK
        );
        return codec;
    }

    Display* GetDisplay() override {
        return display_;
    }

    bool HasCamera() override {
        return true;
    }
};

DECLARE_BOARD(S3N16R8Ov5640Board);
