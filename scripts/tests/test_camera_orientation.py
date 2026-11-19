"""Exercise sensor defaults and explicit overrides against a host V4L2 double."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]


def function(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


class CameraOrientationTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which("c++"), "requires a host C++ compiler")
    def test_sensor_defaults_query_failure_and_explicit_override(self):
        common = (ROOT / "main/boards/common/esp_video.cc").read_text()
        header = (ROOT / "main/boards/common/esp_video.h").read_text()
        board = (
            ROOT / "main/boards/espressif/esp32-s31-korvo-1/esp32_s31_korvo_1.cc"
        ).read_text()
        source = r'''
#include <cassert>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>
#define ESP_LOGI(...)
#define ESP_LOGW(...)
struct esp_cam_sensor_id_t { uint16_t pid; };
struct v4l2_ext_control { uint32_t id, size; uint8_t* p_u8; };
struct v4l2_ext_controls { uint32_t ctrl_class, count; v4l2_ext_control* controls; };
constexpr int ESP_CAM_SENSOR_IOC_G_CHIP_ID=6, V4L2_CTRL_CLASS_ESP_CAM_IOCTL=7;
constexpr int VIDIOC_G_EXT_CTRLS=8;
constexpr bool kConfiguredHMirror=true, kConfiguredVFlip=false;
int query_result=0, query_count=0, callback_count=0;
uint16_t sensor_pid=0;
std::vector<std::pair<char,bool>> writes;
int ioctl(int fd, int command, v4l2_ext_controls* controls) {
    assert(fd==42 && command==VIDIOC_G_EXT_CTRLS);
    assert(controls->ctrl_class==V4L2_CTRL_CLASS_ESP_CAM_IOCTL && controls->count==1);
    auto& ctrl=*controls->controls;
    assert(ctrl.id==ESP_CAM_SENSOR_IOC_G_CHIP_ID && ctrl.size==sizeof(esp_cam_sensor_id_t));
    ++query_count;
    if (query_result) return query_result;
    reinterpret_cast<esp_cam_sensor_id_t*>(ctrl.p_u8)->pid=sensor_pid;
    return 0;
}
class EspVideo {
public:
'''
        source += header.split("public:\n", 1)[1].split("private:", 1)[0]
        source += r'''
    int video_fd_=42;
    void ConfigureOrientation(SensorOrientationCallback callback);
    bool SetHMirror(bool enabled) { writes.emplace_back('h',enabled); return true; }
    bool SetVFlip(bool enabled) { writes.emplace_back('v',enabled); return true; }
};
'''
        source += function(common, "void EspVideo::ConfigureOrientation(")
        source += function(board, "static std::optional<EspVideo::Orientation> GetCameraOrientation(")
        source += r'''
std::optional<EspVideo::Orientation> orientation(uint16_t pid) {
    ++callback_count;
    return GetCameraOrientation(pid);
}
int main() {
    EspVideo camera;
#if CONFIG_XIAOZHI_CAMERA_MIRROR_CONFIGURED
    camera.ConfigureOrientation(orientation);
    assert(query_count==0 && callback_count==0);
    assert((writes==std::vector<std::pair<char,bool>>{{'h',true},{'v',false}}));
#else
    camera.ConfigureOrientation(nullptr);
    assert(query_count==0 && writes.empty());
    sensor_pid=0x3660;
    camera.ConfigureOrientation(orientation);
    assert(query_count==1 && callback_count==1);
    assert((writes==std::vector<std::pair<char,bool>>{{'h',true},{'v',true}}));
    writes.clear(); sensor_pid=0xda4a;
    camera.ConfigureOrientation(orientation);
    assert(query_count==2 && callback_count==2);
    assert((writes==std::vector<std::pair<char,bool>>{{'h',false},{'v',false}}));
    writes.clear(); sensor_pid=0xffff;
    camera.ConfigureOrientation(orientation);
    assert(query_count==3 && callback_count==3 && writes.empty());
    query_result=-1; sensor_pid=0x3660;
    camera.ConfigureOrientation(orientation);
    assert(query_count==4 && callback_count==3 && writes.empty());
#endif
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            for explicit in (0, 1):
                with self.subTest(explicit_override=explicit):
                    binary = str(pathlib.Path(tmp) / f"orientation-{explicit}")
                    subprocess.run(
                        ["c++", "-std=c++17", "-fsanitize=undefined", "-x", "c++", "-",
                         f"-DCONFIG_XIAOZHI_CAMERA_MIRROR_CONFIGURED={explicit}",
                         "-o", binary],
                        input=source, text=True, check=True,
                    )
                    subprocess.run([binary], check=True)

        constructor = function(common, "EspVideo::EspVideo(")
        self.assertLess(constructor.index("ConfigureOrientation(orientation_callback)"),
                        constructor.index("VIDIOC_STREAMON"))
