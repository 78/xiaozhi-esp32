#ifndef MY_CAMERA_H
#define MY_CAMERA_H

#include <esp_timer.h>
#include <esp_log.h>
#include <esp_camera.h>
#include <string>
#include "camera.h"


class MyCamera : public Camera {
public:
    MyCamera();
    ~MyCamera();

    void* Capture(const char* format) override;
    int GetWidth() override;
    int GetHeight() override;
    int GetBufferSize() override;
    const char* GetFormat(void) override;

protected:
    void* buf_ = nullptr;
    int buf_size_ = 0;
    int width_ = 0;
    int height_ = 0;
    pixformat_t format_;
    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

private:

};


#endif
