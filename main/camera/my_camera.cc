#include <esp_log.h>
#include <esp_err.h>
#include <string>
#include <cstdlib>
#include <cstring>

#include "my_camera.h"
#include "board.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "MyCamera"

MyCamera::MyCamera() {
    ESP_LOGI(TAG, "New Camera");
    //skip 10 frames to let the sensor output stable and uptodate
    int count = 10;
    while(count--) {
        camera_fb_t *pic = esp_camera_fb_get();
        if (!pic) {
            ESP_LOGE(TAG, "Camera capture failed");
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }
        
        esp_camera_fb_return(pic);
    }
}

MyCamera::~MyCamera() {
    if(buf_ != nullptr) {
        free(buf_);
        buf_ = nullptr;
    }
}

static const char* format_2_string(pixformat_t id)
{
    const char* str = "Unknown";

    switch(id) {
        case PIXFORMAT_GRAYSCALE:
            str = "GRAYSCALE";
            break;
        case PIXFORMAT_JPEG:
            str = "JPEG";
            break;
        case PIXFORMAT_RAW:
            str = "RAW";
            break;
        case PIXFORMAT_RGB444:
            str = "RGB444";
            break;
        case PIXFORMAT_RGB555:
            str = "RGB555";
            break;
        case PIXFORMAT_RGB565:
            str = "RGB565";
            break;
        case PIXFORMAT_RGB888:
            str = "RGB888";
            break;
        case PIXFORMAT_YUV420:
            str = "YUV420";
            break;
        case PIXFORMAT_YUV422:
            str = "YUV422";
            break;
        default:
            break;
    }
    return str;
}

const char* MyCamera::GetFormat(void)
{
    return format_2_string(format_);
}

int MyCamera::GetWidth(void)
{
    return width_;
}

int MyCamera::GetHeight(void)
{
    return height_;
}

int MyCamera::GetBufferSize(void)
{
    return buf_size_;
}

/* only support jpeg format currently*/
void* MyCamera::Capture(const char* format) 
{
    //skip 3 frames to let the sensor output stable and uptodate
    int count = 3;
    while(count--) {
        camera_fb_t *pic = esp_camera_fb_get();
        if(pic) {
            vTaskDelay(pdMS_TO_TICKS(1));
            esp_camera_fb_return(pic);
        }
    }

    camera_fb_t *pic = esp_camera_fb_get();
    ESP_LOGI(TAG, "camera frame buffer size:%d width:%d height:%d format:%d (%s)", 
        pic->len, pic->width, pic->height, pic->format, format_2_string(pic->format));
    
    if(buf_ != nullptr) {
        ESP_LOGI(TAG, "free old buffer");
        free(buf_);
        buf_ = nullptr;
    }
    
    width_ = pic->width;
    height_ = pic->height;
    format_ = pic->format;

    /* Convert to JPEG */
    if (pic->format != PIXFORMAT_JPEG) {
        bool jpeg_converted = frame2jpg(pic, 80, (uint8_t **)&buf_, (size_t*)&buf_size_);
        static uint8_t fail_count = 0;
        if (!jpeg_converted) {
            fail_count++;
            if (fail_count > 5) {
                ESP_LOGE(TAG, "JPEG compression failed too many times, restarting camera");
                esp_restart();
            }
            ESP_LOGE(TAG, "JPEG compression failed");
            esp_camera_fb_return(pic);
            return NULL;
        } else {
            fail_count = 0;
            ESP_LOGI(TAG, "buffer compressed from %d to %d bytes", pic->len, buf_size_);
        }
    } else {
        buf_ = malloc(pic->len);
        if(buf_ == nullptr) {
            ESP_LOGE(TAG, "no memory!");
            return buf_;
        }
        
        memcpy(buf_, pic->buf, pic->len);
        buf_size_ = pic->len;
    }
    
    esp_camera_fb_return(pic);

    return buf_;
}

bool MyCamera::Lock(int timeout_ms) {
    return true;
}

void MyCamera::Unlock() {}