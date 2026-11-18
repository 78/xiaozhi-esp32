#ifndef PETOI_ROBOT_FRAME_H_
#define PETOI_ROBOT_FRAME_H_

#include <array>
#include <cstddef>
#include <string>

namespace petoi {

constexpr size_t kMaxCommandLength = 128;

// Trivially copyable: FreeRTOS queues copy this object by value.
struct RobotFrame {
    std::array<char, kMaxCommandLength + 3> data{};
    size_t length = 0;
};

inline bool BuildRobotFrame(const std::string& command, RobotFrame& frame) {
    frame.length = 0;
    if (command.empty() || command.size() > kMaxCommandLength ||
        !((command.front() >= 'a' && command.front() <= 'z') ||
          (command.front() >= 'A' && command.front() <= 'Z'))) {
        return false;
    }
    for (unsigned char ch : command) {
        // One ASCII command per call; do not accept embedded frames or sequences.
        if (ch < 32 || ch > 126 || ch == ';') {
            return false;
        }
    }
    frame.data[0] = 'X';
    frame.data[1] = 'A';
    frame.data[2] = 0x15;
    command.copy(frame.data.data() + 3, command.size());
    frame.length = command.size() + 3;
    return true;
}

}  // namespace petoi

#endif  // PETOI_ROBOT_FRAME_H_
