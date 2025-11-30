#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include "lvgl_display.h"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>

#define OLED_FFT_SIZE 256 // Giảm xuống 256 cho nhẹ OLED
class OledDisplay : public LvglDisplay {
private:
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;

    lv_obj_t* status_bar_ = nullptr;
    lv_obj_t* content_ = nullptr;
    lv_obj_t* content_left_ = nullptr;
    lv_obj_t* content_right_ = nullptr;
    lv_obj_t* container_ = nullptr;
    lv_obj_t* side_bar_ = nullptr;
    lv_obj_t *emotion_label_ = nullptr;
    lv_obj_t* chat_message_label_ = nullptr;

    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

    void SetupUI_128x64();
    void SetupUI_128x32();

    // FFT handling methods
    void SetupSpectrumUI();
    void DrawOledSpectrum(); // Hàm cập nhật giao diện

    lv_obj_t* spectrum_container_ = nullptr;
    std::vector<lv_obj_t*> spectrum_bars_; // Dùng vector để quản lý 16 cột sóng

    // Các biến xử lý Audio & FFT (Copy từ LCD sang)
    TaskHandle_t fft_task_handle = nullptr;
    bool fft_task_should_stop = false;
    static void periodicUpdateTaskWrapper(void* arg);
    void periodicUpdateTask();
    void processAudioData();
    void compute(float* real, float* imag, int n, bool forward); // Hàm tính toán FFT

    // Buffer dữ liệu
    int16_t* final_pcm_data_fft = nullptr;
    int16_t* audio_data_ = nullptr;
    int16_t* frame_audio_data = nullptr;
    int audio_display_last_update = 0;
    bool fft_data_ready = false;
    
    // Mảng FFT
    float* fft_real = nullptr;
    float* fft_imag = nullptr;
    float* hanning_window_float = nullptr;
    float avg_power_spectrum[OLED_FFT_SIZE / 2] = {0};

    // QR code handling
    lv_obj_t* qr_canvas_ = nullptr;
    uint8_t* qr_canvas_buffer_ = nullptr;
    bool qr_code_displayed_ = false;
    std::string ip_address_;

public:
    OledDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width, int height, bool mirror_x, bool mirror_y);
    ~OledDisplay();

    virtual void SetMusicInfo(const char* song_name) override;
    virtual void SetChatMessage(const char* role, const char* content) override;
    virtual void SetEmotion(const char* emotion) override;
    virtual void SetTheme(Theme* theme) override;

    // FFT display methods
    int16_t* MakeAudioBuffFFT(size_t sample_count) override;
    void FeedAudioDataFFT(int16_t* data, size_t sample_count) override;
    void ReleaseAudioBuffFFT(int16_t* buffer) override;
    void StartFFT() override; // Hàm bắt đầu task FFT
    void StopFFT() override;  // Hàm dừng task FFT

    // QR code display methods
    void DisplayQRCode(const uint8_t* qrcode, const char* text = nullptr) override;
    void ClearQRCode() override;
    bool QRCodeIsSupported() override;
    void SetIpAddress(const std::string& ip_address) override;
};

#endif // OLED_DISPLAY_H
