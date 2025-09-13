#include "lvgl_gif.h"
#include <esp_log.h>
#include <cstring>

#define TAG "LvglGif"

LvglGif::LvglGif(const lv_img_dsc_t* img_dsc)
    : gif_(nullptr), timer_(nullptr), last_call_(0), playing_(false), loaded_(false) {
    if (!img_dsc || !img_dsc->data) {
        ESP_LOGE(TAG, "Invalid image descriptor");
        return;
    }

    gif_ = gd_open_gif_data(img_dsc->data);
    if (!gif_) {
        ESP_LOGE(TAG, "Failed to open GIF from image descriptor");
        return;
    }

    // Setup LVGL image descriptor
    memset(&img_dsc_, 0, sizeof(img_dsc_));
    img_dsc_.header.magic = LV_IMAGE_HEADER_MAGIC;
    img_dsc_.header.flags = LV_IMAGE_FLAGS_MODIFIABLE;
    img_dsc_.header.cf = LV_COLOR_FORMAT_ARGB8888;
    img_dsc_.header.w = gif_->width;
    img_dsc_.header.h = gif_->height;
    img_dsc_.header.stride = gif_->width * 4;
    img_dsc_.data = gif_->canvas;
    img_dsc_.data_size = gif_->width * gif_->height * 4;

    // Render first frame
    if (gif_->canvas) {
        gd_render_frame(gif_, gif_->canvas);
    }

    loaded_ = true;
    ESP_LOGD(TAG, "GIF loaded from image descriptor: %dx%d", gif_->width, gif_->height);
}

// Destructor
LvglGif::~LvglGif() {
    Cleanup();
}

// LvglImage interface implementation
const lv_img_dsc_t* LvglGif::image_dsc() const {
    if (!loaded_) {
        return nullptr;
    }
    return &img_dsc_;
}

// Animation control methods
void LvglGif::Start() {
    if (!loaded_ || !gif_) {
        ESP_LOGW(TAG, "GIF not loaded, cannot start");
        return;
    }

    if (!timer_) {
        timer_ = lv_timer_create([](lv_timer_t* timer) {
            LvglGif* gif_obj = static_cast<LvglGif*>(lv_timer_get_user_data(timer));
            gif_obj->NextFrame();
        }, 10, this);
    }

    if (timer_) {
        playing_ = true;
        last_call_ = lv_tick_get();
        lv_timer_resume(timer_);
        lv_timer_reset(timer_);
        
        // Render first frame
        NextFrame();
        
        ESP_LOGD(TAG, "GIF animation started");
    }
}

void LvglGif::Pause() {
    if (timer_) {
        playing_ = false;
        lv_timer_pause(timer_);
        ESP_LOGD(TAG, "GIF animation paused");
    }
}

void LvglGif::Resume() {
    if (!loaded_ || !gif_) {
        ESP_LOGW(TAG, "GIF not loaded, cannot resume");
        return;
    }

    if (timer_) {
        playing_ = true;
        lv_timer_resume(timer_);
        ESP_LOGD(TAG, "GIF animation resumed");
    }
}

void LvglGif::Stop() {
    if (timer_) {
        playing_ = false;
        lv_timer_pause(timer_);
    }

    if (gif_) {
        gd_rewind(gif_);
        NextFrame();
        ESP_LOGD(TAG, "GIF animation stopped and rewound");
    }
}

bool LvglGif::IsPlaying() const {
    return playing_;
}

bool LvglGif::IsLoaded() const {
    return loaded_;
}

int32_t LvglGif::GetLoopCount() const {
    if (!loaded_ || !gif_) {
        return -1;
    }
    return gif_->loop_count;
}

void LvglGif::SetLoopCount(int32_t count) {
    if (!loaded_ || !gif_) {
        ESP_LOGW(TAG, "GIF not loaded, cannot set loop count");
        return;
    }
    gif_->loop_count = count;
}

uint16_t LvglGif::width() const {
    if (!loaded_ || !gif_) {
        return 0;
    }
    return gif_->width;
}

uint16_t LvglGif::height() const {
    if (!loaded_ || !gif_) {
        return 0;
    }
    return gif_->height;
}

void LvglGif::SetFrameCallback(std::function<void()> callback) {
    frame_callback_ = callback;
}

void LvglGif::NextFrame() {
    if (!loaded_ || !gif_ || !playing_) {
        return;
    }

    // Check if enough time has passed for the next frame
    uint32_t elapsed = lv_tick_elaps(last_call_);
    if (elapsed < gif_->gce.delay * 10) {
        return;
    }

    last_call_ = lv_tick_get();

    // Get next frame
    int has_next = gd_get_frame(gif_);
    if (has_next == 0) {
        // Animation finished, pause timer
        playing_ = false;
        if (timer_) {
            lv_timer_pause(timer_);
        }
        ESP_LOGD(TAG, "GIF animation completed");
    }

    // Render current frame
    if (gif_->canvas) {
        gd_render_frame(gif_, gif_->canvas);
        
        // Call frame callback if set
        if (frame_callback_) {
            frame_callback_();
        }
    }
}

void LvglGif::Cleanup() {
    // Stop and delete timer
    if (timer_) {
        lv_timer_delete(timer_);
        timer_ = nullptr;
    }

    // Close GIF decoder
    if (gif_) {
        gd_close_gif(gif_);
        gif_ = nullptr;
    }

    playing_ = false;
    loaded_ = false;
    
    // Clear image descriptor
    memset(&img_dsc_, 0, sizeof(img_dsc_));
}
