#include "camera_tuning.h"

#include <esp_log.h>

#include <algorithm>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "esp_video_device.h"
#include "esp_video_isp_ioctl.h"
#include "linux/videodev2.h"

static const char* TAG = "CamTuning";

namespace {

bool SetExtControl(int fd, uint32_t ctrl_class, uint32_t id, int32_t value) {
    v4l2_ext_control control = {};
    v4l2_ext_controls controls = {};
    control.id = id;
    control.value = value;
    controls.ctrl_class = ctrl_class;
    controls.count = 1;
    controls.controls = &control;
    if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
        ESP_LOGW(TAG, "set ctrl 0x%08lx=%ld failed, errno=%d", (unsigned long)id, (long)value,
                 errno);
        return false;
    }
    return true;
}

}  // namespace

bool ApplyCameraTuning(const CameraTuning& tuning) {
    bool any_ok = false;
    bool any_tried = false;

    int cam_fd = open(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, O_RDWR);
    if (cam_fd >= 0) {
        if (tuning.exposure_pct >= 0) {
            any_tried = true;
            // The valid exposure range depends on the sensor mode (max is
            // VTS-6 lines), so resolve it at runtime.
            v4l2_query_ext_ctrl qctrl = {};
            qctrl.id = V4L2_CID_EXPOSURE;
            if (ioctl(cam_fd, VIDIOC_QUERY_EXT_CTRL, &qctrl) == 0) {
                int32_t span = (int32_t)(qctrl.maximum - qctrl.minimum);
                int32_t value =
                    (int32_t)qctrl.minimum + span * std::min(tuning.exposure_pct, 100) / 100;
                if (SetExtControl(cam_fd, V4L2_CTRL_CLASS_CAMERA, V4L2_CID_EXPOSURE, value)) {
                    ESP_LOGI(TAG, "exposure=%ld (%d%% of [%ld..%ld])", (long)value,
                             tuning.exposure_pct, (long)qctrl.minimum, (long)qctrl.maximum);
                    any_ok = true;
                }
            } else {
                ESP_LOGW(TAG, "query exposure range failed, errno=%d", errno);
            }
        }
        if (tuning.gain_index >= 0) {
            any_tried = true;
            if (SetExtControl(cam_fd, V4L2_CTRL_CLASS_USER, V4L2_CID_GAIN, tuning.gain_index)) {
                ESP_LOGI(TAG, "gain index=%d", tuning.gain_index);
                any_ok = true;
            }
        }
        close(cam_fd);
    } else {
        ESP_LOGW(TAG, "open %s failed, errno=%d", ESP_VIDEO_MIPI_CSI_DEVICE_NAME, errno);
    }

    int isp_fd = open(ESP_VIDEO_ISP1_DEVICE_NAME, O_RDWR);
    if (isp_fd >= 0) {
        if (tuning.red_milli >= 0) {
            any_tried = true;
            if (SetExtControl(isp_fd, V4L2_CTRL_CLASS_USER, V4L2_CID_RED_BALANCE,
                              tuning.red_milli)) {
                ESP_LOGI(TAG, "red balance=%d/1000", tuning.red_milli);
                any_ok = true;
            }
        }
        if (tuning.blue_milli >= 0) {
            any_tried = true;
            if (SetExtControl(isp_fd, V4L2_CTRL_CLASS_USER, V4L2_CID_BLUE_BALANCE,
                              tuning.blue_milli)) {
                ESP_LOGI(TAG, "blue balance=%d/1000", tuning.blue_milli);
                any_ok = true;
            }
        }
        close(isp_fd);
    } else {
        ESP_LOGW(TAG, "open %s failed, errno=%d", ESP_VIDEO_ISP1_DEVICE_NAME, errno);
    }

    return any_ok || !any_tried;
}
