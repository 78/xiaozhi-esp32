/**
 * Custom Board: ESP32-S3-CAM N16R8 + OV5640
 * 
 * Peripherals:
 *   - Display:    1.8" ST7735 (160x128) via SPI
 *   - Amplifier:  MAX98357A (I2S)
 *   - Mic:        INMP441 (I2S)
 *   - Camera:     OV5640 5MP (ESP32-S3-EYE pinout)
 */

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

// ─────────────────────────────────────────────
//  DISPLAY PINS  (1.8" ST7735 160x128)
// ─────────────────────────────────────────────
#define DISPLAY_SPI_SCK     19
#define DISPLAY_SPI_SDA     20
#define DISPLAY_RST         21
#define DISPLAY_DC          47
#define DISPLAY_CS          45
#define DISPLAY_BL          48
#define DISPLAY_WIDTH       160
#define DISPLAY_HEIGHT      128

// ─────────────────────────────────────────────
//  AMPLIFIER PINS  (MAX98357A)
// ─────────────────────────────────────────────
#define AUDIO_I2S_DIN       41
#define AUDIO_I2S_BCLK      14
#define AUDIO_I2S_LRCK      46

// ─────────────────────────────────────────────
//  MIC PINS  (INMP441)
// ─────────────────────────────────────────────
#define MIC_I2S_SD          42
#define MIC_I2S_SCK         2
#define MIC_I2S_WS          1

// ─────────────────────────────────────────────
//  CAMERA PINS  (OV5640 - ESP32-S3-EYE layout)
//  This is the standard pinout for ESP32-S3-CAM
//  N16R8 boards sold on AliExpress/eBay
// ─────────────────────────────────────────────
#define CAM_PIN_PWDN        -1
#define CAM_PIN_RESET       -1
#define CAM_PIN_XCLK        15
#define CAM_PIN_SIOD        4     // I2C SDA
#define CAM_PIN_SIOC        5     // I2C SCL
#define CAM_PIN_D7          16    // Y9
#define CAM_PIN_D6          17    // Y8
#define CAM_PIN_D5          18    // Y7
#define CAM_PIN_D4          12    // Y6
#define CAM_PIN_D3          10    // Y5
#define CAM_PIN_D2          8     // Y4
#define CAM_PIN_D1          9     // Y3
#define CAM_PIN_D0          11    // Y2
#define CAM_PIN_VSYNC       6
#define CAM_PIN_HREF        7
#define CAM_PIN_PCLK        13


// ─────────────────────────────────────────────
//  CAMERA INIT
// ─────────────────────────────────────────────
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
    config.xclk_freq_hz     = 20000000;         // 20 MHz
    config.pixel_format     = PIXFORMAT_JPEG;   // Required for OV5640
    config.frame_size       = FRAMESIZE_VGA;    // 640x480 for Xiaozhi AI chat
    config.jpeg_quality     = 12;               // 0-63, lower = better quality
    config.fb_count         = 2;               // Double buffer (needs PSRAM)
    config.fb_location      = CAMERA_FB_IN_PSRAM;
    config.grab_mode        = CAMERA_GRAB_WHEN_EMPTY;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed: 0x%x", err);
        return err;
    }

    // OV5640 sensor tuning
    sensor_t* s = esp_camera_sensor_get();
    if (s != NULL) {
        s->set_brightness(s, 0);
        s->set_contrast(s, 0);
        s->set_saturation(s, 0);
        s->set_special_effect(s, 0);    // No effect
        s->set_whitebal(s, 1);          // Auto white balance ON
        s->set_awb_gain(s, 1);
        s->set_wb_mode(s, 0);           // Auto
        s->set_exposure_ctrl(s, 1);     // Auto exposure ON
        s->set_aec2(s, 0);
        s->set_gain_ctrl(s, 1);         // Auto gain ON
        s->set_agc_gain(s, 0);
        s->set_gainceiling(s, (gainceiling_t)0);
        s->set_bpc(s, 0);
        s->set_wpc(s, 1);
        s->set_raw_gma(s, 1);
        s->set_lenc(s, 1);
        s->set_hmirror(s, 0);
        s->set_vflip(s, 0);
        s->set_dcw(s, 1);
        s->set_colorbar(s, 0);
        ESP_LOGI(TAG, "OV5640 sensor configured successfully");
    }

    ESP_LOGI(TAG, "Camera init OK");
    return ESP_OK;
}


// ─────────────────────────────────────────────
//  BOARD CLASS
// ─────────────────────────────────────────────
class S3N16R8Ov5640Board : public WifiBoard {
private:
    LcdDisplay* display_;

    void InitializeDisplay() {
        ESP_LOGI(TAG, "Initializing ST7735 display...");
        display_ = new SpiLcdDisplay(
            DISPLAY_SPI_SCK,
            DISPLAY_SPI_SDA,
            -1,           // MISO not used
            DISPLAY_CS,
            DISPLAY_DC,
            DISPLAY_RST,
            DISPLAY_BL,
            DISPLAY_WIDTH,
            DISPLAY_HEIGHT,
            0,            // offset_x
            0,            // offset_y
            false,        // mirror_x
            false,        // mirror_y
            false         // swap_xy
        );
    }

    void InitializeAudio() {
        ESP_LOGI(TAG, "Initializing I2S audio (MAX98357A + INMP441)...");
    }

    void InitializeCamera() {
        ESP_LOGI(TAG, "Initializing OV5640 camera...");
        esp_err_t ret = camera_init_custom();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Camera initialization failed!");
        }
    }

public:
    S3N16R8Ov5640Board() : display_(nullptr) {}

    void Initialize() override {
        ESP_LOGI(TAG, "=== S3N16R8 OV5640 Custom Board Init ===");
        InitializeDisplay();
        InitializeAudio();
        InitializeCamera();
        WifiBoard::Initialize();
    }

    AudioCodec* GetAudioCodec() override {
        // MAX98357A speaker (I2S output) + INMP441 mic (I2S input)
        static NoAudioCodecDuplex* codec = new NoAudioCodecDuplex(
            I2S_NUM_0,          // I2S port
            AUDIO_I2S_BCLK,     // BCLK  -> pin 14
            AUDIO_I2S_LRCK,     // LRCLK -> pin 46
            AUDIO_I2S_DIN,      // DOUT (speaker) -> pin 41
            MIC_I2S_SD,         // DIN  (mic)     -> pin 42
            MIC_I2S_WS,         // WS   (mic)     -> pin 1
            MIC_I2S_SCK         // SCK  (mic)     -> pin 2
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


// ─────────────────────────────────────────────
//  BOARD FACTORY — registers this board
// ─────────────────────────────────────────────
DECLARE_BOARD(S3N16R8Ov5640Board);
