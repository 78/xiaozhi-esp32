// ============================================================================
// dooi_camera.h 
// ============================================================================
#ifndef DOOI_CAMERA_H_
#define DOOI_CAMERA_H_

#include <cstdint>
#include <cstddef>
#include <string>

#include "camera.h"
#include "esp_video_init.h"
#include "linux/videodev2.h"
#include "dooi_camera_iface.h"

struct MmapBuffer {
    void   *start  = nullptr;
    size_t  length = 0;
};

struct JpegChunk {
    uint8_t *data;
    size_t   len;
};

class DooiCamera : public Camera {
public:
    explicit DooiCamera(const esp_video_init_config_t &config);
    ~DooiCamera();

    void        SetExplainUrl(const std::string &url, const std::string &token) override;
    bool        Capture() override;
    bool        SetHMirror(bool enabled) override;
    bool        SetVFlip(bool enabled) override;
    std::string Explain(const std::string &question) override;

private:
    std::string explain_url_;
    std::string explain_token_;
};

#endif
