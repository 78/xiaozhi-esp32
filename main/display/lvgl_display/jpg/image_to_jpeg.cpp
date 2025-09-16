// 基于原版to_jpg.cpp，替换为使用jpeg_encoder以节省SRAM
// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD

#include <stddef.h>
#include <string.h>
#include <memory>
#include <esp_attr.h>
#include <esp_heap_caps.h>
#include <esp_log.h>

#include "jpeg_encoder.h"  // 使用新的JPEG编码器
#include "image_to_jpeg.h"


#define TAG "image_to_jpeg"

static void *_malloc(size_t size)
{
    void * res = malloc(size);
    if(res) {
        return res;
    }

    // check if SPIRAM is enabled and is allocatable
#if (CONFIG_SPIRAM_SUPPORT && (CONFIG_SPIRAM_USE_CAPS_ALLOC || CONFIG_SPIRAM_USE_MALLOC))
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#endif
    return NULL;
}

static IRAM_ATTR void convert_line_format(uint8_t * src, pixformat_t format, uint8_t * dst, size_t width, size_t in_channels, size_t line)
{
    int i=0, o=0, l=0;
    if(format == PIXFORMAT_GRAYSCALE) {
        memcpy(dst, src + line * width, width);
    } else if(format == PIXFORMAT_RGB888) {
        l = width * 3;
        src += l * line;
        for(i=0; i<l; i+=3) {
            dst[o++] = src[i+2];
            dst[o++] = src[i+1];
            dst[o++] = src[i];
        }
    } else if(format == PIXFORMAT_RGB565) {
        l = width * 2;
        src += l * line;
        for(i=0; i<l; i+=2) {
            dst[o++] = src[i] & 0xF8;
            dst[o++] = (src[i] & 0x07) << 5 | (src[i+1] & 0xE0) >> 3;
            dst[o++] = (src[i+1] & 0x1F) << 3;
        }
    } else if(format == PIXFORMAT_YUV422) {
        // YUV422转RGB的简化实现
        l = width * 2;
        src += l * line;
        for(i=0; i<l; i+=4) {
            int y0 = src[i];
            int u = src[i+1];
            int y1 = src[i+2];
            int v = src[i+3];

            // 简化的YUV到RGB转换
            int c = y0 - 16;
            int d = u - 128;
            int e = v - 128;
            
            int r = (298 * c + 409 * e + 128) >> 8;
            int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
            int b = (298 * c + 516 * d + 128) >> 8;
            
            dst[o++] = (r < 0) ? 0 : ((r > 255) ? 255 : r);
            dst[o++] = (g < 0) ? 0 : ((g > 255) ? 255 : g);
            dst[o++] = (b < 0) ? 0 : ((b > 255) ? 255 : b);

            // Y1像素
            c = y1 - 16;
            r = (298 * c + 409 * e + 128) >> 8;
            g = (298 * c - 100 * d - 208 * e + 128) >> 8;
            b = (298 * c + 516 * d + 128) >> 8;
            
            dst[o++] = (r < 0) ? 0 : ((r > 255) ? 255 : r);
            dst[o++] = (g < 0) ? 0 : ((g > 255) ? 255 : g);
            dst[o++] = (b < 0) ? 0 : ((b > 255) ? 255 : b);
        }
    }
}

// 回调流实现 - 用于回调版本的JPEG编码
class callback_stream : public jpge2_simple::output_stream {
protected:
    jpg_out_cb ocb;
    void * oarg;
    size_t index;

public:
    callback_stream(jpg_out_cb cb, void * arg) : ocb(cb), oarg(arg), index(0) { }
    virtual ~callback_stream() { }
    virtual bool put_buf(const void* data, int len)
    {
        index += ocb(oarg, index, data, len);
        return true;
    }
    virtual jpge2_simple::uint get_size() const
    {
        return static_cast<jpge2_simple::uint>(index);
    }
};

// 内存流实现 - 用于直接内存输出
class memory_stream : public jpge2_simple::output_stream {
protected:
    uint8_t *out_buf;
    size_t max_len, index;

public:
    memory_stream(void *pBuf, uint buf_size) : out_buf(static_cast<uint8_t*>(pBuf)), max_len(buf_size), index(0) { }

    virtual ~memory_stream() { }

    virtual bool put_buf(const void* pBuf, int len)
    {
        if (!pBuf) {
            //end of image
            return true;
        }
        if ((size_t)len > (max_len - index)) {
            //ESP_LOGW(TAG, "JPG output overflow: %d bytes (%d,%d,%d)", len - (max_len - index), len, index, max_len);
            len = max_len - index;
        }
        if (len) {
            memcpy(out_buf + index, pBuf, len);
            index += len;
        }
        return true;
    }

    virtual jpge2_simple::uint get_size() const
    {
        return static_cast<jpge2_simple::uint>(index);
    }
};

// 使用优化的JPEG编码器进行图像转换，必须在堆上创建编码器
static bool convert_image(uint8_t *src, uint16_t width, uint16_t height, pixformat_t format, uint8_t quality, jpge2_simple::output_stream *dst_stream)
{
    int num_channels = 3;
    jpge2_simple::subsampling_t subsampling = jpge2_simple::H2V2;

    if(format == PIXFORMAT_GRAYSCALE) {
        num_channels = 1;
        subsampling = jpge2_simple::Y_ONLY;
    }

    if(!quality) {
        quality = 1;
    } else if(quality > 100) {
        quality = 100;
    }

    jpge2_simple::params comp_params = jpge2_simple::params();
    comp_params.m_subsampling = subsampling;
    comp_params.m_quality = quality;

    // ⚠️ 关键：必须在堆上创建编码器！约8KB内存从堆分配
    auto dst_image = std::make_unique<jpge2_simple::jpeg_encoder>();

    if (!dst_image->init(dst_stream, width, height, num_channels, comp_params)) {
        ESP_LOGE(TAG, "JPG encoder init failed");
        return false;
    }

    uint8_t* line = (uint8_t*)_malloc(width * num_channels);
    if(!line) {
        ESP_LOGE(TAG, "Scan line malloc failed");
        return false;
    }

    for (int i = 0; i < height; i++) {
        convert_line_format(src, format, line, width, num_channels, i);
        if (!dst_image->process_scanline(line)) {
            ESP_LOGE(TAG, "JPG process line %u failed", i);
            free(line);
            return false;
        }
    }
    free(line);

    if (!dst_image->process_scanline(NULL)) {
        ESP_LOGE(TAG, "JPG image finish failed");
        return false;
    }
    
    // dst_image会在unique_ptr销毁时自动释放内存
    return true;
}

// 🚀 主要函数：高效的图像到JPEG转换实现，节省8KB SRAM
bool image_to_jpeg(uint8_t *src, size_t src_len, uint16_t width, uint16_t height, pixformat_t format, uint8_t quality, uint8_t ** out, size_t * out_len)
{
    ESP_LOGI(TAG, "Using optimized JPEG encoder (saves ~8KB SRAM)");
    
    // 分配JPEG输出缓冲区，这个大小对于大多数图像应该足够
    int jpg_buf_len = 128*1024;

    uint8_t * jpg_buf = (uint8_t *)_malloc(jpg_buf_len);
    if(jpg_buf == NULL) {
        ESP_LOGE(TAG, "JPG buffer malloc failed");
        return false;
    }
    memory_stream dst_stream(jpg_buf, jpg_buf_len);

    if(!convert_image(src, width, height, format, quality, &dst_stream)) {
        free(jpg_buf);
        return false;
    }

    *out = jpg_buf;
    *out_len = dst_stream.get_size();
    return true;
}

// 🚀 回调版本：使用回调函数处理JPEG数据流，适合流式传输
bool image_to_jpeg_cb(uint8_t *src, size_t src_len, uint16_t width, uint16_t height, pixformat_t format, uint8_t quality, jpg_out_cb cb, void *arg)
{
    callback_stream dst_stream(cb, arg);
    return convert_image(src, width, height, format, quality, &dst_stream);
}

