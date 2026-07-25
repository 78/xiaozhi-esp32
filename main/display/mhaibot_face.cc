#include "mhaibot_face.h"

#include <esp_log.h>
#include <esp_random.h>

#define TAG "MhaiBotFace"

MhaiBotFace::MhaiBotFace(lv_obj_t* parent, lv_color_t eye_color)
    : MhaiBotFace(parent, eye_color, Config()) {}

MhaiBotFace::MhaiBotFace(lv_obj_t* parent, lv_color_t eye_color, const Config& config)
    : config_(config) {
    if (parent == nullptr) {
        ESP_LOGE(TAG, "parent is null, face not created");
        return;
    }
    CreateEyes(parent, eye_color);
    if (root_ != nullptr) {
        StartBlinkTimer();
        Hide();
    }
}

MhaiBotFace::~MhaiBotFace() {
    destroying_ = true;
    StopBlinkTimer();
    if (root_ != nullptr && lv_obj_is_valid(root_)) {
        // Detach delete callback before deleting so it does not race teardown.
        lv_obj_remove_event_cb(root_, OnRootDeleted);
        lv_obj_del(root_);
    }
    root_ = nullptr;
    left_eye_ = nullptr;
    right_eye_ = nullptr;
}

void MhaiBotFace::CreateEyes(lv_obj_t* parent, lv_color_t eye_color) {
    const int total_width = config_.eye_width * 2 + config_.eye_gap;

    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, total_width, config_.eye_height);
    lv_obj_set_style_bg_opa(root_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(root_, LV_ALIGN_CENTER, 0, config_.vertical_offset);
    lv_obj_add_event_cb(root_, OnRootDeleted, LV_EVENT_DELETE, this);

    auto make_eye = [this, eye_color](int x_offset) -> lv_obj_t* {
        lv_obj_t* eye = lv_obj_create(root_);
        lv_obj_set_size(eye, config_.eye_width, config_.eye_height);
        lv_obj_set_style_radius(eye, config_.eye_radius, 0);
        lv_obj_set_style_bg_color(eye, eye_color, 0);
        lv_obj_set_style_bg_opa(eye, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(eye, 0, 0);
        lv_obj_set_style_pad_all(eye, 0, 0);
        lv_obj_clear_flag(eye, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(eye, LV_ALIGN_LEFT_MID, x_offset, 0);
        return eye;
    };

    left_eye_ = make_eye(0);
    right_eye_ = make_eye(config_.eye_width + config_.eye_gap);
}

void MhaiBotFace::Show() {
    if (root_ == nullptr || !lv_obj_is_valid(root_)) {
        return;
    }
    // Always reopen eyes so a blink interrupted by Hide() does not leave the
    // face stuck in the Closed phase when it returns.
    OpenEyes();
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_HIDDEN);
    visible_ = true;
    if (blink_timer_ != nullptr) {
        const uint32_t open_ms = RandomIntervalMs(config_.blink_min_ms, config_.blink_max_ms);
        lv_timer_set_period(blink_timer_, open_ms);
        lv_timer_reset(blink_timer_);
        lv_timer_resume(blink_timer_);
    }
}

void MhaiBotFace::Hide() {
    if (root_ != nullptr && lv_obj_is_valid(root_)) {
        lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
    }
    visible_ = false;
    if (blink_timer_ != nullptr) {
        lv_timer_pause(blink_timer_);
    }
}

void MhaiBotFace::SetEmotion(Emotion emotion) {
    // V1.1 keeps the face visually neutral for every mapped emotion; this
    // just records intent so a future version can add per-emotion visuals
    // without touching call sites.
    emotion_ = emotion;
}

void MhaiBotFace::SetColor(lv_color_t eye_color) {
    if (left_eye_ != nullptr && lv_obj_is_valid(left_eye_)) {
        lv_obj_set_style_bg_color(left_eye_, eye_color, 0);
    }
    if (right_eye_ != nullptr && lv_obj_is_valid(right_eye_)) {
        lv_obj_set_style_bg_color(right_eye_, eye_color, 0);
    }
}

void MhaiBotFace::StartBlinkTimer() {
    if (blink_timer_ != nullptr || root_ == nullptr) {
        return;
    }
    const uint32_t first_delay = RandomIntervalMs(config_.blink_min_ms, config_.blink_max_ms);
    blink_timer_ = lv_timer_create(BlinkTimerCb, first_delay, this);
    if (blink_timer_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create blink timer");
        return;
    }
    // Start paused until Show(); Hide() was already called from the ctor.
    lv_timer_pause(blink_timer_);
}

void MhaiBotFace::StopBlinkTimer() {
    if (blink_timer_ == nullptr) {
        return;
    }
    lv_timer_delete(blink_timer_);
    blink_timer_ = nullptr;
}

void MhaiBotFace::OpenEyes() {
    if (left_eye_ == nullptr || right_eye_ == nullptr || !lv_obj_is_valid(left_eye_) ||
        !lv_obj_is_valid(right_eye_)) {
        return;
    }
    lv_obj_set_height(left_eye_, config_.eye_height);
    lv_obj_set_height(right_eye_, config_.eye_height);
    lv_obj_align(left_eye_, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_align(right_eye_, LV_ALIGN_LEFT_MID, config_.eye_width + config_.eye_gap, 0);
    phase_ = BlinkPhase::Open;
}

void MhaiBotFace::CloseEyes() {
    if (left_eye_ == nullptr || right_eye_ == nullptr || !lv_obj_is_valid(left_eye_) ||
        !lv_obj_is_valid(right_eye_)) {
        return;
    }
    lv_obj_set_height(left_eye_, config_.closed_height);
    lv_obj_set_height(right_eye_, config_.closed_height);
    lv_obj_align(left_eye_, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_align(right_eye_, LV_ALIGN_LEFT_MID, config_.eye_width + config_.eye_gap, 0);
    phase_ = BlinkPhase::Closed;
}

uint32_t MhaiBotFace::RandomIntervalMs(uint32_t min_ms, uint32_t max_ms) const {
    if (max_ms <= min_ms) {
        return min_ms;
    }
    const uint32_t span = max_ms - min_ms + 1;
    return min_ms + (esp_random() % span);
}

void MhaiBotFace::BlinkTimerCb(lv_timer_t* timer) {
    auto* self = static_cast<MhaiBotFace*>(lv_timer_get_user_data(timer));
    if (self == nullptr || self->destroying_ || !self->visible_) {
        return;
    }
    if (self->root_ == nullptr || !lv_obj_is_valid(self->root_) || self->left_eye_ == nullptr ||
        self->right_eye_ == nullptr || !lv_obj_is_valid(self->left_eye_) ||
        !lv_obj_is_valid(self->right_eye_)) {
        return;
    }

    if (self->phase_ == BlinkPhase::Open) {
        self->CloseEyes();
        const uint32_t close_ms = self->RandomIntervalMs(self->config_.blink_close_min_ms,
                                                         self->config_.blink_close_max_ms);
        lv_timer_set_period(timer, close_ms);
        lv_timer_reset(timer);
    } else {
        self->OpenEyes();
        const uint32_t open_ms =
            self->RandomIntervalMs(self->config_.blink_min_ms, self->config_.blink_max_ms);
        lv_timer_set_period(timer, open_ms);
        lv_timer_reset(timer);
    }
}

void MhaiBotFace::OnRootDeleted(lv_event_t* event) {
    auto* self = static_cast<MhaiBotFace*>(lv_event_get_user_data(event));
    if (self == nullptr || self->destroying_) {
        return;
    }
    // LVGL is deleting the root (or an ancestor). Stop the timer and null
    // pointers only — do not delete root_ again (would recurse).
    self->StopBlinkTimer();
    self->root_ = nullptr;
    self->left_eye_ = nullptr;
    self->right_eye_ = nullptr;
    self->visible_ = false;
}
