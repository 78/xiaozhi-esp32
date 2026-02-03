#include "swipe_detector.h"
#include <esp_timer.h>
#include <cstdlib>  // for abs()

SwipeDetector::SwipeDetector() {
}

void SwipeDetector::OnTouchStart(int x, int y) {
    touch_active_ = true;
    start_x_ = x;
    start_y_ = y;
    start_time_ms_ = esp_timer_get_time() / 1000;  // Convert microseconds to milliseconds
}

SwipeDirection SwipeDetector::OnTouchEnd(int x, int y) {
    if (!touch_active_) {
        return SwipeDirection::kNone;
    }

    touch_active_ = false;

    int64_t end_time_ms = esp_timer_get_time() / 1000;
    int64_t duration_ms = end_time_ms - start_time_ms_;

    // Check if gesture was too slow
    if (duration_ms > max_swipe_duration_ms_) {
        return SwipeDirection::kNone;
    }

    int dx = x - start_x_;
    int dy = y - start_y_;
    int abs_dx = abs(dx);
    int abs_dy = abs(dy);

    // Check minimum distance
    if (abs_dx < min_swipe_distance_ && abs_dy < min_swipe_distance_) {
        return SwipeDirection::kNone;
    }

    // Determine primary direction (horizontal vs vertical)
    if (abs_dx > abs_dy) {
        // Horizontal swipe
        if (dx > 0) {
            return SwipeDirection::kRight;
        } else {
            return SwipeDirection::kLeft;
        }
    } else {
        // Vertical swipe
        if (dy > 0) {
            return SwipeDirection::kDown;
        } else {
            return SwipeDirection::kUp;
        }
    }
}

void SwipeDetector::Reset() {
    touch_active_ = false;
    start_x_ = 0;
    start_y_ = 0;
    start_time_ms_ = 0;
}
