#ifndef __LICHUANG_CAMERA_H__
#define __LICHUANG_CAMERA_H__

#include "esp32_camera.h"

class LichuangDevPlusCamera : public Esp32Camera {
public:
    LichuangDevPlusCamera(const camera_config_t& config);
    virtual ~LichuangDevPlusCamera() = default;
};

#endif // __LICHUANG_CAMERA_H__
