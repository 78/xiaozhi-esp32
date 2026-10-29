#pragma once

#include <esp_heap_caps.h>
#include <esp_lcd_panel_interface.h>
#include <esp_lcd_panel_ops.h>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace cyberai {

// The shared EmoteDisplay waits for the real panel's completion callback before
// submitting another region. Keep one persistent DMA buffer alive for that transfer.
// The target panel and its IO/callbacks remain owned by the board.
class DmaPanel : public esp_lcd_panel_t {
public:
    DmaPanel(esp_lcd_panel_handle_t target, size_t capacity)
        : esp_lcd_panel_t{}, target_(target), capacity_(capacity) {
        draw_bitmap = DrawBitmap;
    }

    ~DmaPanel() { heap_caps_free(buffer_); }
    DmaPanel(const DmaPanel&) = delete;
    DmaPanel& operator=(const DmaPanel&) = delete;

    esp_err_t Initialize() {
        if (!target_ || !target_->draw_bitmap || capacity_ == 0) {
            return ESP_ERR_INVALID_ARG;
        }
        if (!buffer_) {
            buffer_ = heap_caps_malloc(capacity_, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
        }
        return buffer_ ? ESP_OK : ESP_ERR_NO_MEM;
    }

private:
    static esp_err_t DrawBitmap(esp_lcd_panel_t* panel, int x_start, int y_start, int x_end,
                                int y_end, const void* pixels) {
        auto* self = static_cast<DmaPanel*>(panel);
        if (!pixels || x_start < 0 || y_start < 0 || x_end <= x_start || y_end <= y_start) {
            return ESP_ERR_INVALID_ARG;
        }
        if (!self->buffer_) {
            return ESP_ERR_INVALID_STATE;
        }
        const size_t width = static_cast<size_t>(x_end) - x_start;
        const size_t height = static_cast<size_t>(y_end) - y_start;
        if (width > self->capacity_ / sizeof(uint16_t) / height) {
            return ESP_ERR_INVALID_SIZE;
        }
        const size_t bytes = width * height * sizeof(uint16_t);
        std::memcpy(self->buffer_, pixels, bytes);
        return esp_lcd_panel_draw_bitmap(self->target_, x_start, y_start, x_end, y_end,
                                         self->buffer_);
    }

    esp_lcd_panel_handle_t target_;
    size_t capacity_;
    void* buffer_ = nullptr;
};

}  // namespace cyberai
