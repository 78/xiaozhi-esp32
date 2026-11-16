#pragma once

#include <cstdint>
#include <string>

// Caller serializes access. A cancelled reply stays invalid even after its
// generation finishes, so late audio/done events cannot revive playback.
class RealtimeResponseTracker {
public:
    uint32_t Start(const std::string& id) {
        id_ = id;
        active_ = true;
        return ++generation_;
    }
    void Invalidate() {
        id_.clear();
        active_ = false;
        ++generation_;
    }
    bool Accepts(const char* id) const { return active_ && id && !id_.empty() && id_ == id; }
    bool Finish(const char* id) {
        if (!Accepts(id) || !active_) {
            return false;
        }
        active_ = false;
        return true;
    }
    bool active() const { return active_; }
    uint32_t generation() const { return generation_; }

private:
    std::string id_;
    uint32_t generation_ = 0;
    bool active_ = false;
};
