#ifndef CAMERA_H
#define CAMERA_H

#include <esp_timer.h>
#include <esp_log.h>
#include <esp_camera.h>
#include <string>


class Camera {
public:
    Camera();
    virtual ~Camera();

    virtual void* Capture(const char* format);
    virtual int GetWidth();
    virtual int GetHeight();
    virtual int GetBufferSize();
    virtual const char* GetFormat();

protected:
    virtual bool Lock(int timeout_ms = 0) = 0;
    virtual void Unlock() = 0;

private:

};


#endif
