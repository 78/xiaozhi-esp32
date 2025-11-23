#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include "lvgl_display.h"
#include "gif/lvgl_gif.h"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <font_emoji.h>

#include <atomic>
#include <memory>

#define PREVIEW_IMAGE_DURATION_MS 5000

// Theme color structure
struct ThemeColors {
    lv_color_t background;
    lv_color_t text;
    lv_color_t chat_background;
    lv_color_t user_bubble;
    lv_color_t assistant_bubble;
    lv_color_t system_bubble;
    lv_color_t system_text;
    lv_color_t border;
    lv_color_t low_battery;
};

class LcdDisplay : public LvglDisplay {
protected:
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    
    lv_draw_buf_t draw_buf_;
    lv_obj_t* status_bar_ = nullptr;
    lv_obj_t* content_ = nullptr;
    lv_obj_t* container_ = nullptr;
    lv_obj_t* side_bar_ = nullptr;
    lv_obj_t* preview_image_ = nullptr;
    lv_obj_t* emoji_label_ = nullptr;
    lv_obj_t* emoji_image_ = nullptr;
    std::unique_ptr<LvglGif> gif_controller_ = nullptr;
    lv_obj_t* emoji_box_ = nullptr;
    lv_obj_t* chat_message_label_ = nullptr;
    esp_timer_handle_t preview_timer_ = nullptr;
    std::unique_ptr<LvglImage> preview_image_cached_ = nullptr;
    std::string ip_address_;

    void InitializeLcdThemes();
    void SetupUI();
    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;
   
    // FFT handling methods
    void processAudioData();
    void periodicUpdateTask();
    static void periodicUpdateTaskWrapper(void* arg);
    int16_t* final_pcm_data_fft = nullptr;
    int16_t* audio_data_ = nullptr;
    int16_t* frame_audio_data = nullptr;
    uint32_t last_fft_update = 0;
    bool fft_data_ready = false;
    float* spectrum_data = nullptr;
    int audio_display_last_update = 0;
    std::atomic<bool> fft_task_should_stop = false;
    TaskHandle_t fft_task_handle = nullptr;
    float* fft_real;
    float* fft_imag;
    float* hanning_window_float;
    void compute(float* real, float* imag, int n, bool forward);
    void drawSpectrumIfReady();
    uint16_t get_bar_color(int x_pos);
    void draw_spectrum(float *power_spectrum, int fft_size);
    void draw_bar(int x, int y, int bar_width, int bar_height, uint16_t color, int bar_index);
    void draw_block(int x, int y, int block_x_size, int block_y_size, uint16_t color, int bar_index);

    // LVGL variables for FFT canvas or QR code
    int canvas_width_;
    int canvas_height_;
    lv_obj_t* canvas_ = nullptr;
    uint16_t* canvas_buffer_ = nullptr;
    void create_canvas(int32_t status_bar_height = 0);

    // Qr code handling methods
    bool qr_code_displayed_ = false;

    // Rotate and offset settings
    int rotation_degree_ = 0;
    void SetRotationAndOffset(lv_display_rotation_t rotation, int offset_x, int offset_y);

protected:
    // Add protected constructor
    LcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width, int height);
    
public:
    ~LcdDisplay();
    virtual void SetEmotion(const char* emotion) override;
    virtual void SetMusicInfo(const char* song_name) override;
    virtual void SetChatMessage(const char* role, const char* content) override; 
    virtual void SetPreviewImage(std::unique_ptr<LvglImage> image) override;

    // Add theme switching function
    virtual void SetTheme(Theme* theme) override;

    // FFT display methods
    virtual void StopFFT() override;
    virtual void StartFFT() override;
    virtual void FeedAudioDataFFT(int16_t* data, size_t sample_count) override;
    virtual int16_t* MakeAudioBuffFFT(size_t sample_count) override;
    virtual void ReleaseAudioBuffFFT(int16_t* buffer = nullptr) override;

    // QR code display methods
    virtual void DisplayQRCode(const uint8_t* qrcode, const char* text = nullptr) override;
    virtual void ClearQRCode() override;
    virtual void SetIpAddress(const std::string& ip_address) override;

    // Rotate lcd display
    virtual bool SetRotation(int rotation_degree, bool save_setting) override;
};

// SPI LCD Display
class SpiLcdDisplay : public LcdDisplay {
public:
    SpiLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                  int width, int height, int offset_x, int offset_y,
                  bool mirror_x, bool mirror_y, bool swap_xy);
};

// RGB LCD Display
class RgbLcdDisplay : public LcdDisplay {
public:
    RgbLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                  int width, int height, int offset_x, int offset_y,
                  bool mirror_x, bool mirror_y, bool swap_xy);
};

// MIPI LCD Display
class MipiLcdDisplay : public LcdDisplay {
public:
    MipiLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                   int width, int height, int offset_x, int offset_y,
                   bool mirror_x, bool mirror_y, bool swap_xy);
};

#endif // LCD_DISPLAY_H
