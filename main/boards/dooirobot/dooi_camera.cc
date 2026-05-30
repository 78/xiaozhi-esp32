// ============================================================================
// dooi_camera.cpp 
// ============================================================================
#include "sdkconfig.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <unistd.h>
#include <errno.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <math.h>
#include <thread>
#include <vector>
#include <stdexcept>
#include <algorithm>

#include "esp_heap_caps.h"
#include "esp_imgfx_color_convert.h"
#include "esp_video_device.h"
#include "esp_video_init.h"
#include "linux/videodev2.h"

#include "board.h"
#include "display.h"
#include "dooi_camera.h"
#include "esp_jpeg_common.h"
#include "jpg/image_to_jpeg.h"
#include "jpg/jpeg_to_image.h"
#include "lvgl_display.h"
#include "mcp_server.h"
#include "system_info.h"
#include "driver/ppa.h"
#include "app_ui.h"
#include "app_ui_logic.h"
#include "driver/i2c.h"

#define TAG "DooiCamera"

// ===== 全局变量（保持原有结构）=====
static FrameBuffer frame_;
static FrameBuffer frame_web_;
static FrameBuffer frame_display_;
static FrameBuffer frame_face_;
static FrameBuffer frame_gesture_;
static FrameBuffer frame_cls_;
static FrameBuffer frame_server_;

static v4l2_pix_fmt_t      sensor_format_   = 0;
static int                 video_fd_        = -1;
static bool                streaming_on_    = false;
static std::vector<MmapBuffer> mmap_buffers_;
static ppa_client_handle_t s_ppa_client = nullptr;

// ===== 常量定义 =====
static constexpr uint16_t LCD_WIDTH      = 200;
static constexpr uint16_t LCD_HEIGHT     = 200;
static constexpr uint16_t WEB_WIDTH      = 400;
static constexpr uint16_t WEB_HEIGHT     = 400;
static constexpr uint16_t FACE_WIDTH     = 200;  // 需求120
static constexpr uint16_t FACE_HEIGHT    = 200;  // 需求160
static constexpr uint16_t GESTURE_WIDTH  = 200;  // 需求128
static constexpr uint16_t GESTURE_HEIGHT = 200;  // 需求128
static constexpr uint16_t CLS_WIDTH      = 400;  // 需求224
static constexpr uint16_t CLS_HEIGHT     = 400;  // 需求224
static constexpr uint16_t SERVER_WIDTH   = 400;
static constexpr uint16_t SERVER_HEIGHT  = 400;


#define OV5647_I2C_ADDR 0x36

extern i2c_master_bus_handle_t i2c_bus_;

// 存储 OV5647 的设备句柄
static i2c_master_dev_handle_t s_ov5647_i2c_dev = nullptr;

// 1. 初始化并挂载 OV5647 设备到现有的 I2C 总线上
static bool ensure_ov5647_device() {
    if (s_ov5647_i2c_dev != nullptr) {
        return true;
    }

    // ========== 重点 ==========
    // 这里需要填入你的项目里实际初始化好的 I2C Bus 句柄。
    // 如果你在 Board 类里有对应方法，比如: i2c_master_bus_handle_t bus = Board::GetInstance().GetI2cBus();
    i2c_master_bus_handle_t bus = i2c_bus_; 
    if (bus == nullptr) {
        ESP_LOGE(TAG, "I2C Bus handle is NULL! Cannot configure camera.");
        return false;
    }

    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address  = OV5647_I2C_ADDR;
    dev_cfg.scl_speed_hz    = 100000; // 100KHz

    esp_err_t err = i2c_master_bus_add_device(bus, &dev_cfg, &s_ov5647_i2c_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add OV5647 device to I2C bus: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

// 2. 新版写寄存器函数
static esp_err_t write_ov5647_reg(uint16_t reg, uint8_t data) {
    if (!ensure_ov5647_device()) return ESP_FAIL;
    uint8_t buf[3] = { (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF), data };
    return i2c_master_transmit(s_ov5647_i2c_dev, buf, sizeof(buf), -1);
}

// 3. 新版读寄存器函数
static esp_err_t read_ov5647_reg(uint16_t reg, uint8_t *data) {
    if (!ensure_ov5647_device()) return ESP_FAIL;
    uint8_t reg_buf[2] = { (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF) };
    return i2c_master_transmit_receive(s_ov5647_i2c_dev, reg_buf, sizeof(reg_buf), data, 1, -1);
}

// 4. 16位读写辅助
static uint16_t read_ov5647_reg16(uint16_t reg) {
    uint8_t h = 0, l = 0;
    read_ov5647_reg(reg, &h);
    read_ov5647_reg(reg + 1, &l);
    return (h << 8) | l;
}

static void write_ov5647_reg16(uint16_t reg, uint16_t data) {
    write_ov5647_reg(reg, (data >> 8) & 0xFF);
    write_ov5647_reg(reg + 1, data & 0xFF);
}

// ==========================================
// 核心：光学窗口平移函数 (带读回校验)
// ==========================================
static void ov5647_shift_optical_center(int offset_x, int offset_y) {
    ESP_LOGI(TAG, "=> Shifting OV5647 optical center via new I2C driver...");
    if (!ensure_ov5647_device()) return;

    // 1. 读取原值
    uint16_t x_start = read_ov5647_reg16(0x3800);
    uint16_t x_end   = read_ov5647_reg16(0x3804);
    uint16_t y_start = read_ov5647_reg16(0x3802);
    uint16_t y_end   = read_ov5647_reg16(0x3806);

    ESP_LOGI(TAG, "[Before] Window - X:[%d -> %d], Y:[%d -> %d]", x_start, x_end, y_start, y_end);

    // 2. 写入新值
    if (offset_x != 0) {
        int new_x_start = x_start + offset_x;
        int new_x_end   = x_end + offset_x;
        
        // 只要 start 不小于 0，end 不超过 OV5647 的绝对极限 2624 即可
        if (new_x_start >= 0 && new_x_end <= 2624) {
            // 对齐到 4 的倍数
            new_x_start = (new_x_start / 16) * 16;
            new_x_end = (new_x_end / 16) * 16;
            write_ov5647_reg16(0x3800, new_x_start);
            write_ov5647_reg16(0x3804, new_x_end);
        } else {
            ESP_LOGE(TAG, "Offset out of physical bounds! start:%d, end:%d", new_x_start, new_x_end);
        }
    }

    if (offset_y != 0) {
        int new_y_start = y_start + offset_y;
        int new_y_end   = y_end + offset_y;
        if (new_y_start >= 0 && new_y_end <= 1944) {
            new_y_start = (new_y_start / 16) * 16;
            new_y_end = (new_y_end / 16) * 16;
            write_ov5647_reg16(0x3802, new_y_start);
            write_ov5647_reg16(0x3806, new_y_end);
        }
    }

    // 3. 延时让 Sensor 消化一下
    vTaskDelay(pdMS_TO_TICKS(5));

    // 4. 读回校验（核心排错步骤）
    uint16_t chk_x_start = read_ov5647_reg16(0x3800);
    uint16_t chk_x_end   = read_ov5647_reg16(0x3804);
    ESP_LOGI(TAG, "[After ] Window - X:[%d -> %d], Target was:[%d -> %d]", 
             chk_x_start, chk_x_end, x_start + offset_x, x_end + offset_x);

    if (chk_x_start == x_start) {
        ESP_LOGE(TAG, "Shift failed! Registers were overwritten by driver or I2C write failed.");
    } else {
        ESP_LOGI(TAG, "Shift success! Hardware registers updated.");
    }
}

static bool ov5647_set_hmirror(bool enabled)
{
    if (!ensure_ov5647_device()) {
        return false;
    }

    uint8_t reg = 0;
    if (read_ov5647_reg(0x3821, &reg) != ESP_OK) {
        ESP_LOGE(TAG, "read 0x3821 failed");
        return false;
    }

    uint8_t new_reg = reg;

    if (enabled) {
        new_reg &= ~0x02;   // sensor mirror semantic
        new_reg |=  0x04;   // isp mirror sync
    } else {
        new_reg |=  0x02;
        new_reg &= ~0x04;
    }

    if (write_ov5647_reg(0x3821, new_reg) != ESP_OK) {
        ESP_LOGE(TAG, "write 0x3821 failed");
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(5));

    uint8_t chk = 0;
    read_ov5647_reg(0x3821, &chk);
    ESP_LOGI(TAG, "HMirror sync: 0x3821 0x%02X -> 0x%02X (chk=0x%02X)",
             reg, new_reg, chk);

    return chk == new_reg;
}

void ov5647_enable_auto_exposure()
{
    uint8_t reg;
    read_ov5647_reg(0x3503, &reg);
    reg &= ~0x03;
    write_ov5647_reg(0x3503, reg);

    // 提高亮度目标
    write_ov5647_reg(0x3A0F, 0x50);
    write_ov5647_reg(0x3A10, 0x48);
}

void ov5647_smart_auto_mode()
{
    // 开 AE
    write_ov5647_reg(0x3503, 0x00);

    // 目标亮度（更亮一点）
    write_ov5647_reg(0x3A0F, 0x50);
    write_ov5647_reg(0x3A10, 0x48);

    // 夜间增强
    write_ov5647_reg(0x3A02, 0x07);
    write_ov5647_reg(0x3A03, 0xFF);

    // 增益上限
    write_ov5647_reg(0x3A19, 0xF8);

    // 降噪
    write_ov5647_reg(0x5306, 0x20);
    write_ov5647_reg(0x5307, 0x40);

    // 降锐化
    write_ov5647_reg(0x5300, 0x08);
}

// ===== 辅助函数：PPA初始化 =====
static bool ensure_ppa_client()
{
    if (s_ppa_client) {
        return true;
    }

    ppa_client_config_t cfg = {};
    cfg.oper_type             = PPA_OPERATION_SRM;
    cfg.max_pending_trans_num = 3;
    cfg.data_burst_length     = PPA_DATA_BURST_LENGTH_128;

    esp_err_t err = ppa_register_client(&cfg, &s_ppa_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ppa_register_client failed: %s", esp_err_to_name(err));
        s_ppa_client = nullptr;
        return false;
    }
    ESP_LOGI(TAG, "PPA SRM client initialized");
    return true;
}

// ===== 辅助函数：V4L2格式转PPA格式 =====
static bool v4l2_to_ppa_color(v4l2_pix_fmt_t v4l2_fmt, ppa_srm_color_mode_t *out_cm)
{
    switch (v4l2_fmt) {
        case V4L2_PIX_FMT_YUV420:
            *out_cm = PPA_SRM_COLOR_MODE_YUV420;
            return true;
        case V4L2_PIX_FMT_RGB24:
            *out_cm = PPA_SRM_COLOR_MODE_RGB888;
            return true;
        case V4L2_PIX_FMT_RGB565:
            *out_cm = PPA_SRM_COLOR_MODE_RGB565;
            return true;
        case V4L2_PIX_FMT_YUYV:
            *out_cm = PPA_SRM_COLOR_MODE_YUV422_YUYV;
            return true;
        default:
            ESP_LOGE(TAG, "v4l2_to_ppa_color: unsupported v4l2 fmt=0x%08lx", v4l2_fmt);
            return false;
    }
}

// ===== 核心函数：统一的PPA缩放转换 =====
static bool ppa_scale_convert(
    const uint8_t *src,
    uint16_t       src_w,
    uint16_t       src_h,
    uint8_t       *dst,
    uint16_t       dst_w,
    uint16_t       dst_h,
    bool           swap_color,
    ppa_srm_color_mode_t in_color,
    ppa_srm_color_mode_t out_color)
{
    if (!src || !dst || src_w == 0 || src_h == 0 || dst_w == 0 || dst_h == 0) {
        ESP_LOGE(TAG, "ppa_scale_convert: invalid argument");
        return false;
    }

    if (!ensure_ppa_client()) {
        return false;
    }

    size_t out_size = 0;
    if (out_color == PPA_SRM_COLOR_MODE_RGB565) {
        out_size = (size_t)dst_w * dst_h * 2;
    } else if (out_color == PPA_SRM_COLOR_MODE_RGB888) {
        out_size = (size_t)dst_w * dst_h * 3;
    } else if (out_color == PPA_SRM_COLOR_MODE_YUV420) {
        out_size = (size_t)dst_w * dst_h * 3 / 2;
    } else if(out_color == PPA_SRM_COLOR_MODE_YUV422_YUYV){
        out_size = (size_t)dst_w * dst_h * 2;
    } else {
        ESP_LOGE(TAG, "ppa_scale_convert: unsupported out color mode=%d", (int)out_color);
        return false;
    }

    std::memset(dst, 0, out_size);

    ppa_srm_oper_config_t cfg = {};
    cfg.in.buffer         = src;
    cfg.in.pic_w          = src_w;
    cfg.in.pic_h          = src_h;
    cfg.in.block_w        = src_w;
    cfg.in.block_h        = src_h;
    cfg.in.block_offset_x = 0;
    cfg.in.block_offset_y = 0;
    cfg.in.srm_cm         = in_color;
    cfg.in.yuv_range     = (ppa_color_range_t)0;
    cfg.in.yuv_std       = (ppa_color_conv_std_rgb_yuv_t)0;

    cfg.out.buffer        = dst;
    cfg.out.buffer_size   = out_size;
    cfg.out.pic_w         = dst_w;
    cfg.out.pic_h         = dst_h;
    cfg.out.block_offset_x = 0;
    cfg.out.block_offset_y = 0;
    cfg.out.srm_cm        = out_color;
    cfg.out.yuv_range     = (ppa_color_range_t)0;
    cfg.out.yuv_std       = (ppa_color_conv_std_rgb_yuv_t)0;

    cfg.rotation_angle = PPA_SRM_ROTATION_ANGLE_0;
    cfg.scale_x        = (float)dst_w / (float)src_w;
    cfg.scale_y        = (float)dst_h / (float)src_h;
    cfg.mirror_x       = false;
    cfg.mirror_y       = false;
    cfg.rgb_swap       = swap_color;
    cfg.byte_swap      = false;

    cfg.mode      = PPA_TRANS_MODE_BLOCKING;
    cfg.user_data = nullptr;

    // float sx_req = (float)dst_w / src_w;
    // float sy_req = (float)dst_h / src_h;
    // float step   = 1.0f / 16.0f;

    // // PPA 会往下截断到 1/16
    // float sx_hw = floorf(sx_req / step) * step;
    // float sy_hw = floorf(sy_req / step) * step;

    // uint16_t out_w_hw = (uint16_t)(src_w * sx_hw + 0.5f);
    // uint16_t out_h_hw = (uint16_t)(src_h * sy_hw + 0.5f);

    // ESP_LOGI(TAG, "PPA scale req=(%.4f, %.4f) -> hw=(%.4f, %.4f), out=%ux%u (dst=%ux%u)",
    //         sx_req, sy_req, sx_hw, sy_hw,
    //         out_w_hw, out_h_hw, dst_w, dst_h);

    esp_err_t err = ppa_do_scale_rotate_mirror(s_ppa_client, &cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ppa_do_scale_rotate_mirror failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

// ===== 新增：统一的帧处理函数（消除重复代码）=====
struct FrameProcessConfig {
    uint16_t target_width;
    uint16_t target_height;
    v4l2_pix_fmt_t target_format;
    bool swap_color;
    const char* name;
};

static bool process_frame_to_buffer(
    const FrameProcessConfig& config,
    FrameBuffer* output,
    uint8_t** static_buffer,
    bool* initialized)
{
    auto camera = Board::GetInstance().GetCamera();
    if (!camera) {
        ESP_LOGE(TAG, "%s: camera not initialized", config.name);
        return false;
    }

    if (!camera->Capture()) {
        ESP_LOGE(TAG, "%s: capture failed", config.name);
        return false;
    }

    if (!frame_.data) {
        ESP_LOGE(TAG, "%s: empty frame", config.name);
        return false;
    }

    // 初始化静态缓冲
    if (!*initialized) {
        size_t size = 0;
        if (config.target_format == V4L2_PIX_FMT_RGB24) {
            size = (size_t)config.target_width * config.target_height * 3;
        } else if (config.target_format == V4L2_PIX_FMT_RGB565) {
            size = (size_t)config.target_width * config.target_height * 2;
        } else if (config.target_format == V4L2_PIX_FMT_YUV420) {
            size = (size_t)config.target_width * config.target_height * 3 / 2;
        } else {
            ESP_LOGE(TAG, "%s: unsupported format", config.name);
            return false;
        }

        *static_buffer = static_cast<uint8_t*>(
            heap_caps_aligned_alloc(64, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        
        if (!*static_buffer) {
            ESP_LOGE(TAG, "%s: buffer alloc failed", config.name);
            return false;
        }

        output->data = *static_buffer;
        output->len = size;
        output->width = config.target_width;
        output->height = config.target_height;
        output->format = config.target_format;
        *initialized = true;
    }

    // PPA转换
    ppa_srm_color_mode_t in_cm, out_cm;
    if (!v4l2_to_ppa_color(frame_.format, &in_cm)) {
        return false;
    }
    if (!v4l2_to_ppa_color(config.target_format, &out_cm)) {
        return false;
    }

    bool ok = ppa_scale_convert(
        frame_.data, frame_.width, frame_.height,
        output->data, config.target_width, config.target_height,
        config.swap_color, in_cm, out_cm);

    if (!ok) {
        ESP_LOGE(TAG, "%s: PPA conversion failed", config.name);
    }

    return ok;
}

// ===== 新增：统一的JPEG编码函数 =====
static bool encode_to_jpeg(
    uint8_t* src_data,  // 改为非const
    size_t src_len,
    uint16_t width,
    uint16_t height,
    v4l2_pix_fmt_t format,
    uint8_t* jpeg_buffer,
    size_t jpeg_cap,
    size_t* out_len)
{
    struct JpegCtx {
        uint8_t *buf;
        size_t   cap;
        size_t   len;
        bool     ok;
    } ctx = { jpeg_buffer, jpeg_cap, 0, false };

    bool enc_ok = image_to_jpeg_cb(
        src_data, src_len, width, height, format, 80,
        [](void *arg, size_t index, const void *data, size_t len) -> size_t {
            auto *c = static_cast<JpegCtx *>(arg);
            if (index == 0 && data && len > 0 && len <= c->cap) {
                std::memcpy(c->buf, data, len);
                c->len = len;
                c->ok  = true;
            }
            return len;
        },
        &ctx);

    if (enc_ok && ctx.ok) {
        *out_len = ctx.len;
        return true;
    }
    return false;
}

// ===== DooiCamera 实现 =====
DooiCamera::DooiCamera(const esp_video_init_config_t &config)
{
    if (esp_video_init(&config) != ESP_OK) {
        ESP_LOGE(TAG, "esp_video_init failed");
        return;
    }

    const char *video_device_name = nullptr;
    if (config.csi != nullptr) {
        video_device_name = ESP_VIDEO_MIPI_CSI_DEVICE_NAME;
    }
    if (video_device_name == nullptr) {
        ESP_LOGE(TAG, "no video device is enabled");
        return;
    }

    video_fd_ = open(video_device_name, O_RDWR);
    if (video_fd_ < 0) {
        ESP_LOGE(TAG, "open %s failed, errno=%d(%s)",
                 video_device_name, errno, strerror(errno));
        return;
    }

    struct v4l2_capability cap = {};
    if (ioctl(video_fd_, VIDIOC_QUERYCAP, &cap) != 0) {
        ESP_LOGE(TAG, "VIDIOC_QUERYCAP failed, errno=%d(%s)", errno, strerror(errno));
        close(video_fd_);
        video_fd_ = -1;
        return;
    }

    ESP_LOGD(TAG, "VIDIOC_QUERYCAP: driver=%s, card=%s", cap.driver, cap.card);

    struct v4l2_format format = {};
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(video_fd_, VIDIOC_G_FMT, &format) != 0) {
        ESP_LOGE(TAG, "VIDIOC_G_FMT failed");
        close(video_fd_);
        video_fd_ = -1;
        return;
    }

    struct v4l2_format setformat = {};
    setformat.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    setformat.fmt.pix.width = format.fmt.pix.width;
    setformat.fmt.pix.height = format.fmt.pix.height;

    // 选择最佳格式
    struct v4l2_fmtdesc fmtdesc = {};
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmtdesc.index = 0;
    uint32_t best_fmt = 0;
    int best_rank = 1 << 30;

    auto get_rank = [](uint32_t fmt) -> int {
        switch (fmt) {
            case V4L2_PIX_FMT_RGB24: return 2;
            case V4L2_PIX_FMT_RGB565: return 1;
            case V4L2_PIX_FMT_YUV420: return 3;
            case V4L2_PIX_FMT_YUV422P: return 4;
            case V4L2_PIX_FMT_JPEG: return 5;
            case V4L2_PIX_FMT_GREY: return 20;
            default: return 1 << 29;
        }
    };

    while (ioctl(video_fd_, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
        int rank = get_rank(fmtdesc.pixelformat);
        if (rank < best_rank) {
            best_rank = rank;
            best_fmt = fmtdesc.pixelformat;
        }
        fmtdesc.index++;
    }

    if (best_rank < (1 << 29)) {
        setformat.fmt.pix.pixelformat = best_fmt;
        sensor_format_ = best_fmt;
    }

    if (!setformat.fmt.pix.pixelformat) {
        ESP_LOGE(TAG, "no supported pixel format found");
        close(video_fd_);
        video_fd_ = -1;
        sensor_format_ = 0;
        return;
    }

    if (ioctl(video_fd_, VIDIOC_S_FMT, &setformat) != 0) {
        ESP_LOGE(TAG, "VIDIOC_S_FMT failed");
        close(video_fd_);
        video_fd_ = -1;
        sensor_format_ = 0;
        return;
    }

    frame_.width = setformat.fmt.pix.width;
    frame_.height = setformat.fmt.pix.height;

    // 申请V4L2缓冲
    struct v4l2_requestbuffers req = {};
    req.count = strcmp(video_device_name, ESP_VIDEO_MIPI_CSI_DEVICE_NAME) == 0 ? 2 : 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(video_fd_, VIDIOC_REQBUFS, &req) != 0) {
        ESP_LOGE(TAG, "VIDIOC_REQBUFS failed");
        close(video_fd_);
        video_fd_ = -1;
        sensor_format_ = 0;
        return;
    }

    mmap_buffers_.resize(req.count);
    for (uint32_t i = 0; i < req.count; ++i) {
        struct v4l2_buffer buf = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        
        if (ioctl(video_fd_, VIDIOC_QUERYBUF, &buf) != 0) {
            ESP_LOGE(TAG, "VIDIOC_QUERYBUF failed");
            close(video_fd_);
            video_fd_ = -1;
            sensor_format_ = 0;
            return;
        }
        
        void *start = mmap(nullptr, buf.length, PROT_READ | PROT_WRITE,
                          MAP_SHARED, video_fd_, buf.m.offset);
        if (start == MAP_FAILED) {
            ESP_LOGE(TAG, "mmap failed");
            close(video_fd_);
            video_fd_ = -1;
            sensor_format_ = 0;
            return;
        }
        
        mmap_buffers_[i].start = start;
        mmap_buffers_[i].length = buf.length;

        if (ioctl(video_fd_, VIDIOC_QBUF, &buf) != 0) {
            ESP_LOGE(TAG, "VIDIOC_QBUF failed");
            close(video_fd_);
            video_fd_ = -1;
            sensor_format_ = 0;
            return;
        }
    }

    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(video_fd_, VIDIOC_STREAMON, &type) != 0) {
        ESP_LOGE(TAG, "VIDIOC_STREAMON failed");
        close(video_fd_);
        video_fd_ = -1;
        sensor_format_ = 0;
        return;
    }
    
    ov5647_shift_optical_center(-480, 0); 
    ov5647_set_hmirror(true);
    // ov5647_enable_auto_exposure();
    // ov5647_smart_auto_mode();


    // ISP预热
    xTaskCreate(
        [](void *arg) {
            (void)arg;
            uint16_t capture_count = 0;
            TickType_t start = xTaskGetTickCount();
            TickType_t duration = 50 / portTICK_PERIOD_MS;
            
            while ((xTaskGetTickCount() - start) < duration) {
                struct v4l2_buffer buf = {};
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_MMAP;
                
                if (ioctl(video_fd_, VIDIOC_DQBUF, &buf) != 0) {
                    vTaskDelay(10 / portTICK_PERIOD_MS);
                    continue;
                }
                if (ioctl(video_fd_, VIDIOC_QBUF, &buf) != 0) {
                    ESP_LOGE(TAG, "VIDIOC_QBUF failed during init");
                }
                capture_count++;
            }
            
            ESP_LOGI(TAG, "Camera init success, captured %d frames in 5s", capture_count);
            streaming_on_ = true;
            vTaskDelete(nullptr);
        },
        "CameraInitTask", 4096, this, 5, nullptr);

    (void)ensure_ppa_client();
}

DooiCamera::~DooiCamera()
{
    if (streaming_on_ && video_fd_ >= 0) {
        int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(video_fd_, VIDIOC_STREAMOFF, &type);
    }

    for (auto &b : mmap_buffers_) {
        if (b.start && b.length) {
            munmap(b.start, b.length);
        }
    }

    if (video_fd_ >= 0) {
        close(video_fd_);
        video_fd_ = -1;
    }

    sensor_format_ = 0;
    esp_video_deinit();

    if (s_ppa_client) {
        ppa_unregister_client(s_ppa_client);
        s_ppa_client = nullptr;
    }
}

bool DooiCamera::Capture()
{
    if (!streaming_on_ || video_fd_ < 0) {
        return false;
    }

    struct v4l2_buffer buf = {};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    
    if (ioctl(video_fd_, VIDIOC_DQBUF, &buf) != 0) {
        ESP_LOGE(TAG, "VIDIOC_DQBUF failed");
        return false;
    }

    frame_.len = buf.bytesused;
    if (!frame_.data) {
        frame_.data = (uint8_t *)heap_caps_aligned_alloc(64, frame_.len,
                                                         MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }

    switch (sensor_format_) {
        case V4L2_PIX_FMT_RGB24:
        case V4L2_PIX_FMT_RGB565:
        case V4L2_PIX_FMT_YUYV:
        case V4L2_PIX_FMT_YUV420:
        case V4L2_PIX_FMT_GREY:
        case V4L2_PIX_FMT_JPEG:
            std::memcpy(frame_.data, mmap_buffers_[buf.index].start,
                       MIN(mmap_buffers_[buf.index].length, frame_.len));
            frame_.format = sensor_format_;
            break;
        case V4L2_PIX_FMT_YUV422P:
            frame_.format = V4L2_PIX_FMT_YUYV;
            std::memcpy(frame_.data, mmap_buffers_[buf.index].start,
                       MIN(mmap_buffers_[buf.index].length, frame_.len));
            break;
        default:
            ESP_LOGE(TAG, "unsupported sensor format: 0x%08lx", sensor_format_);
            ioctl(video_fd_, VIDIOC_QBUF, &buf);
            return false;
    }

    if (ioctl(video_fd_, VIDIOC_QBUF, &buf) != 0) {
        ESP_LOGE(TAG, "VIDIOC_QBUF failed");
    }

    return true;
}

bool DooiCamera::SetHMirror(bool enabled)
{
    if (video_fd_ < 0) return false;
    
    struct v4l2_ext_controls ctrls = {};
    struct v4l2_ext_control ctrl = {};
    ctrl.id = V4L2_CID_HFLIP;
    ctrl.value = enabled ? 1 : 0;
    ctrls.ctrl_class = V4L2_CTRL_CLASS_USER;
    ctrls.count = 1;
    ctrls.controls = &ctrl;
    
    return ioctl(video_fd_, VIDIOC_S_EXT_CTRLS, &ctrls) == 0;
}

bool DooiCamera::SetVFlip(bool enabled)
{
    if (video_fd_ < 0) return false;
    
    struct v4l2_ext_controls ctrls = {};
    struct v4l2_ext_control ctrl = {};
    ctrl.id = V4L2_CID_VFLIP;
    ctrl.value = enabled ? 1 : 0;
    ctrls.ctrl_class = V4L2_CTRL_CLASS_USER;
    ctrls.count = 1;
    ctrls.controls = &ctrl;
    
    return ioctl(video_fd_, VIDIOC_S_EXT_CTRLS, &ctrls) == 0;
}

void DooiCamera::SetExplainUrl(const std::string &url, const std::string &token)
{
    explain_url_ = url;
    explain_token_ = token;
}

std::string DooiCamera::Explain(const std::string &question)
{
    if (explain_url_.empty()) {
        throw std::runtime_error("Image explain URL or token is not set");
    }

    if (!frame_.data || frame_.len == 0) {
        throw std::runtime_error("No frame captured for Explain()");
    }

    // 准备server用的JPEG（320x240）
    static uint8_t *s_down = nullptr;
    static uint8_t *s_jpeg = nullptr;
    static bool s_inited = false;

    constexpr uint16_t dst_w = SERVER_WIDTH;
    constexpr uint16_t dst_h = SERVER_HEIGHT;
    constexpr size_t jpeg_cap = 80 * 1024;

    size_t down_len = 0;
    if (frame_.format == V4L2_PIX_FMT_RGB24) {
        down_len = (size_t)dst_w * dst_h * 3;
    } else if (frame_.format == V4L2_PIX_FMT_RGB565) {
        down_len = (size_t)dst_w * dst_h * 2;
    } else if (frame_.format == V4L2_PIX_FMT_YUV420) {
        down_len = (size_t)dst_w * dst_h * 3 / 2;
    } else if (frame_.format == V4L2_PIX_FMT_YUYV) {
        down_len = (size_t)dst_w * dst_h * 2;
    } else {
        throw std::runtime_error("Unsupported frame format for Explain");
    }

    if (!s_inited) {
        s_down = static_cast<uint8_t *>(
            heap_caps_aligned_alloc(64, down_len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        s_jpeg = static_cast<uint8_t *>(
            heap_caps_aligned_alloc(16, jpeg_cap, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        s_inited = (s_down != nullptr && s_jpeg != nullptr);
        if (!s_inited) {
            throw std::runtime_error("Failed to allocate buffers for Explain");
        }
    }

    ppa_srm_color_mode_t in_cm;
    if (!v4l2_to_ppa_color(frame_.format, &in_cm)) {
        throw std::runtime_error("Unsupported color format for PPA");
    }

    // PPA缩放
    bool ok = ppa_scale_convert(
        frame_.data, frame_.width, frame_.height,
        s_down, dst_w, dst_h,
        false, in_cm, in_cm);
    
    if (!ok) {
        throw std::runtime_error("PPA scale failed for Explain");
    }

    // JPEG编码
    size_t jpeg_len = 0;
    if (!encode_to_jpeg(s_down, down_len, dst_w, dst_h, frame_.format,
                       s_jpeg, jpeg_cap, &jpeg_len)) {
        throw std::runtime_error("JPEG encode failed for Explain");
    }

    frame_server_.data = s_jpeg;
    frame_server_.len = jpeg_len;
    frame_server_.width = dst_w;
    frame_server_.height = dst_h;
    frame_server_.format = V4L2_PIX_FMT_JPEG;

    // HTTP上传
    auto network = Board::GetInstance().GetNetwork();
    auto http = network->CreateHttp(3);
    std::string boundary = "----ESP32_CAMERA_BOUNDARY";

    http->SetHeader("Device-Id", SystemInfo::GetMacAddress().c_str());
    http->SetHeader("Client-Id", Board::GetInstance().GetUuid().c_str());
    if (!explain_token_.empty()) {
        http->SetHeader("Authorization", "Bearer " + explain_token_);
    }
    http->SetHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
    http->SetHeader("Transfer-Encoding", "chunked");

    if (!http->Open("POST", explain_url_)) {
        throw std::runtime_error("Failed to connect to explain URL");
    }

    // 问题字段
    std::string question_field;
    question_field += "--" + boundary + "\r\n";
    question_field += "Content-Disposition: form-data; name=\"question\"\r\n";
    question_field += "\r\n";
    question_field += question + "\r\n";
    http->Write(question_field.c_str(), question_field.size());

    // JPEG文件字段
    std::string file_header;
    file_header += "--" + boundary + "\r\n";
    file_header += "Content-Disposition: form-data; name=\"file\"; filename=\"camera.jpg\"\r\n";
    file_header += "Content-Type: image/jpeg\r\n";
    file_header += "\r\n";
    http->Write(file_header.c_str(), file_header.size());

    http->Write(reinterpret_cast<const char *>(frame_server_.data), frame_server_.len);

    std::string multipart_footer;
    multipart_footer += "\r\n--" + boundary + "--\r\n";
    http->Write(multipart_footer.c_str(), multipart_footer.size());
    http->Write("", 0);

    if (http->GetStatusCode() != 200) {
        ESP_LOGE(TAG, "Failed to upload photo, status code: %d", http->GetStatusCode());
        throw std::runtime_error("Failed to upload photo");
    }

    std::string result = http->ReadAll();
    http->Close();

    app_event_t evt = {.type = APP_EVT_CAMERA_TIGGRER_PREVIEW};
    app_ui_logic_post(&evt, false);

    ESP_LOGI(TAG, "Explain: raw=%d bytes, jpeg=%d bytes, question=%s",
             (int)frame_.len, (int)frame_server_.len, question.c_str());    
    
    return result;
}

// ============================================================================
// C接口实现：使用统一的处理函数
// ============================================================================

extern "C" {

FrameBuffer camera_get_frame()
{
    auto camera = Board::GetInstance().GetCamera();
    if (!camera) {
        FrameBuffer fb{};
        return fb;
    }
    camera->Capture();
    return frame_;
}

FrameBuffer camera_get_web_frame()
{
    static uint8_t *s_down = nullptr;
    static uint8_t *s_jpeg = nullptr;
    static bool s_inited = false;

    auto camera = Board::GetInstance().GetCamera();
    if (!camera || !camera->Capture()) {
        FrameBuffer fb{};
        return fb;
    }

    if (!frame_.data || frame_.width == 0 || frame_.height == 0) {
        ESP_LOGE(TAG, "camera_get_web_frame: empty frame");
        FrameBuffer fb{};
        return fb;
    }

    constexpr uint16_t dst_w = WEB_WIDTH;
    constexpr uint16_t dst_h = WEB_HEIGHT;
    constexpr size_t jpeg_cap = 80 * 1024;

    size_t down_len = 0;
    if (frame_.format == V4L2_PIX_FMT_RGB24) {
        down_len = (size_t)dst_w * dst_h * 3;
    } else if (frame_.format == V4L2_PIX_FMT_RGB565) {
        down_len = (size_t)dst_w * dst_h * 2;
    } else if (frame_.format == V4L2_PIX_FMT_YUV420) {
        down_len = (size_t)dst_w * dst_h * 3 / 2;
    } else if (frame_.format == V4L2_PIX_FMT_YUYV) {
        down_len = (size_t)dst_w * dst_h * 2;
    } else {
        ESP_LOGE(TAG, "camera_get_web_frame: unsupported format");
        FrameBuffer fb{};
        return fb;
    }

    if (!s_inited) {
        s_down = static_cast<uint8_t *>(
            heap_caps_aligned_alloc(64, down_len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        s_jpeg = static_cast<uint8_t *>(
            heap_caps_aligned_alloc(16, jpeg_cap, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        s_inited = (s_down != nullptr && s_jpeg != nullptr);
        if (!s_inited) {
            ESP_LOGE(TAG, "camera_get_web_frame: buffer alloc failed");
            FrameBuffer fb{};
            return fb;
        }
    }

    ppa_srm_color_mode_t in_cm;
    if (!v4l2_to_ppa_color(frame_.format, &in_cm)) {
        FrameBuffer fb{};
        return fb;
    }

    bool ok = ppa_scale_convert(
        frame_.data, frame_.width, frame_.height,
        s_down, dst_w, dst_h,
        false, in_cm, in_cm);
    
    if (!ok) {
        ESP_LOGE(TAG, "camera_get_web_frame: PPA scale failed");
        FrameBuffer fb{};
        return fb;
    }

    size_t jpeg_len = 0;
    if (!encode_to_jpeg(s_down, down_len, dst_w, dst_h, frame_.format,
                       s_jpeg, jpeg_cap, &jpeg_len)) {
        ESP_LOGE(TAG, "camera_get_web_frame: JPEG encode failed");
        FrameBuffer fb{};
        return fb;
    }

    frame_web_.data = s_jpeg;
    frame_web_.len = jpeg_len;
    frame_web_.width = dst_w;
    frame_web_.height = dst_h;
    frame_web_.format = V4L2_PIX_FMT_JPEG;

    return frame_web_;
}

FrameBuffer camera_get_display_frame()
{
    static uint8_t *s_buffer = nullptr;
    static bool s_inited = false;

    FrameProcessConfig config = {
        LCD_WIDTH,
        LCD_HEIGHT,
        V4L2_PIX_FMT_RGB565,
        false,
        "display"
    };

    if (process_frame_to_buffer(config, &frame_display_, &s_buffer, &s_inited)) {
        return frame_display_;
    }

    FrameBuffer fb{};
    return fb;
}

FrameBuffer camera_get_face_frame()
{
    static uint8_t *s_buffer = nullptr;
    static bool s_inited = false;

    FrameProcessConfig config = {
        FACE_WIDTH,
        FACE_HEIGHT,
        V4L2_PIX_FMT_RGB24,
        true,
        "face"
    };

    if (process_frame_to_buffer(config, &frame_face_, &s_buffer, &s_inited)) {
        return frame_face_;
    }

    FrameBuffer fb{};
    return fb;
}

FrameBuffer camera_get_gesture_frame()
{
    static uint8_t *s_buffer = nullptr;
    static bool s_inited = false;

    FrameProcessConfig config = {
        GESTURE_WIDTH,
        GESTURE_HEIGHT,
        V4L2_PIX_FMT_RGB24,
        true,
        "gesture"
    };

    if (process_frame_to_buffer(config, &frame_gesture_, &s_buffer, &s_inited)) {
        return frame_gesture_;
    }

    FrameBuffer fb{};
    return fb;
}

FrameBuffer camera_get_cls_frame()
{
    static uint8_t *s_buffer = nullptr;
    static bool s_inited = false;

    FrameProcessConfig config = {
        GESTURE_WIDTH,
        GESTURE_HEIGHT,
        V4L2_PIX_FMT_RGB24,
        true,
        "cls"
    };

    if (process_frame_to_buffer(config, &frame_cls_, &s_buffer, &s_inited)) {
        return frame_cls_;
    }

    FrameBuffer fb{};
    return fb;
}

} // extern "C"