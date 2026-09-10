#pragma once

#include <cstdint>

// TODO: Replace this compatibility shim with the Zectrix sleep manager once
// its implementation is contributed. Keeping the API calls in place preserves
// the display activity/busy semantics intended by the board implementation.
enum class SleepBusySrc {
    Display,
};

inline void sm_kick(uint32_t duration_ms, const char* reason) {
    (void)duration_ms;
    (void)reason;
}

inline void sm_set_busy(SleepBusySrc source, bool busy) {
    (void)source;
    (void)busy;
}
