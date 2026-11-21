"""Run the actual deferred TTS handlers through channel-close races on the host."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


def scheduled_body(block):
    start = block.index("{", block.index("Schedule("))
    depth, end = 1, start + 1
    while depth:
        depth += (block[end] == "{") - (block[end] == "}")
        end += 1
    return block[start + 1:end - 1]


class TtsClosedChannelTests(unittest.TestCase):
    def test_real_deferred_handlers_recheck_channel_at_execution(self):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("requires a host C++17 compiler")
        source = (ROOT / "main/application.cc").read_text(encoding="utf-8")
        tts = source.split('else if (strcmp(type->valuestring, "tts") == 0)', 1)[1]
        tts = tts.split('else if (strcmp(type->valuestring, "stt") == 0)', 1)[0]
        bodies = {name: scheduled_body(tts.split(f'if (strcmp(state->valuestring, "{name}") == 0)', 1)[1])
                  for name in ("start", "stop", "sentence_start")}
        driver = r'''
#include <cassert>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>
#define ESP_LOGW(...) ((void)0)
enum State { kDeviceStateIdle, kDeviceStateListening, kDeviceStateSpeaking };
enum Mode { kListeningModeAutoStop, kListeningModeManualStop };
struct FakeProtocol { bool open=false; bool IsAudioChannelOpened() const { return open; } };
struct Display {
    int updates=0;
    void AddTextGlyphs(const std::vector<int>&, uint8_t) { ++updates; }
    void SetChatMessage(const char*, const char*) { ++updates; }
};
struct Application {
    FakeProtocol* protocol_=nullptr;
    State state=kDeviceStateIdle;
    Mode listening_mode_=kListeningModeAutoStop;
    bool aborted_=true;
    Display ui;
    std::vector<std::function<void()>> pending;
    void Schedule(std::function<void()> task) { pending.push_back(std::move(task)); }
    void Run() { auto tasks=std::move(pending); pending.clear(); for(auto& task:tasks) task(); }
    State GetDeviceState() const { return state; }
    void SetDeviceState(State next) { state=next; }
    void Start() { Schedule([this]() { @START@ }); }
    void Stop() { Schedule([this]() { @STOP@ }); }
    void Sentence() {
        auto* display=&ui;
        Schedule([this,display,message=std::string("test"),glyphs=std::vector<int>{1},bpp=uint8_t(1)]() {
            @SENTENCE@
        });
    }
};
int main() {
    FakeProtocol protocol;
    Application app; app.protocol_=&protocol;
    // A late start after a timeout must not enter silent speaking or clear abort.
    app.Start(); app.Run(); assert(app.state==kDeviceStateIdle && app.aborted_);
    // It can be valid on receipt and become invalid before the main task runs it.
    protocol.open=true; app.state=kDeviceStateListening; app.Start();
    protocol.open=false; app.state=kDeviceStateIdle; app.Run();
    assert(app.state==kDeviceStateIdle && app.aborted_);
    // Protocol teardown before execution is also safe.
    protocol.open=true; app.Start(); app.protocol_=nullptr; app.Run();
    assert(app.state==kDeviceStateIdle); app.protocol_=&protocol;
    // Preserve normal speech, captions, and automatic/manual stop behavior.
    protocol.open=true; app.state=kDeviceStateListening; app.Start(); app.Run();
    assert(app.state==kDeviceStateSpeaking && !app.aborted_);
    app.Sentence(); app.Run(); assert(app.ui.updates==2);
    app.Stop(); app.Run(); assert(app.state==kDeviceStateListening);
    app.Start(); app.Run(); app.listening_mode_=kListeningModeManualStop;
    app.Stop(); app.Run(); assert(app.state==kDeviceStateIdle);
    // A queued caption must not repaint a closed conversation.
    app.Sentence(); protocol.open=false; app.Run(); assert(app.ui.updates==2);
    // Stop must not restart listening while the close callback is still queued.
    app.state=kDeviceStateSpeaking; app.listening_mode_=kListeningModeAutoStop;
    app.Stop(); app.Run(); assert(app.state==kDeviceStateSpeaking);
    app.SetDeviceState(kDeviceStateIdle); // the pending channel-close transition
}
'''.replace("@START@", bodies["start"]).replace("@STOP@", bodies["stop"]).replace("@SENTENCE@", bodies["sentence_start"])
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            cpp, exe = path / "tts.cc", path / "tts.exe"
            cpp.write_text(driver, encoding="utf-8")
            compiled = subprocess.run([compiler, "-std=c++17", str(cpp), "-o", str(exe)], capture_output=True)
            self.assertEqual(compiled.returncode, 0, (compiled.stdout + compiled.stderr).decode(errors="replace"))
            result = subprocess.run([str(exe)], capture_output=True)
            self.assertEqual(result.returncode, 0, (result.stdout + result.stderr).decode(errors="replace"))
