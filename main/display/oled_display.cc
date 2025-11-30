#include "oled_display.h"
#include "assets/lang_config.h"
#include "lvgl_theme.h"
#include "lvgl_font.h"

#include <string>
#include <algorithm>

#include <esp_log.h>
#include <esp_err.h>
#include <esp_lvgl_port.h>
#include <font_awesome.h>
#include <vector>
#include <cmath>
#include <cstring>
#include <math.h>
#include <qrcode.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SPEC_BAR_COUNT 16
#define SPEC_BAR_WIDTH 6
#define SPEC_BAR_GAP 2
#define TAG "OledDisplay"

LV_FONT_DECLARE(BUILTIN_TEXT_FONT);
LV_FONT_DECLARE(BUILTIN_ICON_FONT);
LV_FONT_DECLARE(font_awesome_30_1);

void OledDisplay::SetupSpectrumUI() {
    DisplayLockGuard lock(this);
    if (spectrum_container_ != nullptr) {
        ESP_LOGW(TAG, "Spectrum UI already set up");
        return;
    }

    auto screen = lv_screen_active();
    spectrum_container_ = lv_obj_create(screen);
    if (spectrum_container_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create spectrum container");
        return;
    }
    lv_obj_set_size(spectrum_container_, LV_HOR_RES, LV_VER_RES - 16); 
    lv_obj_align(spectrum_container_, LV_ALIGN_BOTTOM_MID, 0, 0);
    
    lv_obj_set_style_border_width(spectrum_container_, 0, 0);       // Không viền
    lv_obj_set_style_radius(spectrum_container_, 0, 0);             // Không bo tròn
    lv_obj_set_style_bg_color(spectrum_container_, lv_color_white(), 0); // Nền đen
    lv_obj_set_style_bg_opa(spectrum_container_, LV_OPA_COVER, 0);   // Phủ kín
    
    lv_obj_set_style_pad_all(spectrum_container_, 0, 0);
    lv_obj_set_style_pad_column(spectrum_container_, 1, 0); 
    
    lv_obj_set_flex_flow(spectrum_container_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(spectrum_container_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    
    lv_obj_add_flag(spectrum_container_, LV_OBJ_FLAG_HIDDEN); // Mặc định ẩn

    spectrum_bars_.clear();
    for (int i = 0; i < SPEC_BAR_COUNT; i++) {
        lv_obj_t* bar = lv_bar_create(spectrum_container_);
        lv_obj_set_size(bar, SPEC_BAR_WIDTH, LV_VER_RES - 16); 
        
        lv_bar_set_range(bar, 0, 100); // Giá trị từ 0% đến 100%
        lv_bar_set_value(bar, 0, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(bar, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN); // Đảm bảo phủ kín
        
        lv_obj_set_style_bg_color(bar, lv_color_black(), LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR); // Đảm bảo phủ kín
        
        lv_obj_set_style_radius(bar, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(bar, 0, LV_PART_INDICATOR);

        spectrum_bars_.push_back(bar);
    }
    ESP_LOGI(TAG, "Spectrum UI setup completed with %d bars", SPEC_BAR_COUNT);
}
OledDisplay::OledDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
    int width, int height, bool mirror_x, bool mirror_y)
    : panel_io_(panel_io), panel_(panel) {
    width_ = width;
    height_ = height;
    
    final_pcm_data_fft = nullptr;
    audio_data_ = nullptr;
    frame_audio_data = nullptr;
    fft_real = nullptr;
    fft_imag = nullptr;
    hanning_window_float = nullptr;
    spectrum_container_ = nullptr;
    qr_canvas_ = nullptr;
    qr_canvas_buffer_ = nullptr;
    qr_code_displayed_ = false;

    auto text_font = std::make_shared<LvglBuiltInFont>(&BUILTIN_TEXT_FONT);
    auto icon_font = std::make_shared<LvglBuiltInFont>(&BUILTIN_ICON_FONT);
    auto large_icon_font = std::make_shared<LvglBuiltInFont>(&font_awesome_30_1);
    
    auto dark_theme = new LvglTheme("dark");
    dark_theme->set_text_font(text_font);
    dark_theme->set_icon_font(icon_font);
    dark_theme->set_large_icon_font(large_icon_font);

    auto& theme_manager = LvglThemeManager::GetInstance();
    theme_manager.RegisterTheme("dark", dark_theme);
    current_theme_ = dark_theme;

    ESP_LOGI(TAG, "Initialize LVGL");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 1;
    port_cfg.task_stack = 6144;
#if CONFIG_SOC_CPU_CORES_NUM > 1
    port_cfg.task_affinity = 1;
#endif
    lvgl_port_init(&port_cfg);

    ESP_LOGI(TAG, "Adding OLED display");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io_,
        .panel_handle = panel_,
        .control_handle = nullptr,
        .buffer_size = static_cast<uint32_t>(width_ * height_),
        .double_buffer = false,
        .trans_size = 0,
        .hres = static_cast<uint32_t>(width_),
        .vres = static_cast<uint32_t>(height_),
        .monochrome = true,
        .rotation = {
            .swap_xy = false,
            .mirror_x = mirror_x,
            .mirror_y = mirror_y,
        },
        .flags = {
            .buff_dma = 1,
            .buff_spiram = 0,
            .sw_rotate = 0,
            .full_refresh = 0,
            .direct_mode = 0,
        },
    };

    display_ = lvgl_port_add_disp(&display_cfg);
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add display");
        return;
    }

    fft_real = (float*)heap_caps_malloc(OLED_FFT_SIZE * sizeof(float), MALLOC_CAP_SPIRAM);
    fft_imag = (float*)heap_caps_malloc(OLED_FFT_SIZE * sizeof(float), MALLOC_CAP_SPIRAM);
    hanning_window_float = (float*)heap_caps_malloc(OLED_FFT_SIZE * sizeof(float), MALLOC_CAP_SPIRAM);

    for (int i = 0; i < OLED_FFT_SIZE; i++) {
        hanning_window_float[i] = 0.5 * (1.0 - cos(2.0 * M_PI * i / (OLED_FFT_SIZE - 1)));
    }
    
    audio_data_=(int16_t*)heap_caps_malloc(sizeof(int16_t)*1152, MALLOC_CAP_SPIRAM);
    if(audio_data_!=nullptr){
        ESP_LOGI(TAG, "audio_data_ allocated");
        memset(audio_data_,0,sizeof(int16_t)*1152);
    } else {
        ESP_LOGE(TAG, "Failed to allocate audio_data_");
    }
    frame_audio_data=(int16_t*)heap_caps_malloc(sizeof(int16_t)*1152, MALLOC_CAP_SPIRAM);
    if(frame_audio_data!=nullptr){
        ESP_LOGI(TAG, "frame_audio_data allocated");
        memset(frame_audio_data,0,sizeof(int16_t)*1152);
    } else {
        ESP_LOGE(TAG, "Failed to allocate frame_audio_data");
    }
    ESP_LOGI(TAG,"Initialize fft_input, audio_data_, frame_audio_data, spectrum_data");
    

    if (height_ == 64) {
        SetupUI_128x64();
    } else {
        SetupUI_128x32();
    }
    SetupSpectrumUI();
}

OledDisplay::~OledDisplay() {
    if (content_ != nullptr) {
        lv_obj_del(content_);
    }
    if (status_bar_ != nullptr) {
        lv_obj_del(status_bar_);
    }
    if (side_bar_ != nullptr) {
        lv_obj_del(side_bar_);
    }
    if (container_ != nullptr) {
        lv_obj_del(container_);
    }

    // Clean up QR code resources
    if (qr_canvas_ != nullptr) {
        lv_obj_del(qr_canvas_);
    }
    if (qr_canvas_buffer_ != nullptr) {
        heap_caps_free(qr_canvas_buffer_);
    }

    if (panel_ != nullptr) {
        esp_lcd_panel_del(panel_);
    }
    if (panel_io_ != nullptr) {
        esp_lcd_panel_io_del(panel_io_);
    }
    lvgl_port_deinit();
}

bool OledDisplay::Lock(int timeout_ms) {
    return lvgl_port_lock(timeout_ms);
}

void OledDisplay::Unlock() {
    lvgl_port_unlock();
}

void OledDisplay::SetChatMessage(const char* role, const char* content) {
    DisplayLockGuard lock(this);
    if (chat_message_label_ == nullptr) {
        return;
    }

    // Replace all newlines with spaces
    std::string content_str = content;
    std::replace(content_str.begin(), content_str.end(), '\n', ' ');

    if (content_right_ == nullptr) {
        lv_label_set_text(chat_message_label_, content_str.c_str());
    } else {
        if (content == nullptr || content[0] == '\0') {
            lv_obj_add_flag(content_right_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_label_set_text(chat_message_label_, content_str.c_str());
            lv_obj_remove_flag(content_right_, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void OledDisplay::SetupUI_128x64() {
    DisplayLockGuard lock(this);

    auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    auto text_font = lvgl_theme->text_font()->font();
    auto icon_font = lvgl_theme->icon_font()->font();
    auto large_icon_font = lvgl_theme->large_icon_font()->font();

    auto screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, text_font, 0);
    lv_obj_set_style_text_color(screen, lv_color_black(), 0);

    /* Container */
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_row(container_, 0, 0);

    /* Status bar */
    status_bar_ = lv_obj_create(container_);
    lv_obj_set_size(status_bar_, LV_HOR_RES, 16);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_radius(status_bar_, 0, 0);

    /* Content */
    content_ = lv_obj_create(container_);
    lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_radius(content_, 0, 0);
    lv_obj_set_style_pad_all(content_, 0, 0);
    lv_obj_set_width(content_, LV_HOR_RES);
    lv_obj_set_flex_grow(content_, 1);
    lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_flex_main_place(content_, LV_FLEX_ALIGN_CENTER, 0);

    // 创建左侧固定宽度的容器
    content_left_ = lv_obj_create(content_);
    lv_obj_set_size(content_left_, 32, LV_SIZE_CONTENT);  // 固定宽度32像素
    lv_obj_set_style_pad_all(content_left_, 0, 0);
    lv_obj_set_style_border_width(content_left_, 0, 0);

    emotion_label_ = lv_label_create(content_left_);
    lv_obj_set_style_text_font(emotion_label_, large_icon_font, 0);
    lv_label_set_text(emotion_label_, FONT_AWESOME_MICROCHIP_AI);
    lv_obj_center(emotion_label_);
    lv_obj_set_style_pad_top(emotion_label_, 8, 0);

    // 创建右侧可扩展的容器
    content_right_ = lv_obj_create(content_);
    lv_obj_set_size(content_right_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(content_right_, 0, 0);
    lv_obj_set_style_border_width(content_right_, 0, 0);
    lv_obj_set_flex_grow(content_right_, 1);
    lv_obj_add_flag(content_right_, LV_OBJ_FLAG_HIDDEN);

    chat_message_label_ = lv_label_create(content_right_);
    lv_label_set_text(chat_message_label_, "");
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_width(chat_message_label_, width_ - 32);
    lv_obj_set_style_pad_top(chat_message_label_, 14, 0);

    // 延迟一定的时间后开始滚动字幕
    static lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_delay(&a, 1000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_obj_set_style_anim(chat_message_label_, &a, LV_PART_MAIN);
    lv_obj_set_style_anim_duration(chat_message_label_, lv_anim_speed_clamped(60, 300, 60000), LV_PART_MAIN);

    /* Status bar */
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_column(status_bar_, 0, 0);

    network_label_ = lv_label_create(status_bar_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, icon_font, 0);

    notification_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(notification_label_, 1);
    lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(notification_label_, "");
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(status_label_, 1);
    lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);

    mute_label_ = lv_label_create(status_bar_);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, icon_font, 0);

    battery_label_ = lv_label_create(status_bar_);
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, icon_font, 0);

    low_battery_popup_ = lv_obj_create(screen);
    lv_obj_set_scrollbar_mode(low_battery_popup_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_size(low_battery_popup_, LV_HOR_RES * 0.9, text_font->line_height * 2);
    lv_obj_align(low_battery_popup_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(low_battery_popup_, lv_color_black(), 0);
    lv_obj_set_style_radius(low_battery_popup_, 10, 0);
    low_battery_label_ = lv_label_create(low_battery_popup_);
    lv_label_set_text(low_battery_label_, Lang::Strings::BATTERY_NEED_CHARGE);
    lv_obj_set_style_text_color(low_battery_label_, lv_color_white(), 0);
    lv_obj_center(low_battery_label_);
    lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);
}

void OledDisplay::periodicUpdateTaskWrapper(void* arg) {
    auto self = static_cast<OledDisplay*>(arg);
    self->periodicUpdateTask();
}

void OledDisplay::periodicUpdateTask() {
    ESP_LOGI(TAG, "FFT Task Started");

    const TickType_t displayInterval = pdMS_TO_TICKS(40);  // Display refresh interval (40ms)
    const TickType_t audioProcessInterval = pdMS_TO_TICKS(15); // Audio processing interval (15ms)
    
    TickType_t lastDisplayTime = xTaskGetTickCount();
    TickType_t lastAudioTime = xTaskGetTickCount();
    
    while (!fft_task_should_stop) {
        TickType_t currentTime = xTaskGetTickCount();
        
        // Process audio data at regular intervals
        if (currentTime - lastAudioTime >= audioProcessInterval) {
            if (final_pcm_data_fft != nullptr) {
                processAudioData();  // Quick processing, non-blocking
            } else {
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            lastAudioTime = currentTime;
        }

        // Cập nhật màn hình (30ms một lần ~ 30FPS)
        if (currentTime - lastDisplayTime >= displayInterval) {
            if (fft_data_ready) {
                DisplayLockGuard lock(this);
                DrawOledSpectrum(); // Vẽ lên màn hình
                fft_data_ready = false;
                lastDisplayTime = currentTime;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    ESP_LOGI(TAG, "FFT display task stopped");
    fft_task_handle = nullptr;  // Clear the task handle
    vTaskDelete(NULL);
}

void OledDisplay::DrawOledSpectrum() {
    if (spectrum_container_ == nullptr) {
        ESP_LOGW(TAG, "spectrum_container_ is nullptr");
        vTaskDelay(pdMS_TO_TICKS(1000));
        return;
    }

    if (lv_obj_has_flag(spectrum_container_, LV_OBJ_FLAG_HIDDEN)) {
        ESP_LOGI(TAG, "Showing spectrum container");
         lv_obj_remove_flag(spectrum_container_, LV_OBJ_FLAG_HIDDEN);
    }

    int samples_per_bar = (OLED_FFT_SIZE / 2) / SPEC_BAR_COUNT; 
    std::string debug_vals = "";

    for (int i = 0; i < SPEC_BAR_COUNT; i++) {
        float sum = 0;
        int count = 0;
        
        int start_idx = i * samples_per_bar;
        if (start_idx < 2) start_idx = 2; // Bỏ qua 2 mẫu tần số thấp

        for (int j = 0; j < samples_per_bar; j++) {
            if ((start_idx + j) < (OLED_FFT_SIZE/2)) {
                sum += avg_power_spectrum[start_idx + j];
                count++;
            }
        }
        
        float val = 0;
        if (count > 0) val = sum / count;

        int bar_val = 0;
        if (val > 0.000001f) { 
            float db = 10.0f * log10f(val);
            
            float min_db = -55.0f; // Giảm xuống (từ -50) để bắt được tín hiệu yếu hơn
            float max_db = 5.0f;   // Giảm xuống (từ 10) để cột sóng dễ đạt đỉnh hơn
            
            float percent = (db - min_db) / (max_db - min_db);
            if (percent < 0) percent = 0;
            if (percent > 1) percent = 1;
            
            bar_val = (int)(percent * 100.0f);
        }

        // Hiệu ứng rơi từ từ
        int old_val = lv_bar_get_value(spectrum_bars_[i]);
        if (bar_val < old_val) {
             bar_val = old_val - 4; 
             if (bar_val < 0) bar_val = 0;
        } else {
            // Hiệu ứng tăng nhanh (để sóng phản ứng tức thì)
            bar_val = std::max(bar_val, old_val);
        }


        if (i < 4) debug_vals += std::to_string(bar_val) + " ";

        if (i < spectrum_bars_.size()) {
            lv_bar_set_value(spectrum_bars_[i], bar_val, LV_ANIM_OFF);
        }
    }
    
    static int log_limit = 0;
    if (log_limit++ > 20) {
        ESP_LOGI("SpectrumDebug", "DB Val: %s", debug_vals.c_str());
        log_limit = 0;
    }
}

int16_t* OledDisplay::MakeAudioBuffFFT(size_t sample_count) {
    if (final_pcm_data_fft == nullptr) {
        final_pcm_data_fft = (int16_t *)heap_caps_malloc(sample_count, MALLOC_CAP_SPIRAM);
    }
    return final_pcm_data_fft;
}
/*
void OledDisplay::FeedAudioDataFFT(int16_t* data, size_t sample_count) {
    if (final_pcm_data_fft != nullptr) {
        memcpy(final_pcm_data_fft, data, sample_count);
    }
}
*/
void OledDisplay::FeedAudioDataFFT(int16_t* data, size_t sample_count) {
    if (final_pcm_data_fft != nullptr) {
        memcpy(final_pcm_data_fft, data, sample_count);
        
        // Thêm đoạn log debug này (nhớ xóa sau khi chạy được)
        static int debug_count = 0;
        if (debug_count++ > 100) {
            ESP_LOGI(TAG, "Dang nhan du lieu audio: %d bytes", sample_count);
            debug_count = 0;
        }
    } else {
        // Nếu buffer chưa được tạo, báo lỗi
        static int err_count = 0;
        if (err_count++ > 100) {
            ESP_LOGE(TAG, "Loi: final_pcm_data_fft la NULL!");
            err_count = 0;
        }
    }
}
void OledDisplay::StartFFT() {
    if (fft_task_handle != nullptr) return;
    fft_task_should_stop = false;
    xTaskCreate(periodicUpdateTaskWrapper, "oled_fft", 4096 * 2, this, 1, &fft_task_handle);
}

void OledDisplay::StopFFT() {
    ESP_LOGI(TAG, "Stopping FFT display");
    // Stop the FFT display task
    if (fft_task_handle != nullptr) {
        ESP_LOGI(TAG, "Stopping FFT display task");
        fft_task_should_stop = true;  // Set the stop flag
        
        // Wait for the task to stop (wait up to 1 second)
        int wait_count = 0;
        while (fft_task_handle != nullptr && wait_count < 100) {
            vTaskDelay(pdMS_TO_TICKS(10));
            wait_count++;
        }
        
        if (fft_task_handle != nullptr) {
            ESP_LOGW(TAG, "FFT task did not stop gracefully, force deleting");
            vTaskDelete(fft_task_handle);
            fft_task_handle = nullptr;
        } else {
            ESP_LOGI(TAG, "FFT display task stopped successfully");
        }
    }
    // Reset FFT state variables
    fft_data_ready = false;
    audio_display_last_update = 0;
    
    // Ẩn spectrum đi khi dừng
    DisplayLockGuard lock(this);
    if (spectrum_container_) {
        ESP_LOGI(TAG, "Hiding spectrum container");
        lv_obj_add_flag(spectrum_container_, LV_OBJ_FLAG_HIDDEN);
    }
}

void OledDisplay::ReleaseAudioBuffFFT(int16_t* buffer) {
    if (final_pcm_data_fft != nullptr) {
        heap_caps_free(final_pcm_data_fft);
        final_pcm_data_fft = nullptr;
    }
}

void OledDisplay::processAudioData() {
    // Logic xử lý buffer âm thanh -> FFT (rút gọn từ bản gốc)
    if (final_pcm_data_fft == nullptr) {
        ESP_LOGI(TAG, "audio_data_ is nullptr");
        vTaskDelay(pdMS_TO_TICKS(500));
        return;
    }

    if (audio_display_last_update <= 2) {
        if (audio_data_ == nullptr) {
            ESP_LOGI(TAG, "audio_data_ buffer is nullptr");
            vTaskDelay(pdMS_TO_TICKS(500));
            return;
        }
        memcpy(audio_data_, final_pcm_data_fft, sizeof(int16_t) * 1152);
        for (int i = 0; i < 1152; i++) frame_audio_data[i] += audio_data_[i];
        audio_display_last_update++;
    } else {
        // Thực hiện FFT
        const int HOP_SIZE = OLED_FFT_SIZE; // Tinh chỉnh theo OLED_FFT_SIZE
        int num_segments = (1152 - OLED_FFT_SIZE) / HOP_SIZE;
        if (num_segments < 1) num_segments = 1;

        // Reset mảng spectrum
        memset(avg_power_spectrum, 0, sizeof(avg_power_spectrum));

        for (int seg = 0; seg < num_segments; seg++) {
            int start = seg * HOP_SIZE;
            for (int i = 0; i < OLED_FFT_SIZE; i++) {
                float sample = frame_audio_data[start + i] / 32768.0f;
                fft_real[i] = sample * hanning_window_float[i];
                fft_imag[i] = 0.0f;
            }
            compute(fft_real, fft_imag, OLED_FFT_SIZE, true);
            
            for (int i = 0; i < OLED_FFT_SIZE / 2; i++) {
                avg_power_spectrum[i] += (fft_real[i] * fft_real[i] + fft_imag[i] * fft_imag[i]);
            }
        }
        
        audio_display_last_update = 0;
        fft_data_ready = true;
        memset(frame_audio_data, 0, sizeof(int16_t) * 1152);
    }
}

void OledDisplay::compute(float* real, float* imag, int n, bool forward) {
    // Hàm FFT tiêu chuẩn (Cooley-Tukey) - Copy y nguyên
    int j = 0;
    for (int i = 0; i < n; i++) {
        if (j > i) {
            std::swap(real[i], real[j]);
            std::swap(imag[i], imag[j]);
        }
        int m = n >> 1;
        while (m >= 1 && j >= m) { j -= m; m >>= 1; }
        j += m;
    }
    for (int s = 1; s <= (int)log2(n); s++) {
        int m = 1 << s;
        int m2 = m >> 1;
        float w_real = 1.0f, w_imag = 0.0f;
        float angle = (forward ? -2.0f : 2.0f) * M_PI / m;
        float wm_real = cosf(angle);
        float wm_imag = sinf(angle);
        for (int j2 = 0; j2 < m2; j2++) {
            for (int k = j2; k < n; k += m) {
                int k2 = k + m2;
                float t_real = w_real * real[k2] - w_imag * imag[k2];
                float t_imag = w_real * imag[k2] + w_imag * real[k2];
                real[k2] = real[k] - t_real;
                imag[k2] = imag[k] - t_imag;
                real[k] += t_real;
                imag[k] += t_imag;
            }
            float w_temp = w_real;
            w_real = w_real * wm_real - w_imag * wm_imag;
            w_imag = w_temp * wm_imag + w_imag * wm_real;
        }
    }
    if (forward) {
        for (int i = 0; i < n; i++) { real[i] /= n; imag[i] /= n; }
    }
}

void OledDisplay::SetupUI_128x32() {
    DisplayLockGuard lock(this);

    auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    auto text_font = lvgl_theme->text_font()->font();
    auto icon_font = lvgl_theme->icon_font()->font();
    auto large_icon_font = lvgl_theme->large_icon_font()->font();

    auto screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, text_font, 0);

    /* Container */
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_column(container_, 0, 0);

    /* Emotion label on the left side */
    content_ = lv_obj_create(container_);
    lv_obj_set_size(content_, 32, 32);
    lv_obj_set_style_pad_all(content_, 0, 0);
    lv_obj_set_style_border_width(content_, 0, 0);
    lv_obj_set_style_radius(content_, 0, 0);

    emotion_label_ = lv_label_create(content_);
    lv_obj_set_style_text_font(emotion_label_, large_icon_font, 0);
    lv_label_set_text(emotion_label_, FONT_AWESOME_MICROCHIP_AI);
    lv_obj_center(emotion_label_);

    /* Right side */
    side_bar_ = lv_obj_create(container_);
    lv_obj_set_size(side_bar_, width_ - 32, 32);
    lv_obj_set_flex_flow(side_bar_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(side_bar_, 0, 0);
    lv_obj_set_style_border_width(side_bar_, 0, 0);
    lv_obj_set_style_radius(side_bar_, 0, 0);
    lv_obj_set_style_pad_row(side_bar_, 0, 0);

    /* Status bar */
    status_bar_ = lv_obj_create(side_bar_);
    lv_obj_set_size(status_bar_, width_ - 32, 16);
    lv_obj_set_style_radius(status_bar_, 0, 0);
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_column(status_bar_, 0, 0);

    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(status_label_, 1);
    lv_obj_set_style_pad_left(status_label_, 2, 0);
    lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);

    notification_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(notification_label_, 1);
    lv_obj_set_style_pad_left(notification_label_, 2, 0);
    lv_label_set_text(notification_label_, "");
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    mute_label_ = lv_label_create(status_bar_);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, icon_font, 0);

    network_label_ = lv_label_create(status_bar_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, icon_font, 0);

    battery_label_ = lv_label_create(status_bar_);
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, icon_font, 0);

    chat_message_label_ = lv_label_create(side_bar_);
    lv_obj_set_size(chat_message_label_, width_ - 32, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_left(chat_message_label_, 2, 0);
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(chat_message_label_, "");

    // 延迟一定的时间后开始滚动字幕
    static lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_delay(&a, 1000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_obj_set_style_anim(chat_message_label_, &a, LV_PART_MAIN);
    lv_obj_set_style_anim_duration(chat_message_label_, lv_anim_speed_clamped(60, 300, 60000), LV_PART_MAIN);
}

void OledDisplay::SetEmotion(const char* emotion) {
    const char* utf8 = font_awesome_get_utf8(emotion);
    DisplayLockGuard lock(this);
    if (emotion_label_ == nullptr) {
        return;
    }
    if (utf8 != nullptr) {
        lv_label_set_text(emotion_label_, utf8);
    } else {
        lv_label_set_text(emotion_label_, FONT_AWESOME_NEUTRAL);
    }
}

void OledDisplay::SetTheme(Theme* theme) {
    DisplayLockGuard lock(this);

    auto lvgl_theme = static_cast<LvglTheme*>(theme);
    auto text_font = lvgl_theme->text_font()->font();

    auto screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, text_font, 0);
}

void OledDisplay::SetMusicInfo(const char* song_name) {
    // 默认实现：对于非微信模式，将歌名显示在聊天消息标签中
    DisplayLockGuard lock(this);
    if (chat_message_label_ == nullptr) {
        return;
    }
    if (song_name != nullptr && strlen(song_name) > 0) {
        std::string music_text = "";
        music_text += song_name;
        lv_label_set_text(chat_message_label_, music_text.c_str());
    } else {
        lv_label_set_text(chat_message_label_, "");
    }
}

void OledDisplay::DisplayQRCode(const uint8_t* qrcode, const char* text) {
    DisplayLockGuard lock(this);
    if (qrcode == nullptr) {
        ESP_LOGE(TAG, "QR code is null");
        return;
    }

    // Get QR code size
    int qr_size = esp_qrcode_get_size(qrcode);
    ESP_LOGI(TAG, "QR code size: %d, text: %s", qr_size, text != nullptr ? text : "123456789");

    // Calculate pixel size for OLED (smaller than LCD)
    int max_size = (width_ < height_ ? width_ : height_) - 10; // Leave margin
    int pixel_size = max_size / qr_size;
    if (pixel_size < 1) pixel_size = 1; // Minimum 1 pixel per module for OLED
    ESP_LOGI(TAG, "QR code pixel size: %d", pixel_size);

    // Create canvas if not exists
    if (qr_canvas_ == nullptr) {
        auto screen = lv_screen_active();
        
        // For OLED, use monochrome buffer (1 bit per pixel)
        int canvas_w = width_;
        int canvas_h = height_;
        size_t buf_size = LV_CANVAS_BUF_SIZE(canvas_w, canvas_h, 1, LV_COLOR_FORMAT_I1);
        
        qr_canvas_buffer_ = (uint8_t*)heap_caps_malloc(buf_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        if (qr_canvas_buffer_ == nullptr) {
            ESP_LOGE(TAG, "Failed to allocate QR canvas buffer");
            return;
        }
        
        qr_canvas_ = lv_canvas_create(screen);
        lv_canvas_set_buffer(qr_canvas_, qr_canvas_buffer_, canvas_w, canvas_h, LV_COLOR_FORMAT_I1);
        lv_obj_center(qr_canvas_);
        ESP_LOGI(TAG, "QR canvas created: %dx%d, buffer size: %d", canvas_w, canvas_h, buf_size);
    }

    lv_obj_move_foreground(qr_canvas_);
    // Fill white background
    lv_canvas_fill_bg(qr_canvas_, lv_color_white(), LV_OPA_COVER);
    
    // Initialize layer for drawing
    lv_layer_t layer;
    lv_canvas_init_layer(qr_canvas_, &layer);
    
    // Setup rect descriptor for QR code modules
    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_color = lv_color_black();
    rect_dsc.bg_opa = LV_OPA_COVER;
    
    // Calculate QR code position (centered with space for text at bottom)
    int text_height = text != nullptr ? 10 : 0;
    int qr_display_size = qr_size * pixel_size;
    int qr_pos_x = (width_ - qr_display_size) / 2;
    int qr_pos_y = (height_ - text_height - qr_display_size) / 2;
    
    // Draw QR code modules
    for (int y = 0; y < qr_size; y++) {
        for (int x = 0; x < qr_size; x++) {
            if (esp_qrcode_get_module(qrcode, x, y)) {
                lv_area_t coords_rect;
                coords_rect.x1 = x * pixel_size + qr_pos_x;
                coords_rect.y1 = y * pixel_size + qr_pos_y;
                coords_rect.x2 = (x + 1) * pixel_size - 1 + qr_pos_x;
                coords_rect.y2 = (y + 1) * pixel_size - 1 + qr_pos_y;
                
                lv_draw_rect(&layer, &rect_dsc, &coords_rect);
            }
        }
    }

    // Draw text at bottom if provided
    if (text != nullptr || !ip_address_.empty()) {
        lv_draw_label_dsc_t label_dsc;
        lv_draw_label_dsc_init(&label_dsc);
        label_dsc.color = lv_color_black();
        label_dsc.text = text != nullptr ? text : ip_address_.c_str();
        
        int text_pos_y = qr_pos_y + qr_display_size + 2;
        lv_area_t coords_text = {0, text_pos_y, width_ - 1, height_ - 1};
        lv_draw_label(&layer, &label_dsc, &coords_text);
        ESP_LOGI(TAG, "QR text drawn: %s", label_dsc.text);
    }
    
    // Finish layer
    lv_canvas_finish_layer(qr_canvas_, &layer);
    ESP_LOGI(TAG, "QR code drawn on OLED canvas");
    qr_code_displayed_ = true;
}

void OledDisplay::ClearQRCode() {
    if (!qr_code_displayed_) {
        return;
    }

    qr_code_displayed_ = false;
    DisplayLockGuard lock(this);
    
    if (qr_canvas_ != nullptr) {
        ESP_LOGI(TAG, "Clearing QR code from OLED canvas");
        lv_obj_del(qr_canvas_);
        qr_canvas_ = nullptr;
    }

    if (qr_canvas_buffer_ != nullptr) {
        heap_caps_free(qr_canvas_buffer_);
        qr_canvas_buffer_ = nullptr;
        ESP_LOGI(TAG, "QR canvas buffer freed");
    }
}

bool OledDisplay::QRCodeIsSupported() {
    return false;
}

void OledDisplay::SetIpAddress(const std::string& ip_address) {
    ip_address_ = ip_address;
    ESP_LOGI(TAG, "IP address set to: %s", ip_address_.c_str());
}