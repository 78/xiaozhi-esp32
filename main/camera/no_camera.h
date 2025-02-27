#ifndef _NO_CAMERA_H_
#define _NO_CAMERA_H_

#include "camera.h"

class NoCamera : public Camera {
private:
    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

public:
    NoCamera();
    ~NoCamera();
};

#endif
