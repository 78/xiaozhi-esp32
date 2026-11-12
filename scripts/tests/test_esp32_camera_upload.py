"""Exercise the real camera upload methods with a scripted HTTP adapter."""

import os
import shlex
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class Esp32CameraUploadTest(unittest.TestCase):
    def test_body_writes_and_invalid_write_counts(self):
        compiler = shlex.split(os.environ.get("CXX", "c++"))
        if not compiler or not shutil.which(compiler[0]):
            self.skipTest("requires a host C++23 compiler")
        source = (ROOT / "main/boards/common/esp32_camera.cc").read_text()
        # Both Explain overloads are the final methods in the production file.
        methods = source[source.index(
            "std::expected<std::string, std::string> Esp32Camera::Explain("
        ):]
        driver = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <expected>
#include <map>
#include <memory>
#include <string>

#define ESP_LOGI(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
enum {
    PIXFORMAT_RGB565, PIXFORMAT_YUV422, PIXFORMAT_YUV420,
    PIXFORMAT_GRAYSCALE, PIXFORMAT_JPEG, PIXFORMAT_RGB888,
    V4L2_PIX_FMT_RGB565, V4L2_PIX_FMT_YUYV, V4L2_PIX_FMT_YUV420,
    V4L2_PIX_FMT_GREY, V4L2_PIX_FMT_JPEG, V4L2_PIX_FMT_RGB24
};
using v4l2_pix_fmt_t = int;
struct Frame {
    uint8_t* buf;
    size_t len;
    uint16_t width = 320, height = 240;
    int format = PIXFORMAT_JPEG;
};
static const std::string jpeg("\xff\xd8\0\xff\xd9", 5);
static uint8_t* jpeg_allocation;
bool image_to_jpeg(uint8_t*, size_t, uint16_t, uint16_t, int, int,
                   uint8_t** output, size_t* size) {
    assert(!jpeg_allocation);
    jpeg_allocation = static_cast<uint8_t*>(std::malloc(jpeg.size()));
    assert(jpeg_allocation);
    std::memcpy(jpeg_allocation, jpeg.data(), jpeg.size());
    *output = jpeg_allocation;
    *size = jpeg.size();
    return true;
}
void FreeJpeg(void* data) {
    assert(data == jpeg_allocation);
    std::free(data);
    jpeg_allocation = nullptr;
}
int64_t esp_timer_get_time() { return 0; }
size_t uxTaskGetStackHighWaterMark(void*) { return 1024; }
struct Error { std::string ToString() const { return "test error"; } };
enum class Mode { Full, Partial, Oversized, Zero, Negative, Failure };
struct State {
    Mode mode = Mode::Full;
    std::map<std::string, std::string> headers;
    std::string body, url;
    int writes = 0, closes = 0, status_reads = 0;
} state;
class Http {
public:
    void SetHeader(const std::string& key, const std::string& value) {
        state.headers[key] = value;
    }
    std::expected<void, Error> Open(const std::string& method, const std::string& url) {
        assert(method == "POST"); state.url = url; return {};
    }
    std::expected<int, Error> Write(const char* data, size_t size) {
        ++state.writes;
        switch (state.mode) {
            case Mode::Oversized:
                // Legacy chunk framing reports more bytes than the supplied payload.
                // A second write would consume an underflowed size and invalid pointer.
                assert(state.writes == 1);
                return static_cast<int>(size + 8);
            case Mode::Zero: return 0;
            case Mode::Negative: return -1;
            case Mode::Failure: return std::unexpected(Error{});
            default: break;
        }
        const size_t count = state.mode == Mode::Partial ? std::min(size, size_t(3)) : size;
        state.body.append(data, count);
        return static_cast<int>(count);
    }
    void Close() { ++state.closes; }
    std::expected<int, Error> GetStatusCode() { ++state.status_reads; return 200; }
    std::string ReadAll() { return "explained"; }
};
struct Network {
    std::unique_ptr<Http> CreateHttp(int id) {
        assert(id == 3); return std::make_unique<Http>();
    }
};
struct Board {
    static Board& GetInstance() { static Board board; return board; }
    Network* GetNetwork() { static Network network; return &network; }
    std::string GetUuid() { return "client-id"; }
};
struct SystemInfo { static std::string GetMacAddress() { return "device-id"; } };
class Esp32Camera {
public:
    Frame* current_fb_;
    uint8_t* encode_buf_ = nullptr;
    size_t encode_buf_size_ = 0;
    std::string explain_url_ = "https://legacy.example/", explain_token_ = "legacy-token";
    std::expected<std::string, std::string> Explain(const std::string&);
    std::expected<std::string, std::string> Explain(
        const std::string&, const std::string&, const std::string&);
};
#define free FreeJpeg
// @METHODS@
#undef free
int main() {
    Frame frame{nullptr, 0};
    Esp32Camera camera{&frame};
    for (Mode mode : {Mode::Full, Mode::Partial}) {
        state = {}; state.mode = mode;
        auto result = camera.Explain("look");
        assert(result && *result == "explained");
        assert(state.url == camera.explain_url_);
        assert(state.headers.at("Authorization") == "Bearer legacy-token");
        assert(state.headers.at("Device-Id") == "device-id");
        assert(state.headers.at("Client-Id") == "client-id");
        assert(state.headers.at("Content-Length") == std::to_string(state.body.size()));
        assert(!state.headers.contains("Transfer-Encoding"));
        assert(state.body.find("\r\n\r\nlook\r\n") != std::string::npos);
        assert(state.body.find(jpeg) != std::string::npos);
        assert(state.body.ends_with("\r\n------ESP32_CAMERA_BOUNDARY--\r\n"));
        assert(state.closes == 1 && state.status_reads == 1 && !jpeg_allocation);
    }
    for (Mode mode : {Mode::Oversized, Mode::Zero, Mode::Negative, Mode::Failure}) {
        state = {}; state.mode = mode;
        auto result = camera.Explain("look");
        assert(!result && result.error() == "Failed to upload photo");
        assert(state.writes == 1 && state.closes == 1 && state.status_reads == 0);
        assert(!jpeg_allocation);
    }
}
'''.replace("// @METHODS@", methods)
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / "camera_upload.cc"
            binary = Path(directory) / "camera_upload"
            cpp.write_text(driver)
            compiled = subprocess.run(
                compiler + ["-std=c++23", "-fno-exceptions", "-g",
                            "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                            str(cpp), "-o", str(binary)],
                capture_output=True, text=True,
            )
            self.assertEqual(compiled.returncode, 0, compiled.stderr)
            result = subprocess.run([str(binary)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
