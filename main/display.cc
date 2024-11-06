#include <esp_log.h>
#include <esp_err.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lvgl_port.h>
#include <string>
#include <cstdlib>

#include "display.h"
#include "board.h"
#include "application.h"

#define TAG "Display"

Display::Display(int sda_pin, int scl_pin) : sda_pin_(sda_pin), scl_pin_(scl_pin) {
    if (sda_pin_ == GPIO_NUM_NC || scl_pin_ == GPIO_NUM_NC) {
        ESP_LOGI(TAG, "Display not connected");
        return;
    }

    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = (gpio_num_t)sda_pin_,
        .scl_io_num = (gpio_num_t)scl_pin_,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = {
            .enable_internal_pullup = 0,
        },
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &i2c_bus_));

    // SSD1306 config
    esp_lcd_panel_io_i2c_config_t io_config = {
        .dev_addr = 0x3C,
        .on_color_trans_done = nullptr,
        .user_ctx = nullptr,
        .control_phase_bytes = 1,
        .dc_bit_offset = 6,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .flags = {
            .dc_low_on_data = 0,
            .disable_control_phase = 0,
        },
        .scl_speed_hz = 100 * 1000,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c_v2(i2c_bus_, &io_config, &panel_io_));

    ESP_LOGI(TAG, "Install SSD1306 driver");
    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = -1;
    panel_config.bits_per_pixel = 1;

    esp_lcd_panel_ssd1306_config_t ssd1306_config = {
        .height = DISPLAY_HEIGHT
    };
    panel_config.vendor_config = &ssd1306_config;

    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(panel_io_, &panel_config, &panel_));
    ESP_LOGI(TAG, "SSD1306 driver installed");

    // Reset the display
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
    if (esp_lcd_panel_init(panel_) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize display");
        return;
    }

    ESP_LOGI(TAG, "Initialize LVGL");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_port_init(&port_cfg);

    // Set the display to on
    ESP_LOGI(TAG, "Turning display on");
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

    ESP_LOGI(TAG, "Adding LCD screen");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io_,
        .panel_handle = panel_,
        .control_handle = nullptr,
        .buffer_size = DISPLAY_WIDTH * DISPLAY_HEIGHT,
        .double_buffer = false,
        .trans_size = 0,
        .hres = DISPLAY_WIDTH,
        .vres = DISPLAY_HEIGHT,
        .monochrome = true,
        .rotation = {
            .swap_xy = false,
            .mirror_x = DISPLAY_MIRROR_X,
            .mirror_y = DISPLAY_MIRROR_Y,
        },
        .flags = {
            .buff_dma = 1,
            .buff_spiram = 0,
            .sw_rotate = 0,
            .full_refresh = 0,
            .direct_mode = 0,
        },
    };
    disp_ = lvgl_port_add_disp(&display_cfg);;
    
    ESP_LOGI(TAG, "Display Loading...");
    if (lvgl_port_lock(0)) {
        label_ = lv_label_create(lv_disp_get_scr_act(disp_));
        // lv_obj_set_style_text_font(label_, font_, 0);
        lv_label_set_text(label_, "Initializing...");
        lv_obj_set_width(label_, disp_->driver->hor_res);
        lv_obj_set_height(label_, disp_->driver->ver_res);

        notification_ = lv_label_create(lv_disp_get_scr_act(disp_));
        // lv_obj_set_style_text_font(notification_, font_, 0);
        lv_label_set_text(notification_, "Notification\nTest");
        lv_obj_set_width(notification_, disp_->driver->hor_res);
        lv_obj_set_height(notification_, disp_->driver->ver_res);
        lv_obj_set_style_opa(notification_, LV_OPA_MIN, 0);
        lvgl_port_unlock();
    }

    // Create a timer to update the display every 10 seconds
    esp_timer_create_args_t update_display_timer_args = {
        .callback = [](void *arg) {
            Display* display = static_cast<Display*>(arg);
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

Display::~Display() {
    if (notification_timer_ != nullptr) {
        esp_timer_stop(notification_timer_);
        esp_timer_delete(notification_timer_);
    }
    if (update_display_timer_ != nullptr) {
        esp_timer_stop(update_display_timer_);
        esp_timer_delete(update_display_timer_);
    }

    lvgl_port_lock(0);
    if (label_ != nullptr) {
        lv_obj_del(label_);
        lv_obj_del(notification_);
    }
    lvgl_port_unlock();

    if (font_ != nullptr) {
        lv_font_free(font_);
    }

    if (disp_ != nullptr) {
        lvgl_port_deinit();
        esp_lcd_panel_del(panel_);
        esp_lcd_panel_io_del(panel_io_);
        i2c_master_bus_reset(i2c_bus_);
    }
}

void Display::SetText(const std::string &text) {
    if (label_ != nullptr) {
        text_ = text;
        lvgl_port_lock(0);
        // Change the text of the label
        lv_label_set_text(label_, text_.c_str());
        lvgl_port_unlock();
    }
}

void Display::ShowNotification(const std::string &text) {
    if (notification_ != nullptr) {
        lvgl_port_lock(0);
        lv_label_set_text(notification_, text.c_str());
        lv_obj_set_style_opa(notification_, LV_OPA_MAX, 0);
        lv_obj_set_style_opa(label_, LV_OPA_MIN, 0);
        lvgl_port_unlock();

        if (notification_timer_ != nullptr) {
            esp_timer_stop(notification_timer_);
            esp_timer_delete(notification_timer_);
        }

        esp_timer_create_args_t timer_args = {
            .callback = [](void *arg) {
                Display *display = static_cast<Display*>(arg);
                lvgl_port_lock(0);
                lv_obj_set_style_opa(display->notification_, LV_OPA_MIN, 0);
                lv_obj_set_style_opa(display->label_, LV_OPA_MAX, 0);
                lvgl_port_unlock();
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

void Display::UpdateDisplay() {
    auto chat_state = Application::GetInstance().GetChatState();
    if (chat_state == kChatStateIdle || chat_state == kChatStateConnecting || chat_state == kChatStateListening) {
        std::string text;
        auto& board = Board::GetInstance();
        std::string network_name;
        int signal_quality;
        std::string signal_quality_text;
        if (!board.GetNetworkState(network_name, signal_quality, signal_quality_text)) {
            text = "No network";
        } else {
            text = network_name + "\n" + signal_quality_text;
            if (std::abs(signal_quality) != 99) {
                text += " (" + std::to_string(signal_quality) + ")";
            }
        }

        int battery_voltage;
        bool charging;
        if (board.GetBatteryVoltage(battery_voltage, charging)) {
            text += "\n" + std::to_string(battery_voltage) + "mV";
            if (charging) {
                text += " (Charging)";
            }
        }
        SetText(text);
    }
}
