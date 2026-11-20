#ifndef DEFERRED_REPLY_GATE_H_
#define DEFERRED_REPLY_GATE_H_

#include <atomic>
#include <cstdint>

// One completion wins, including cancellation; old voice sessions never receive
// a reply belonging to a previous session. Direct transports own their sender.
class DeferredReplyGate {
public:
    DeferredReplyGate(uint32_t epoch, bool session_bound) : epoch_(epoch), bound_(session_bound) {}
    bool Claim() { return !completed_.exchange(true); }
    bool CanReply(uint32_t current_epoch) const { return !bound_ || epoch_ == current_epoch; }

private:
    const uint32_t epoch_;
    const bool bound_;
    std::atomic<bool> completed_{false};
};

#endif
