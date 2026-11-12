"""Preserve ESP-Hi emotion routing when replacing its dynamic lookup table."""

import os
import shlex
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class EspHiEmotionRoutingTest(unittest.TestCase):
    def test_alias_fps_fallback_and_player_failure(self):
        compiler = shlex.split(os.environ.get("CXX", "c++"))
        if not compiler or not shutil.which(compiler[0]):
            self.skipTest("requires a host C++17 compiler")
        source = (ROOT / "main/boards/espressif/esp-hi/emoji_display.cc").read_text()
        start = source.index("bool EmojiWidget::SetEmotion(")
        method = source[start:source.index("\nvoid EmojiWidget::SetStatus", start)]
        driver = r'''
            #include <cassert>
            #include <cstring>
            #include <memory>
            #include <string>
            #include <tuple>
            #include <unordered_map>
            struct Player {
                int calls = 0, fps = 0;
                bool repeat = false, success = true;
                std::string asset;
                bool StartPlayer(const std::string& name, bool loop, int rate) {
                    ++calls; asset = name; repeat = loop; fps = rate;
                    return success;
                }
            };
            struct EmojiWidget {
                std::unique_ptr<Player> player_ = std::make_unique<Player>();
                bool SetEmotion(const char*);
            };
        ''' + method + r'''
            int main() {
                EmojiWidget widget;
                auto& player = *widget.player_;
                assert(widget.SetEmotion("laughing"));
                assert(player.asset == "happy_loop" && player.repeat && player.fps == 25);
                assert(widget.SetEmotion("winking"));
                assert(player.asset == "blink_quick" && player.repeat && player.fps == 5);
                int calls = player.calls;
                assert(widget.SetEmotion("neutral") && player.calls == calls);
                assert(!widget.SetEmotion("unknown") && player.calls == calls);
                assert(!widget.SetEmotion(nullptr) && player.calls == calls);
                player.success = false;
                assert(!widget.SetEmotion("sad"));
                assert(player.asset == "sad_loop" && player.calls == calls + 1);
                widget.player_.reset();
                assert(!widget.SetEmotion("neutral") && !widget.SetEmotion("laughing"));
            }
        '''
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            (build / "test.cc").write_text(driver)
            result = subprocess.run(
                compiler + ["-std=c++17", str(build / "test.cc"), "-o", str(build / "test")],
                capture_output=True, text=True,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            result = subprocess.run([str(build / "test")], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
