#ifndef SWIPE_DETECTOR_H
#define SWIPE_DETECTOR_H

#include <cstdint>

enum class SwipeDirection {
    kNone,
    kLeft,
    kRight,
    kUp,
    kDown
};

/**
 * Detects horizontal swipe gestures from touch input.
 * Used to switch between character faces.
 */
class SwipeDetector {
public:
    SwipeDetector();

    /**
     * Call when touch starts.
     * @param x Starting X coordinate
     * @param y Starting Y coordinate
     */
    void OnTouchStart(int x, int y);

    /**
     * Call when touch ends.
     * @param x Ending X coordinate
     * @param y Ending Y coordinate
     * @return Detected swipe direction (kNone if no swipe)
     */
    SwipeDirection OnTouchEnd(int x, int y);

    /**
     * Reset the detector state.
     */
    void Reset();

    /**
     * Check if a touch is currently active.
     */
    bool IsTouchActive() const { return touch_active_; }

    // Configuration
    void SetMinSwipeDistance(int distance) { min_swipe_distance_ = distance; }
    void SetMaxSwipeDuration(int duration_ms) { max_swipe_duration_ms_ = duration_ms; }

private:
    bool touch_active_ = false;
    int start_x_ = 0;
    int start_y_ = 0;
    int64_t start_time_ms_ = 0;

    // Thresholds
    int min_swipe_distance_ = 50;      // Minimum pixels to count as swipe
    int max_swipe_duration_ms_ = 500;  // Maximum time for a swipe gesture
};

#endif // SWIPE_DETECTOR_H
