#include "lcd_touch.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <cmath>

static const char* TAG = "LcdTouch";

#define TOUCH_POLLING_DELAY_MS 10

// ============================================================================
// LcdTouch Base Class Implementation
// ============================================================================

LcdTouch::LcdTouch(esp_lcd_touch_handle_t touch_handle, esp_lcd_panel_io_handle_t panel_io,
                   uint16_t width, uint16_t height, bool swap_xy, bool mirror_x, bool mirror_y, TouchInterruptCallback callback)
    : touch_handle_(touch_handle)
    , panel_io_(panel_io)
    , swap_xy_(swap_xy)
    , mirror_x_(mirror_x)
    , mirror_y_(mirror_y)
    , width_(width)
    , height_(height)
    , interrupt_callback_(callback)
{
    ESP_LOGI(TAG, "LcdTouch initialized: %dx%d, swap_xy=%d, mirror_x=%d, mirror_y=%d",
             width_, height_, swap_xy_, mirror_x_, mirror_y_);

#ifdef LVGL_PORT_TOUCH_DRIVER_CALLBACK
    const lvgl_port_touch_cfg_t touch_cfg = {
      .disp = lv_display_get_default(),
      .handle = touch_handle_,
    };
    lvgl_port_add_touch(&touch_cfg);
#else
    ESP_LOGI(TAG, "Adding custom touch driver to LVGL...");
    touch_indev_ = lv_indev_create();
    lv_indev_set_type(touch_indev_, LV_INDEV_TYPE_POINTER);
    lv_indev_set_driver_data(touch_indev_, touch_handle_);
    lv_indev_set_user_data(touch_indev_, this);
#if (0)
    lv_indev_set_read_cb(touch_indev_, [](lv_indev_t *drv, lv_indev_data_t *data) {
        LcdTouch* instance = (LcdTouch*)lv_indev_get_user_data(drv);
        instance->touch_driver_read(drv, data);
    });
#else
    xTaskCreatePinnedToCore(touch_event_task, "touch_task", 4 * 1024, this, 5, NULL, 0);
#endif
#endif
}

LcdTouch::~LcdTouch() {
    if (touch_handle_) {
        ESP_LOGI(TAG, "LcdTouch destroyed");
    }
}

void LcdTouch::touch_event_task(void* arg)
{
    LcdTouch *touch = static_cast<LcdTouch*>(arg);
    if (touch == nullptr) {
        ESP_LOGE(TAG, "Invalid touchpad pointer in touch_event_task");
        vTaskDelete(NULL);
        return;
    }

    lv_indev_data_t data;
    vTaskDelay(pdMS_TO_TICKS(100)); // Initial delay
    while (true) {
        touch->touch_driver_read(touch->touch_indev_, &data);
    }
}

void LcdTouch::touch_driver_read(lv_indev_t *drv, lv_indev_data_t *data) {
    constexpr uint8_t TOUCH_MAX_POINT = 1;
    esp_lcd_touch_point_data_t point_data[TOUCH_MAX_POINT];
    uint8_t touch_cnt = 0;

    if (interrupt_callback_) {
        if (!interrupt_callback_()) {
            data->state = LV_INDEV_STATE_RELEASED;
            data->continue_reading = true;
            return;
        }
    } else {
        // No interrupt callback, proceed with polling
        vTaskDelay(pdMS_TO_TICKS(TOUCH_POLLING_DELAY_MS));
    }

    esp_lcd_touch_handle_t touch_ctx = (esp_lcd_touch_handle_t)lv_indev_get_driver_data(drv);
    esp_err_t err = esp_lcd_touch_read_data(touch_ctx);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read touch data: %s", esp_err_to_name(err));
        return;
    }

    err = esp_lcd_touch_get_data(touch_ctx, point_data, &touch_cnt, TOUCH_MAX_POINT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get touch data: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGD(TAG, "Touch points detected: %d, at (%d, %d)", touch_cnt, point_data[0].x, point_data[0].y);

    if (err == ESP_OK && touch_cnt > 0 && touch_cnt <= TOUCH_MAX_POINT) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = point_data[0].x;
        data->point.y = point_data[0].y;
        int64_t current_time = esp_timer_get_time();
        if (current_time - touch_end_time_ > TOUCH_SWIPE_RELEASE_TIMEOUT) {
            release_timeout_us_ = gesture_config_.release_timeout_us;
            was_touching_ = false;
            ESP_LOGI(TAG, "Touch stable after release timeout");
        }
        touch_end_time_ = current_time;
        
        int16_t current_x = static_cast<int16_t>(point_data[0].x);
        int16_t current_y = static_cast<int16_t>(point_data[0].y);
        
        // Handle touch press (start of touch)
        if (!was_touching_) {
            ESP_LOGI(TAG, "Touch press detected at (%d, %d)", current_x, current_y);
            HandleTouchPress(current_x, current_y);
        }
        
        // Update current position while touching
        touch_current_x_ = current_x;
        touch_current_y_ = current_y;
        
        // Detect swipe while touching (only if not already swiping, same as custom_touch_read_cb)
        if (was_touching_ && !is_swiping_) {
            TouchGesture gesture = DetectSwipeGesture();
            if (gesture != TOUCH_GESTURE_NONE) {
                gesture_detected_ = true;
                release_timeout_us_ = TOUCH_SWIPE_RELEASE_TIMEOUT; // Extend release timeout to avoid immediate release handling
                HandleTouchRefresh(current_x, current_y);
                if (gesture_callback_) {
                    gesture_callback_(gesture, touch_current_x_, touch_current_y_);
                }
            }
        }
        
        // Detect long press
        if (!long_press_detected_ && !gesture_detected_) {
            int64_t touch_duration = esp_timer_get_time() - touch_start_time_;
            if (touch_duration > gesture_config_.long_press_time_us) {
                long_press_detected_ = true;
                ESP_LOGI(TAG, "🖐️ Long press detected at (%d, %d) after %lld us", touch_current_x_, touch_current_y_, touch_duration);
                if (gesture_callback_) {
                    gesture_callback_(TOUCH_GESTURE_LONG_PRESS, touch_current_x_, touch_current_y_);
                }
            }
        }
        
        was_touching_ = true;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
        // Handle touch release (end of touch)
        if (was_touching_) { // Debounce release
            int64_t current_time = esp_timer_get_time();
            int32_t time_since_release = current_time - touch_end_time_;
            ESP_LOGI(TAG, "Time since last touch point: %d us, release timeout: %d us", (int)time_since_release, (int)release_timeout_us_);
            if (time_since_release > release_timeout_us_) {
                if (!gesture_detected_ && !long_press_detected_) {
                    ESP_LOGI(TAG, "Touch release confirmed after %d us", (int)time_since_release);
                    HandleTouchRelease();
                }
                was_touching_ = false;
                release_timeout_us_ = gesture_config_.release_timeout_us; // Reset to default
            }
        }
    }

    data->continue_reading = true;
}

void LcdTouch::HandleTouchPress(int16_t x, int16_t y) {
    touch_start_x_ = x;
    touch_start_y_ = y;
    touch_current_x_ = x;
    touch_current_y_ = y;
    touch_start_time_ = esp_timer_get_time();
    gesture_detected_ = false;
    long_press_detected_ = false;
    is_swiping_ = false;  // Reset swipe flag
    
    ESP_LOGD(TAG, "Touch press at (%d, %d) at time %lld", x, y, touch_start_time_);
}

void LcdTouch::HandleTouchRefresh(int16_t x, int16_t y) {
    touch_start_x_ = x;
    touch_start_y_ = y;
    touch_current_x_ = x;
    touch_current_y_ = y;
    is_swiping_ = false;  // Reset swipe flag
    touch_start_time_ = esp_timer_get_time();
}

void LcdTouch::HandleTouchRelease() {
    int64_t touch_duration = esp_timer_get_time() - touch_start_time_;
    
    ESP_LOGD(TAG, "Touch release after %lld us, is_swiping_=%d", touch_duration, is_swiping_);
    
    // Only trigger tap if no swipe was detected (same logic as custom_touch_read_cb)
    if (!is_swiping_ && !gesture_detected_) {
        int64_t current_time = esp_timer_get_time();
        int64_t time_since_last_tap = current_time - last_tap_time_;
        
        // Check for double tap
        if (time_since_last_tap < gesture_config_.double_tap_window_us && time_since_last_tap > gesture_config_.tap_timeout_us && last_tap_time_ > 0) {
            // Double tap detected
            ESP_LOGI(TAG, "👆👆 Double tap at (%d, %d)", touch_start_x_, touch_start_y_);
            if (gesture_callback_) {
                gesture_callback_(TOUCH_GESTURE_DOUBLE_TAP, touch_start_x_, touch_start_y_);
            }
            last_tap_time_ = 0; // Reset to prevent triple tap
        } else {
            // Single tap detected
            ESP_LOGI(TAG, "👆 Single tap at (%d, %d)", touch_start_x_, touch_start_y_);
            if (gesture_callback_) {
                gesture_callback_(TOUCH_GESTURE_TAP, touch_start_x_, touch_start_y_);
            }
            last_tap_time_ = current_time;
        }
    } else if (is_swiping_) {
        ESP_LOGD(TAG, "Swipe completed, no tap action");
    }
    
    // Reset swipe flag for next touch
    is_swiping_ = false;
}

TouchGesture LcdTouch::DetectSwipeGesture() {
    int16_t dx = touch_current_x_ - touch_start_x_;
    int16_t dy = touch_current_y_ - touch_start_y_;
    int64_t touch_duration = esp_timer_get_time() - touch_start_time_;
    
    // Check if swipe timeout has been exceeded
    if (touch_duration > gesture_config_.swipe_timeout_us) {
        return TOUCH_GESTURE_NONE;
    }
    
    // Detect horizontal swipe
    if (std::abs(dx) > gesture_config_.swipe_threshold && abs(dx) > abs(dy) * gesture_config_.ratio_xy) {
        is_swiping_ = true;  // Mark as swiping to prevent tap
        if (dx > 0) {
            ESP_LOGI(TAG, "👉 Swipe RIGHT detected (dx=%d)", dx);
            return TOUCH_GESTURE_SWIPE_RIGHT;
        } else {
            ESP_LOGI(TAG, "👈 Swipe LEFT detected (dx=%d)", dx);
            return TOUCH_GESTURE_SWIPE_LEFT;
        }
    }
    
    // Detect vertical swipe
    if (std::abs(dy) > gesture_config_.swipe_threshold && abs(dy) > abs(dx) * gesture_config_.ratio_xy) {
        is_swiping_ = true;  // Mark as swiping to prevent tap
        if (dy > 0) {
            ESP_LOGI(TAG, "👇 Swipe DOWN detected (dy=%d)", dy);
            return TOUCH_GESTURE_SWIPE_DOWN;
        } else {
            ESP_LOGI(TAG, "👆 Swipe UP detected (dy=%d)", dy);
            return TOUCH_GESTURE_SWIPE_UP;
        }
    }
    
    return TOUCH_GESTURE_NONE;
}

void LcdTouch::SetGestureCallback(TouchEventCallback callback) {
    gesture_callback_ = callback;
}

void LcdTouch::SetInterruptCallback(TouchInterruptCallback callback) {
    interrupt_callback_ = callback;
}

void LcdTouch::SetRatioXY(float ratio) {
    gesture_config_.ratio_xy = ratio;
    ESP_LOGI(TAG, "Ratio XY set to %.2f", ratio);
}

void LcdTouch::SetSwipeThreshold(int16_t threshold) {
    gesture_config_.swipe_threshold = threshold;
    ESP_LOGI(TAG, "Swipe threshold set to %d pixels", threshold);
}

void LcdTouch::SetSwipeTimeout(int32_t timeout_us) {
    gesture_config_.swipe_timeout_us = timeout_us;
    ESP_LOGI(TAG, "Swipe timeout set to %lld us", (long long)timeout_us);
}

void LcdTouch::SetTapTimeout(int32_t timeout_us) {
    gesture_config_.tap_timeout_us = timeout_us;
    ESP_LOGI(TAG, "Tap timeout set to %lld us", (long long)timeout_us);
}

void LcdTouch::SetDoubleTapWindow(int32_t window_us) {
    gesture_config_.double_tap_window_us = window_us;
    ESP_LOGI(TAG, "Double tap window set to %lld us", (long long)window_us);
}

void LcdTouch::SetLongPressTime(int32_t time_us) {
    gesture_config_.long_press_time_us = time_us;
    ESP_LOGI(TAG, "Long press time set to %lld us", (long long)time_us);
}

void LcdTouch::SetReleaseTimeout(int32_t time_us) {
    gesture_config_.release_timeout_us = time_us;
    release_timeout_us_ = time_us;
    ESP_LOGI(TAG, "Release timeout set to %lld us", (long long)time_us);
}

esp_lcd_touch_handle_t LcdTouch::GetTouchHandle() const {
    return touch_handle_;
}

// ============================================================================
// I2cLcdTouch Implementation
// ============================================================================

I2cLcdTouch::I2cLcdTouch(esp_lcd_touch_handle_t touch_handle, esp_lcd_panel_io_handle_t panel_io,
                         uint16_t width, uint16_t height, bool swap_xy, bool mirror_x, bool mirror_y, TouchInterruptCallback callback)
    : LcdTouch(touch_handle, panel_io, width, height, swap_xy, mirror_x, mirror_y, callback)
{
}

I2cLcdTouch::~I2cLcdTouch() {
    ESP_LOGI(TAG, "I2cLcdTouch destroyed");
}

// ============================================================================
// SpiLcdTouch Implementation
// ============================================================================

SpiLcdTouch::SpiLcdTouch(esp_lcd_touch_handle_t touch_handle, esp_lcd_panel_io_handle_t panel_io,
                         uint16_t width, uint16_t height, bool swap_xy, bool mirror_x, bool mirror_y, TouchInterruptCallback callback)
    : LcdTouch(touch_handle, panel_io, width, height, swap_xy, mirror_x, mirror_y, callback)
{
}

SpiLcdTouch::~SpiLcdTouch() {
    ESP_LOGI(TAG, "SpiLcdTouch destroyed");
}
