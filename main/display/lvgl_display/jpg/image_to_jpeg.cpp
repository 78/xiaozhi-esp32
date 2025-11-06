#include <esp_attr.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <stddef.h>
#include <string.h>

#include "esp_jpeg_common.h"
#include "esp_jpeg_enc.h"
#if CONFIG_XIAOZHI_ENABLE_HARDWARE_JPEG_ENCODER
#include "driver/jpeg_encode.h"
#endif
#include "image_to_jpeg.h"

#define TAG "image_to_jpeg"

static void* malloc_psram(size_t size) {
    void* p = malloc(size);
    if (p)
        return p;
#if (CONFIG_SPIRAM_SUPPORT && (CONFIG_SPIRAM_USE_CAPS_ALLOC || CONFIG_SPIRAM_USE_MALLOC))
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    return NULL;
#endif
}

static __always_inline uint8_t expand_5_to_8(uint8_t v) {
    return (uint8_t)((v << 3) | (v >> 2));
}

static __always_inline uint8_t expand_6_to_8(uint8_t v) {
    return (uint8_t)((v << 2) | (v >> 4));
}

static uint8_t* convert_input_to_encoder_buf(const uint8_t* src, uint16_t width, uint16_t height, v4l2_pix_fmt_t format,
                                             jpeg_pixel_format_t* out_fmt, int* out_size) {
    // 直接支持的格式：GRAY、RGB888、YCbYCr(YUYV)
    if (format == V4L2_PIX_FMT_GREY) {
        int sz = (int)width * (int)height;
        uint8_t* buf = (uint8_t*)jpeg_calloc_align(sz, 16);
        if (!buf)
            return NULL;
        memcpy(buf, src, sz);
        if (out_fmt)
            *out_fmt = JPEG_PIXEL_FORMAT_GRAY;
        if (out_size)
            *out_size = sz;
        return buf;
    }

    // V4L2 YUYV (Y Cb Y Cr) 可直接作为 JPEG_PIXEL_FORMAT_YCbYCr 输入
    if (format == V4L2_PIX_FMT_YUYV) {
        int sz = (int)width * (int)height * 2;
        uint8_t* buf = (uint8_t*)jpeg_calloc_align(sz, 16);
        if (!buf)
            return NULL;
        memcpy(buf, src, sz);
        if (out_fmt)
            *out_fmt = JPEG_PIXEL_FORMAT_YCbYCr;
        if (out_size)
            *out_size = sz;
        return buf;
    }

    // V4L2 UYVY (Cb Y Cr Y) -> 重排为 YUYV 再作为 YCbYCr 输入
    if (format == V4L2_PIX_FMT_UYVY) {
        int sz = (int)width * (int)height * 2;
        const uint8_t* s = src;
        uint8_t* buf = (uint8_t*)jpeg_calloc_align(sz, 16);
        if (!buf)
            return NULL;
        uint8_t* d = buf;
        for (int i = 0; i < sz; i += 4) {
            // src: Cb, Y0, Cr, Y1 -> dst: Y0, Cb, Y1, Cr
            d[0] = s[1];
            d[1] = s[0];
            d[2] = s[3];
            d[3] = s[2];
            s += 4;
            d += 4;
        }
        if (out_fmt)
            *out_fmt = JPEG_PIXEL_FORMAT_YCbYCr;
        if (out_size)
            *out_size = sz;
        return buf;
    }

    // V4L2 YUV422P (YUV422 Planar) -> 重排为 YUYV (YCbYCr)
    if (format == V4L2_PIX_FMT_YUV422P) {
        int sz = (int)width * (int)height * 2;
        const uint8_t* y_plane = src;
        const uint8_t* u_plane = y_plane + (int)width * (int)height;
        const uint8_t* v_plane = u_plane + ((int)width / 2) * (int)height;
        uint8_t* buf = (uint8_t*)jpeg_calloc_align(sz, 16);
        if (!buf)
            return NULL;
        uint8_t* dst = buf;
        for (int y = 0; y < height; y++) {
            const uint8_t* y_row = y_plane + y * (int)width;
            const uint8_t* u_row = u_plane + y * ((int)width / 2);
            const uint8_t* v_row = v_plane + y * ((int)width / 2);
            for (int x = 0; x < width; x += 2) {
                uint8_t y0 = y_row[x + 0];
                uint8_t y1 = y_row[x + 1];
                uint8_t cb = u_row[x / 2];
                uint8_t cr = v_row[x / 2];
                dst[0] = y0;
                dst[1] = cb;
                dst[2] = y1;
                dst[3] = cr;
                dst += 4;
            }
        }
        if (out_fmt)
            *out_fmt = JPEG_PIXEL_FORMAT_YCbYCr;
        if (out_size)
            *out_size = sz;
        return buf;
    }

    // 其余格式转换为 RGB888
    int rgb_size = (int)width * (int)height * 3;
    uint8_t* rgb = (uint8_t*)jpeg_calloc_align(rgb_size, 16);
    if (!rgb)
        return NULL;

    if (format == V4L2_PIX_FMT_RGB24) {
        // V4L2_RGB24 即 RGB888
        memcpy(rgb, src, rgb_size);
    } else if (format == V4L2_PIX_FMT_RGB565) {
        // RGB565 小端，需要转换为 RGB888
        const uint8_t* p = src;
        uint8_t* d = rgb;
        int pixels = (int)width * (int)height;
        for (int i = 0; i < pixels; i++) {
            uint8_t lo = p[0];  // 低字节（LSB）
            uint8_t hi = p[1];  // 高字节（MSB）
            p += 2;

            uint8_t r5 = (hi >> 3) & 0x1F;
            uint8_t g6 = ((hi & 0x07) << 3) | ((lo & 0xE0) >> 5);
            uint8_t b5 = lo & 0x1F;

            d[0] = expand_5_to_8(r5);
            d[1] = expand_6_to_8(g6);
            d[2] = expand_5_to_8(b5);
            d += 3;
        }
    } else {
        // 其他未覆盖格式，清零
        memset(rgb, 0, rgb_size);
    }

    if (out_fmt)
        *out_fmt = JPEG_PIXEL_FORMAT_RGB888;
    if (out_size)
        *out_size = rgb_size;
    return rgb;
}

#if CONFIG_XIAOZHI_ENABLE_HARDWARE_JPEG_ENCODER
static jpeg_encoder_handle_t s_hw_jpeg_handle = NULL;

static bool hw_jpeg_ensure_inited(void) {
    if (s_hw_jpeg_handle) {
        return true;
    }
    jpeg_encode_engine_cfg_t eng_cfg = {
        .intr_priority = 0,
        .timeout_ms = 100,
    };
    esp_err_t er = jpeg_new_encoder_engine(&eng_cfg, &s_hw_jpeg_handle);
    if (er != ESP_OK) {
        ESP_LOGE(TAG, "jpeg_new_encoder_engine failed: %d", (int)er);
        s_hw_jpeg_handle = NULL;
        return false;
    }
    return true;
}

static uint8_t* convert_input_to_hw_encoder_buf(const uint8_t* src, uint16_t width, uint16_t height, v4l2_pix_fmt_t format,
                                                jpeg_enc_input_format_t* out_fmt, int* out_size) {
    if (format == V4L2_PIX_FMT_GREY) {
        int sz = (int)width * (int)height;
        uint8_t* buf = (uint8_t*)malloc_psram(sz);
        if (!buf)
            return NULL;
        memcpy(buf, src, sz);
        if (out_fmt)
            *out_fmt = JPEG_ENCODE_IN_FORMAT_GRAY;
        if (out_size)
            *out_size = sz;
        return buf;
    }

    if (format == V4L2_PIX_FMT_RGB24) {
        int sz = (int)width * (int)height * 3;
        uint8_t* buf = (uint8_t*)malloc_psram(sz);
        if (!buf) {
            ESP_LOGE(TAG, "malloc_psram failed");
            return NULL;
        }
        memcpy(buf, src, sz);
        if (out_fmt)
            *out_fmt = JPEG_ENCODE_IN_FORMAT_RGB888;
        if (out_size)
            *out_size = sz;
        return buf;
    }

    if (format == V4L2_PIX_FMT_RGB565) {
        int sz = (int)width * (int)height * 2;
        uint8_t* buf = (uint8_t*)malloc_psram(sz);
        if (!buf)
            return NULL;
        memcpy(buf, src, sz);
        if (out_fmt)
            *out_fmt = JPEG_ENCODE_IN_FORMAT_RGB565;
        if (out_size)
            *out_size = sz;
        return buf;
    }

    if (format == V4L2_PIX_FMT_YUYV) {
        // 硬件需要 | Y1 V Y0 U | 的“大端”格式，因此需要 bswap16
        int sz = (int)width * (int)height * 2;
        uint16_t* buf = (uint16_t*)malloc_psram(sz);
        if (!buf)
            return NULL;
        const uint16_t* bsrc = (const uint16_t*)src;
        for (int i = 0; i < sz / 2; i++) {
            buf[i] = __builtin_bswap16(bsrc[i]);
        }
        if (out_fmt)
            *out_fmt = JPEG_ENCODE_IN_FORMAT_YUV422;
        if (out_size)
            *out_size = sz;
        return (uint8_t*)buf;
    }

    return NULL;
}

static bool encode_with_hw_jpeg(const uint8_t* src, size_t src_len, uint16_t width, uint16_t height,
                                v4l2_pix_fmt_t format, uint8_t quality, uint8_t** jpg_out, size_t* jpg_out_len,
                                jpg_out_cb cb, void* cb_arg) {
    if (quality < 1)
        quality = 1;
    if (quality > 100)
        quality = 100;

    jpeg_enc_input_format_t enc_src_type = JPEG_ENCODE_IN_FORMAT_RGB888;
    int enc_in_size = 0;
    uint8_t* enc_in = convert_input_to_hw_encoder_buf(src, width, height, format, &enc_src_type, &enc_in_size);
    if (!enc_in) {
        ESP_LOGW(TAG, "hw jpeg: unsupported format, fallback to sw");
        return false;
    }

    if (!hw_jpeg_ensure_inited()) {
        free(enc_in);
        return false;
    }

    jpeg_encode_cfg_t enc_cfg = {0};
    enc_cfg.width = width;
    enc_cfg.height = height;
    enc_cfg.src_type = enc_src_type;
    enc_cfg.image_quality = quality;
    enc_cfg.sub_sample = (enc_src_type == JPEG_ENCODE_IN_FORMAT_GRAY) ? JPEG_DOWN_SAMPLING_GRAY : JPEG_DOWN_SAMPLING_YUV422;

    size_t out_cap = (size_t)width * (size_t)height * 3 / 2 + 64 * 1024;
    if (out_cap < 128 * 1024)
        out_cap = 128 * 1024;
    jpeg_encode_memory_alloc_cfg_t jpeg_enc_output_mem_cfg = { .buffer_direction = JPEG_ENC_ALLOC_OUTPUT_BUFFER };
    size_t out_cap_aligned = 0;
    uint8_t* outbuf = (uint8_t*)jpeg_alloc_encoder_mem(out_cap, &jpeg_enc_output_mem_cfg, &out_cap_aligned);
    if (!outbuf) {
        free(enc_in);
        ESP_LOGE(TAG, "alloc out buffer failed");
        return false;
    }

    uint32_t out_len = 0;
    esp_err_t er = jpeg_encoder_process(s_hw_jpeg_handle, &enc_cfg, enc_in, (uint32_t)enc_in_size, outbuf, (uint32_t)out_cap_aligned, &out_len);
    free(enc_in);

    if (er != ESP_OK) {
        free(outbuf);
        ESP_LOGE(TAG, "jpeg_encoder_process failed: %d", (int)er);
        return false;
    }

    if (cb) {
        cb(cb_arg, 0, outbuf, (size_t)out_len);
        cb(cb_arg, 1, NULL, 0);
        free(outbuf);
        if (jpg_out)
            *jpg_out = NULL;
        if (jpg_out_len)
            *jpg_out_len = 0;
        return true;
    }

    if (jpg_out && jpg_out_len) {
        *jpg_out = outbuf;
        *jpg_out_len = (size_t)out_len;
        return true;
    }

    free(outbuf);
    return true;
}
#endif // CONFIG_XIAOZHI_ENABLE_HARDWARE_JPEG_ENCODER

static bool encode_with_esp_new_jpeg(const uint8_t* src, size_t src_len, uint16_t width, uint16_t height,
                                     v4l2_pix_fmt_t format, uint8_t quality, uint8_t** jpg_out, size_t* jpg_out_len,
                                     jpg_out_cb cb, void* cb_arg) {
    if (quality < 1)
        quality = 1;
    if (quality > 100)
        quality = 100;

    jpeg_pixel_format_t enc_src_type = JPEG_PIXEL_FORMAT_RGB888;
    int enc_in_size = 0;
    uint8_t* enc_in = convert_input_to_encoder_buf(src, width, height, format, &enc_src_type, &enc_in_size);
    if (!enc_in) {
        ESP_LOGE(TAG, "alloc/convert input failed");
        return false;
    }

    jpeg_enc_config_t cfg = DEFAULT_JPEG_ENC_CONFIG();
    cfg.width = width;
    cfg.height = height;
    cfg.src_type = enc_src_type;
    cfg.subsampling = (enc_src_type == JPEG_PIXEL_FORMAT_GRAY) ? JPEG_SUBSAMPLE_GRAY : JPEG_SUBSAMPLE_420;
    cfg.quality = quality;
    cfg.rotate = JPEG_ROTATE_0D;
    cfg.task_enable = false;

    jpeg_enc_handle_t h = NULL;
    jpeg_error_t ret = jpeg_enc_open(&cfg, &h);
    if (ret != JPEG_ERR_OK) {
        jpeg_free_align(enc_in);
        ESP_LOGE(TAG, "jpeg_enc_open failed: %d", (int)ret);
        return false;
    }

    // 估算输出缓冲区：宽高的 1.5 倍 + 64KB
    size_t out_cap = (size_t)width * (size_t)height * 3 / 2 + 64 * 1024;
    if (out_cap < 128 * 1024)
        out_cap = 128 * 1024;
    uint8_t* outbuf = (uint8_t*)malloc_psram(out_cap);
    if (!outbuf) {
        jpeg_enc_close(h);
        jpeg_free_align(enc_in);
        ESP_LOGE(TAG, "alloc out buffer failed");
        return false;
    }

    int out_len = 0;
    ret = jpeg_enc_process(h, enc_in, enc_in_size, outbuf, (int)out_cap, &out_len);
    jpeg_enc_close(h);
    jpeg_free_align(enc_in);

    if (ret != JPEG_ERR_OK) {
        free(outbuf);
        ESP_LOGE(TAG, "jpeg_enc_process failed: %d", (int)ret);
        return false;
    }

    if (cb) {
        cb(cb_arg, 0, outbuf, (size_t)out_len);
        cb(cb_arg, 1, NULL, 0);  // 结束信号
        free(outbuf);
        if (jpg_out)
            *jpg_out = NULL;
        if (jpg_out_len)
            *jpg_out_len = 0;
        return true;
    }

    if (jpg_out && jpg_out_len) {
        *jpg_out = outbuf;
        *jpg_out_len = (size_t)out_len;
        return true;
    }

    free(outbuf);
    return true;
}

bool image_to_jpeg(uint8_t* src, size_t src_len, uint16_t width, uint16_t height, v4l2_pix_fmt_t format,
                   uint8_t quality, uint8_t** out, size_t* out_len) {
#if CONFIG_XIAOZHI_ENABLE_HARDWARE_JPEG_ENCODER
    if (encode_with_hw_jpeg(src, src_len, width, height, format, quality, out, out_len, NULL, NULL)) {
        return true;
    }
    // Fallback to esp_new_jpeg
#endif
    return encode_with_esp_new_jpeg(src, src_len, width, height, format, quality, out, out_len, NULL, NULL);
}

bool image_to_jpeg_cb(uint8_t* src, size_t src_len, uint16_t width, uint16_t height, v4l2_pix_fmt_t format,
                      uint8_t quality, jpg_out_cb cb, void* arg) {
#if CONFIG_XIAOZHI_ENABLE_HARDWARE_JPEG_ENCODER
    if (encode_with_hw_jpeg(src, src_len, width, height, format, quality, NULL, NULL, cb, arg)) {
        return true;
    }
    // Fallback to esp_new_jpeg
#endif
    return encode_with_esp_new_jpeg(src, src_len, width, height, format, quality, NULL, NULL, cb, arg);
}
