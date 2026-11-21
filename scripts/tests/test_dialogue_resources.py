"""Exercise real resource monitor boundaries without ESP hardware."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class DialogueResourceTests(unittest.TestCase):
    def test_drained_replies_idle_recovery_and_incomplete_requests(self):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("requires a host C++17 compiler")
        stub = r'''
#pragma once
#include <cstddef>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>
inline std::vector<std::string> lines;
inline size_t available = 10000;
inline unsigned captures = 0;
constexpr int MALLOC_CAP_INTERNAL=1, MALLOC_CAP_8BIT=2, MALLOC_CAP_SPIRAM=4;
struct multi_heap_info_t {
    size_t total_free_bytes, largest_free_block, minimum_free_bytes;
    size_t total_allocated_bytes, allocated_blocks, free_blocks;
};
inline void heap_caps_get_info(multi_heap_info_t* info, int) {
    ++captures; *info={available,available/2,500,20000-available,20,10};
}
inline size_t heap_caps_get_free_size(int cap) { return cap & MALLOC_CAP_SPIRAM ? 0 : available; }
inline unsigned uxTaskGetNumberOfTasks() { return 12; }
inline unsigned uxTaskGetStackHighWaterMark(void*) { return 900; }
inline void record(const char*, const char* format, ...) {
    char buffer[2048]; va_list args; va_start(args,format);
    vsnprintf(buffer,sizeof(buffer),format,args); va_end(args); lines.emplace_back(buffer);
}
#define ESP_LOGD(...) record(__VA_ARGS__)
'''
        driver = r'''
#include <cassert>
#include "diagnostics/dialogue_resources.h"
bool has(const char* text) { return lines.back().find(text)!=std::string::npos; }
int main() {
    DialogueResources monitor;
    monitor.ResponseStarted(); monitor.ResponseStopped(false); // unsolicited greeting
    monitor.Poll(kDeviceStateListening,true,true,0);
    assert(lines.empty());
    monitor.Poll(kDeviceStateIdle,true,false,1);
    monitor.Poll(kDeviceStateIdle,true,false,3000001);
    assert(has("phase=idle_settled")); assert(has("previous_comparable=0"));
    auto n=lines.size();
    monitor.Poll(kDeviceStateIdle,true,false,9000000); assert(lines.size()==n);
    monitor.BeginRequest(kDeviceStateListening,true);
    monitor.ResponseStarted(); monitor.ResponseStopped(false);
    available=8000;
    monitor.Poll(kDeviceStateListening,false,true,10000000);
    assert(lines.size()==n+1); // STOP with queued playback is not the end
    monitor.Poll(kDeviceStateSpeaking,true,true,10000001);
    assert(lines.size()==n+1); // state/UI cleanup has not finished
    available=9500;
    monitor.Poll(kDeviceStateListening,true,true,11000000);
    assert(has("result=reply_complete")); assert(has("net_internal_used=500"));
    assert(has("sampled_min_internal_free=8000")); assert(has("previous_comparable=0"));
    monitor.BeginRequest(kDeviceStateListening,true);
    monitor.ResponseStarted(); monitor.ResponseStopped(false);
    monitor.Poll(kDeviceStateListening,true,true,12000000);
    assert(has("previous_comparable=1")); assert(has("previous_internal_used=0"));
    monitor.Poll(kDeviceStateIdle,true,false,13000000);
    available=10000;
    monitor.Poll(kDeviceStateIdle,true,false,16000000);
    assert(has("phase=idle_settled")); assert(has("previous_internal_used=0"));
    monitor.BeginRequest(kDeviceStateListening,true);
    monitor.ResponseStarted(); monitor.ResponseStopped(true);
    monitor.Poll(kDeviceStateListening,true,true,17000000);
    assert(has("result=interrupted")); assert(has("previous_comparable=0"));
    monitor.BeginRequest(kDeviceStateListening,true);
    monitor.ResponseStarted(); monitor.ResponseStopped(false);
    monitor.Poll(kDeviceStateIdle,true,false,18000000); // manual-stop uses a different end state
    assert(has("result=reply_complete")); assert(has("previous_comparable=0"));
    monitor.BeginRequest(kDeviceStateListening,true);
    monitor.Poll(kDeviceStateIdle,true,false,19000000);
    assert(has("result=channel_closed"));
    monitor.BeginRequest(kDeviceStateListening,true);
    auto before=lines.size();
    monitor.BeginRequest(kDeviceStateListening,true);
    assert(lines.size()==before+2);
    assert(lines[lines.size()-2].find("result=superseded")!=std::string::npos);
    // A stopped response superseded before draining does not become a good baseline.
    monitor.ResponseStarted(); monitor.ResponseStopped(false);
    monitor.BeginRequest(kDeviceStateListening,true);
    monitor.ResponseStarted(); monitor.ResponseStopped(false);
    monitor.Poll(kDeviceStateListening,true,true,20000000);
    assert(has("previous_comparable=0")); // last valid baseline was manual-stop idle
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / "stub.h").write_text(stub, encoding="utf-8")
            for name in ("esp_heap_caps.h", "esp_log.h", "freertos/FreeRTOS.h", "freertos/task.h"):
                header = path / name
                header.parent.mkdir(parents=True, exist_ok=True)
                header.write_text('#include "stub.h"\n', encoding="utf-8")
            source = path / "test.cc"
            binary = path / "test.exe"
            source.write_text(driver, encoding="utf-8")
            subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-I", str(path),
                            "-I", str(ROOT / "main"), str(source), "-o", str(binary)],
                           check=True, capture_output=True, text=True)
            subprocess.run([str(binary)], check=True, capture_output=True, text=True)


if __name__ == "__main__":
    unittest.main()
