#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <limits>

// The caller authenticates packets and serializes access. Times are monotonic microseconds.
class UdpProbe {
public:
    static constexpr int64_t kIntervalUs = 15000000;
    enum class Action { None, Send, Failed };
    explicit UdpProbe(const uint8_t* ssrc) {
        packet_[0] = 0x02;
        packet_[3] = 16;
        memcpy(packet_.data() + 4, ssrc, 4);
    }

    Action Poll(int64_t now) {
        if (failed_) {
            return Action::None;
        }
        if (!pending_) {
            if (now < next_round_) {
                return Action::None;
            }
            if (sequence_ == std::numeric_limits<uint32_t>::max()) {
                failed_ = true;
                return Action::Failed;
            }
            ++sequence_;
            for (int i = 0; i < 4; ++i) {
                packet_[12 + i] = static_cast<uint8_t>(sequence_ >> (24 - 8 * i));
            }
            pending_ = true;
            started_ = now;
            attempt_ = 1;
            return Action::Send;
        }
        if (now - started_ >= 5000000) {
            failed_ = true;
            return Action::Failed;
        }
        const int64_t retry_at = attempt_ == 1 ? 1000000 : 3000000;
        if (attempt_ < 3 && now - started_ >= retry_at) {
            ++attempt_;
            return Action::Send;
        }
        return Action::None;
    }

    bool Matches(const uint8_t* header, int64_t now) const {
        return pending_ && !failed_ && now - started_ < 5000000 && header[0] == 0x03 &&
               memcmp(header + 1, packet_.data() + 1, 15) == 0;
    }

    void Complete(int64_t now, uint32_t jitter_us) {
        pending_ = false;
        next_round_ = now + kIntervalUs + jitter_us;
    }

    bool Failed() const { return failed_; }
    int64_t ElapsedUs(int64_t now) const { return now - started_; }
    unsigned Attempts() const { return attempt_; }
    int64_t NextActionAt() const {
        if (!pending_) {
            return next_round_;
        }
        return started_ + (attempt_ == 1 ? 1000000 : attempt_ == 2 ? 3000000 : 5000000);
    }
    std::array<uint8_t, 32>& Packet() { return packet_; }

private:
    std::array<uint8_t, 32> packet_{};
    uint32_t sequence_ = 0;
    unsigned attempt_ = 0;
    bool pending_ = false;
    bool failed_ = false;
    int64_t started_ = 0;
    int64_t next_round_ = 0;
};
