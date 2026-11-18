"""Check the actual C++ wire encoder without requiring ESP-IDF or hardware."""

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


class PetoiRobotFrameTest(unittest.TestCase):
    def test_wire_bytes_and_rejected_inputs(self):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("A host C++ compiler is required")
        board = Path(__file__).resolve().parents[2] / "main/boards/petoi/bittle-ai-c3"
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "frame_test.cc"
            binary = Path(directory) / "frame_test.exe"
            source.write_text(r'''
#include "robot_frame.h"
#include <cassert>
#include <string>

int main() {
    petoi::RobotFrame frame;
    assert(petoi::BuildRobotFrame("kup", frame));
    assert(frame.length == 6);
    assert(std::string(frame.data.data(), frame.length) == std::string("XA\x15" "kup", 6));
    assert(petoi::BuildRobotFrame("m0 -30", frame));
    assert(frame.length == 9);
    assert(petoi::BuildRobotFrame(std::string(128, 'k'), frame));
    assert(frame.length == 131);
    const std::string invalid[] = {
        "", " kup", "kup\n", "kup\r", "kup;ksit", "kup\t",
        std::string(129, 'k'), std::string("kup\0d", 5),
        std::string("k\x15", 2), std::string("k\x7f", 2),
        std::string("k\xc3\xa9", 3), "123"
    };
    for (const auto& command : invalid) {
        assert(!petoi::BuildRobotFrame(command, frame));
        assert(frame.length == 0);
    }
    // Repeated commands in separate calls remain valid (no cross-turn deduplication).
    assert(petoi::BuildRobotFrame("d", frame));
    assert(petoi::BuildRobotFrame("d", frame));
    assert(frame.length == 4);
}
''', encoding="utf-8")
            subprocess.run(
                [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-I", str(board),
                 str(source), "-o", str(binary)],
                check=True, capture_output=True, text=True,
            )
            subprocess.run([str(binary)], check=True, capture_output=True, text=True)


if __name__ == "__main__":
    unittest.main()
