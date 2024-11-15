#include <esp_log.h>
#include <esp_err.h>
#include <string>
#include <cstdlib>

#include "display.h"
#include "board.h"
#include "application.h"

#define TAG "Display"
LV_FONT_DECLARE(font_dingding)
void Display::SetupUI()
{
    if (disp_ == nullptr)
    {
        return;
    }

    ESP_LOGI(TAG, "Setting up UI");
    Lock();
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), 0); // 修改背景为黑色

    label_ = lv_label_create(lv_disp_get_scr_act(disp_));
    // lv_obj_set_style_text_font(label_, font_, 0);
    lv_obj_set_style_text_font(label_, &font_dingding, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label_, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_align(label_, LV_ALIGN_BOTTOM_LEFT, 10, 20);

    // lv_obj_set_style_text_color(label_, lv_color_black(), 0);
    lv_label_set_text(label_, "Initializing...");
    lv_obj_set_width(label_, disp_->driver->hor_res);
    lv_obj_set_height(label_, disp_->driver->ver_res);

    notification_ = lv_label_create(lv_disp_get_scr_act(disp_));
    lv_obj_set_style_text_font(notification_, &font_dingding, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(notification_, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_align(notification_, LV_ALIGN_TOP_LEFT, 10, 40);

    // lv_obj_set_style_text_font(notification_, font_, 0);
    // lv_obj_set_style_text_color(notification_, lv_color_black(), 0);
    lv_label_set_text(notification_, "Notification\nTest");
    lv_obj_set_width(notification_, disp_->driver->hor_res);
    lv_obj_set_height(notification_, disp_->driver->ver_res);
    lv_obj_set_style_opa(notification_, LV_OPA_MIN, 0);
    Unlock();

    // Create a timer to update the display every 10 seconds
    esp_timer_create_args_t update_display_timer_args = {
        .callback = [](void *arg)
        {
            Display *display = static_cast<Display *>(arg);
            display->UpdateDisplay();
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "UpdateDisplay",
        .skip_unhandled_events = false,
    };
    ESP_ERROR_CHECK(esp_timer_create(&update_display_timer_args, &update_display_timer_));
    ESP_ERROR_CHECK(esp_timer_start_periodic(update_display_timer_, 10 * 1000000));
}

Display::~Display()
{
    if (notification_timer_ != nullptr)
    {
        esp_timer_stop(notification_timer_);
        esp_timer_delete(notification_timer_);
    }
    if (update_display_timer_ != nullptr)
    {
        esp_timer_stop(update_display_timer_);
        esp_timer_delete(update_display_timer_);
    }
    if (label_ != nullptr)
    {
        lv_obj_del(label_);
        lv_obj_del(notification_);
    }
    if (font_ != nullptr)
    {
        lv_font_free(font_);
    }
}

void Display::SetText(const std::string &text)
{
    if (label_ != nullptr)
    {
        text_ = text;
        Lock();
        // Change the text of the label
        lv_label_set_text(label_, text_.c_str());
        Unlock();
    }
}

void Display::ShowNotification(const std::string &text)
{
    if (notification_ != nullptr)
    {
        Lock();
        lv_label_set_text(notification_, text.c_str());
        lv_obj_set_style_opa(notification_, LV_OPA_MAX, 0);
        lv_obj_set_style_opa(label_, LV_OPA_MIN, 0);
        Unlock();

        if (notification_timer_ != nullptr)
        {
            esp_timer_stop(notification_timer_);
            esp_timer_delete(notification_timer_);
        }

        esp_timer_create_args_t timer_args = {
            .callback = [](void *arg)
            {
                Display *display = static_cast<Display *>(arg);
                display->Lock();
                lv_obj_set_style_opa(display->notification_, LV_OPA_MIN, 0);
                lv_obj_set_style_opa(display->label_, LV_OPA_MAX, 0);
                display->Unlock();
            },
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "Notification Timer",
            .skip_unhandled_events = false,
        };
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &notification_timer_));
        ESP_ERROR_CHECK(esp_timer_start_once(notification_timer_, 3000000));
    }
}

void Display::UpdateDisplay()
{
    auto chat_state = Application::GetInstance().GetChatState();
    if (chat_state == kChatStateIdle)
    {
        std::string text;
        auto &board = Board::GetInstance();
        std::string network_name;
        int signal_quality;
        std::string signal_quality_text;
        if (!board.GetNetworkState(network_name, signal_quality, signal_quality_text))
        {
            text = "No network";
        }
        else
        {
            text = network_name + "\n" + signal_quality_text;
            if (std::abs(signal_quality) != 99)
            {
                text += " (" + std::to_string(signal_quality) + ")";
            }
        }

        int battery_level;
        bool charging;
        if (board.GetBatteryLevel(battery_level, charging)) {
            text += "\nPower " + std::to_string(battery_level) + "%";
            if (charging) {
                text += " (Charging)";
            }
        }
        SetText(text);
    }
}
