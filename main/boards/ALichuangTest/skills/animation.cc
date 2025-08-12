#include "animation.h"

#include <esp_log.h>
#include <esp_err.h>
#include <esp_lvgl_port.h>
#include <esp_heap_caps.h>
#include <cstring>

#include "board.h"

#define TAG "AnimaDisplay"

AnimaDisplay::AnimaDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                           int width, int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y, bool swap_xy,
                           DisplayFonts fonts)
    : LcdDisplay(panel_io, panel, fonts, width, height) {

    // draw white
    std::vector<uint16_t> buffer(width_, 0xFFFF);
    for (int y = 0; y < height_; y++) {
        esp_lcd_panel_draw_bitmap(panel_, 0, y, width_, y + 1, buffer.data());
    }

    // Set the display to on
    ESP_LOGI(TAG, "Turning display on");
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    ESP_LOGI(TAG, "Initialize LVGL port");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 1;
    port_cfg.timer_period_ms = 50;
    lvgl_port_init(&port_cfg);

    ESP_LOGI(TAG, "Adding LCD display");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io_,
        .panel_handle = panel_,
        .control_handle = nullptr,
        .buffer_size = static_cast<uint32_t>(width_ * 20),
        .double_buffer = false,
        .trans_size = 0,
        .hres = static_cast<uint32_t>(width_),
        .vres = static_cast<uint32_t>(height_),
        .monochrome = false,
        .rotation = {
            .swap_xy = swap_xy,
            .mirror_x = mirror_x,
            .mirror_y = mirror_y,
        },
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .buff_dma = 1,
            .buff_spiram = 0,
            .sw_rotate = 0,
            .swap_bytes = 1,
            .full_refresh = 0,
            .direct_mode = 0,
        },
    };

    display_ = lvgl_port_add_disp(&display_cfg);
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add display");
        return;
    }

    if (offset_x != 0 || offset_y != 0) {
        lv_display_set_offset(display_, offset_x, offset_y);
    }

    SetupUI();
}

void AnimaDisplay::SetEmotion(const char* emotion) {
    // 触发情感变化回调
    if (emotion_callback_) {
        emotion_callback_(std::string(emotion));
    }
}

void AnimaDisplay::CreateCanvas() {
    DisplayLockGuard lock(this);
    
    // 如果已经有画布，先销毁
    if (canvas_ != nullptr) {
        DestroyCanvas();
    }
    
    // 创建画布所需的缓冲区
    // 每个像素2字节(RGB565)
    size_t buf_size = width_ * height_ * 2;  // RGB565: 2 bytes per pixel
    
    // 分配内存，优先使用PSRAM
    canvas_buffer_ = heap_caps_malloc(buf_size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    if (canvas_buffer_ == nullptr) {
        ESP_LOGE("Display", "Failed to allocate canvas buffer");
        return;
    }
    
    // 获取活动屏幕
    lv_obj_t* screen = lv_screen_active();
    
    // 创建画布对象
    canvas_ = lv_canvas_create(screen);
    if (canvas_ == nullptr) {
        ESP_LOGE("Display", "Failed to create canvas");
        heap_caps_free(canvas_buffer_);
        canvas_buffer_ = nullptr;
        return;
    }
    
    // 初始化画布
    lv_canvas_set_buffer(canvas_, canvas_buffer_, width_, height_, LV_COLOR_FORMAT_RGB565);
    
    // 设置画布位置为全屏
    // lv_obj_set_pos(canvas_, 0, 25);
    // lv_obj_set_size(canvas_, width_, height_ - 25);
    lv_obj_set_pos(canvas_, 0, 0);
    lv_obj_set_size(canvas_, width_, height_);
    
    // 设置画布为透明
    lv_canvas_fill_bg(canvas_, lv_color_make(0, 0, 0), LV_OPA_TRANSP);
    
    // 设置画布为顶层
    lv_obj_move_foreground(canvas_);
    
    ESP_LOGI("Display", "Canvas created successfully");
}

void AnimaDisplay::DestroyCanvas() {
    DisplayLockGuard lock(this);
    
    if (canvas_ != nullptr) {
        lv_obj_del(canvas_);
        canvas_ = nullptr;
    }
    
    if (canvas_buffer_ != nullptr) {
        heap_caps_free(canvas_buffer_);
        canvas_buffer_ = nullptr;
    }
    
    ESP_LOGI("Display", "Canvas destroyed");
}

void AnimaDisplay::DrawImageOnCanvas(int x, int y, int width, int height, const uint8_t* img_data) {
    DisplayLockGuard lock(this);
    
    // 确保有画布
    if (canvas_ == nullptr) {
        ESP_LOGE("Display", "Canvas not created");
        return;
    }
    
    // 创建一个描述器来映射图像数据
    const lv_image_dsc_t img_dsc = {
        .header = {
            .magic = LV_IMAGE_HEADER_MAGIC,
            .cf = LV_COLOR_FORMAT_RGB565,
            .flags = 0,
            .w = (uint32_t)width,
            .h = (uint32_t)height,
            .stride = (uint32_t)(width * 2),  // RGB565: 2 bytes per pixel
            .reserved_2 = 0,
        },
        .data_size = (uint32_t)(width * height * 2),  // RGB565: 2 bytes per pixel
        .data = img_data,
        .reserved = NULL
    };
    
    // 使用图层绘制图像到画布上
    lv_layer_t layer;
    lv_canvas_init_layer(canvas_, &layer);
    
    lv_draw_image_dsc_t draw_dsc;
    lv_draw_image_dsc_init(&draw_dsc);
    draw_dsc.src = &img_dsc;
    
    lv_area_t area;
    area.x1 = x;
    area.y1 = y;
    area.x2 = x + width - 1;
    area.y2 = y + height - 1;

    lv_draw_image(&layer, &draw_dsc, &area);
    lv_canvas_finish_layer(canvas_, &layer);
    
    // 确保画布在最上层
    lv_obj_move_foreground(canvas_);
    
    // ESP_LOGI("Display", "Image drawn on canvas at x=%d, y=%d, w=%d, h=%d", x, y, width, height);
}