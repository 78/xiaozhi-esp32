#ifndef PETOI_ROBOT_PLAN_H_
#define PETOI_ROBOT_PLAN_H_

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <string>

#include "robot_frame.h"

namespace petoi {
constexpr size_t kMaxActions = 4;
struct RobotPlan {
    std::array<RobotFrame, kMaxActions> frames{};
    size_t count = 0;
};

inline bool BuildRobotPlan(const std::string& text, RobotPlan& plan) {
    plan.count = 0;
    if (text.empty() || text.size() > kMaxCommandLength)
        return false;
    RobotPlan candidate;
    size_t start = 0;
    while (true) {
        const size_t end = text.find(',', start);
        auto command = text.substr(start, end == std::string::npos ? end : end - start);
        const auto first = command.find_first_not_of(' ');
        const auto last = command.find_last_not_of(' ');
        if (first == std::string::npos || candidate.count == kMaxActions)
            return false;
        command = command.substr(first, last - first + 1);
        if (!BuildRobotFrame(command, candidate.frames[candidate.count]))
            return false;
        ++candidate.count;
        if (end == std::string::npos)
            break;
        start = end + 1;
    }
    plan = candidate;  // No partial sequence becomes visible on validation failure.
    return true;
}

inline std::string PlanText(const RobotPlan& plan) {
    std::string text;
    for (size_t i = 0; i < plan.count; ++i) {
        if (i)
            text += ',';
        text.append(plan.frames[i].data.data() + 3, plan.frames[i].length - 3);
    }
    return text;
}

inline bool SameAction(const RobotFrame& a, const RobotFrame& b) {
    return a.length == b.length &&
           std::equal(a.data.begin(), a.data.begin() + a.length, b.data.begin());
}

inline uint32_t CommandTimeoutMs(const RobotFrame& frame) {
    const std::string command(frame.data.data() + 3, frame.length - 3);
    constexpr uint32_t fallback = 12000;
    if (command.rfind("kpu", 0) == 0)
        return 8000;
    const auto pos = command.find(' ');
    if (pos == std::string::npos)
        return fallback;
    char* end = nullptr;
    const long value = std::strtol(command.c_str() + pos, &end, 10);
    if (end == command.c_str() + pos || value <= 0)
        return fallback;
    if (command.rfind("kvtL", 0) == 0 || command.rfind("kvtR", 0) == 0)
        return std::clamp<uint64_t>(4000 + static_cast<uint64_t>(value) * 50, 6000, 15000);
    if (value < 200 && (command.rfind("kwk", 0) == 0 || command.rfind("kbk", 0) == 0 ||
                        command.rfind("ktr", 0) == 0 || command.rfind("kcr", 0) == 0))
        return std::clamp<uint64_t>(2000 + static_cast<uint64_t>(value) * 1000, 4000, 20000);
    return std::clamp<uint64_t>(value >= 200 ? static_cast<uint64_t>(value) + 3000
                                             : 3000 + static_cast<uint64_t>(value) * 1500,
                                fallback, 120000);
}
}  // namespace petoi

#endif
