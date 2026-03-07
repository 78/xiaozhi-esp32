#ifndef _CUSTOMLCDDISPLAY_H_
#define _CUSTOMLCDDISPLAY_H_

#include "display/lcd_display.h"
#include "lvgl_theme.h"

LV_FONT_DECLARE(font_puhui_16_4);
LV_FONT_DECLARE(font_awesome_16_4);

class CustomLcdDisplay : public SpiLcdDisplay {
private:
    lv_timer_t* init_check_timer_ = nullptr;
    bool init_custom_display_ = false;

    bool CreateCustomLcdDisplayImpl() {
        DisplayLockGuard lock(this);
        LvglTheme* lvgl_theme = static_cast<LvglTheme*>(current_theme_);
        if (lvgl_theme == nullptr) {
            ESP_LOGE("CustomLcdDisplay", "lvgl_theme is null");
            return false;
        }
        
        auto text_font = lvgl_theme->text_font()->font();
        auto icon_font = lvgl_theme->icon_font()->font();
        
        if (emoji_box_ == nullptr) {
            ESP_LOGE("CustomLcdDisplay", "emoji_box_ is null, skip align");
            return false;
        }
        if (bottom_bar_ == nullptr) {
            ESP_LOGE("CustomLcdDisplay", "bottom_bar_ is null, skip align");
            return false;
        }

        auto screen = lv_screen_active();
        lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);
        lv_obj_align(emoji_box_, LV_ALIGN_CENTER, 0, -5);

        lv_obj_set_size(bottom_bar_, LV_HOR_RES, text_font->line_height + lvgl_theme->spacing(8)+8);
        lv_obj_align(bottom_bar_, LV_ALIGN_BOTTOM_MID, 0, -5);
        ESP_LOGI("CustomLcdDisplay", "Custom UI initialized successfully");
        return true;
    }

    static void InitCheckTimerCb(lv_timer_t* timer) {
        CustomLcdDisplay* self = static_cast<CustomLcdDisplay*>(lv_timer_get_user_data(timer));
        if (self == nullptr) {
            lv_timer_del(timer); 
            return;
        }

        if (self->IsSetupUICalled() && !self->init_custom_display_) {
            if (self->CreateCustomLcdDisplayImpl()) {
                self->init_custom_display_ = true;
                lv_timer_del(timer);
                self->init_check_timer_ = nullptr;
                ESP_LOGI("CustomLcdDisplay", "Init timer stopped, custom UI ready");
            }
        }
    }

public:
    CustomLcdDisplay(esp_lcd_panel_io_handle_t io_handle, 
                    esp_lcd_panel_handle_t panel_handle,
                    int width,
                    int height,
                    int offset_x,
                    int offset_y,
                    bool mirror_x,
                    bool mirror_y,
                    bool swap_xy) 
        : SpiLcdDisplay(io_handle, panel_handle,
                    width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy) {
        init_check_timer_ = lv_timer_create(InitCheckTimerCb, 50, this);
        if (init_check_timer_ == nullptr) {
            ESP_LOGE("CustomLcdDisplay", "Failed to create init check timer");
        } else {
            ESP_LOGI("CustomLcdDisplay", "Init check timer created (50ms interval)");
        }
    }

    ~CustomLcdDisplay() override {
        if (init_check_timer_ != nullptr) {
            lv_timer_del(init_check_timer_);
            init_check_timer_ = nullptr;
        }
    }

    bool IsSetupUICalled() const {
        return setup_ui_called_;
    }
};

#endif