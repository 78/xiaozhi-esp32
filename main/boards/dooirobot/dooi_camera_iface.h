// ============================================================================
// dooi_camera_iface.h
// ============================================================================
#ifndef DOOI_CAMERA_IFACE_H_
#define DOOI_CAMERA_IFACE_H_

#include "esp_video_init.h"

#ifndef v4l2_pix_fmt_t
typedef uint32_t v4l2_pix_fmt_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t *data;
    size_t len;
    uint16_t width;
    uint16_t height;
    v4l2_pix_fmt_t format;
} FrameBuffer;

FrameBuffer camera_get_frame();
FrameBuffer camera_get_web_frame();
FrameBuffer camera_get_display_frame();
FrameBuffer camera_get_face_frame();
FrameBuffer camera_get_gesture_frame();
FrameBuffer camera_get_cls_frame();

#ifdef __cplusplus
}
#endif

#endif // DOOI_CAMERA_IFACE_H_