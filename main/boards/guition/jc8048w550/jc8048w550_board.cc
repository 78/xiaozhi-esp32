#include "application.h"
#include "assets/lang_config.h"
#include "codecs/no_audio_codec.h"
#include "config.h"
#include "dashboard_fetcher.h"
#include "display/lcd_display.h"
#include "display/lvgl_display/lvgl_theme.h"
#include "idle_dashboard.h"
#include "vi_fonts.h"
#include "wifi_board.h"

#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_rgb.h>
#include <esp_lcd_touch_gt911.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_lvgl_port.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

#define TAG "JC8048W550Board"

// Two-part layout for the 800x480 panel: the emotion in a 240px column on the
// left, centred, and a right panel that shows the conversation while
// talking and a board of info cards while idle.
//
// The stock chat UI registers no touch handler of its own, and the only
// physical switch (BOOT) shares IO0 with the speaker's bit clock. A tap on the
// emotion column is the only way to start or stop talking.
class JC8048W550Display : public RgbLcdDisplay {
public:
    using RgbLcdDisplay::RgbLcdDisplay;

    void SetupUI() override {
        if (right_panel_ != nullptr) {
            return;
        }
        RgbLcdDisplay::SetupUI();
        DisplayLockGuard lock(this);
        GrowLvglPool();
        if (container_ == nullptr || emoji_box_ == nullptr || emoji_image_ == nullptr ||
            top_bar_ == nullptr) {
            return;
        }

        ApplyBackground();
        LayoutEmojiColumn();
        BuildRightPanel();
        RefreshRightPanel();

        // Only the emotion column starts and stops listening. The emotion
        // image and glyph are not clickable, so a tap on them lands on the box.
        lv_obj_add_flag(emoji_box_, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(emoji_box_, OnTap, LV_EVENT_CLICKED, this);
    }

    bool SetEmotion(const char* emotion) override {
        bool shown = RgbLcdDisplay::SetEmotion(emotion);
        if (emoji_image_ != nullptr) {
            DisplayLockGuard lock(this);
            FitEmojiImage();
        }
        return shown;
    }

    // The base class has no idea which status means "talking"; the Application
    // sets these three for the whole conversation and Standby (or the idle
    // clock) afterwards.
    void SetStatus(const char* status) override {
        RgbLcdDisplay::SetStatus(status);
        if (status == nullptr || right_panel_ == nullptr) {
            return;
        }
        DisplayLockGuard lock(this);
        talking_ = strcmp(status, Lang::Strings::CONNECTING) == 0 ||
                   strcmp(status, Lang::Strings::LISTENING) == 0 ||
                   strcmp(status, Lang::Strings::SPEAKING) == 0;
        RefreshRightPanel();
    }

    void SetChatMessage(const char* role, const char* content) override {
        if (role == nullptr || content == nullptr) {
            return;
        }
        if (chat_list_ == nullptr) {
            ESP_LOGW(TAG, "SetChatMessage('%s') called before SetupUI() - message will be lost!",
                     role);
            return;
        }
        DisplayLockGuard lock(this);
        if (content[0] == '\0') {
            // An empty system message is how the Application withdraws the
            // previous one (e.g. "please wait").
            if (strcmp(role, kRoleSystem) == 0) {
                uint32_t count = lv_obj_get_child_cnt(chat_list_);
                while (count > 0) {
                    lv_obj_t* last = lv_obj_get_child(chat_list_, count - 1);
                    if (!RowHasRole(last, kRoleSystem)) {
                        break;
                    }
                    lv_obj_delete(last);
                    count--;
                }
                RefreshRightPanel();
            }
            return;
        }

        // Show the list first so the bubble can be measured and scrolled to.
        if (dashboard_ != nullptr) {
            dashboard_->HideDetail();
        }
        lv_obj_remove_flag(chat_list_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(info_panel_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_t* row = AddChatBubble(role, content);
        while (lv_obj_get_child_cnt(chat_list_) > kMaxChatRows) {
            lv_obj_delete(lv_obj_get_child(chat_list_, 0));
        }
        RefreshRightPanel();
        lv_obj_update_layout(chat_list_);
        lv_obj_scroll_to_view(row, LV_ANIM_OFF);
    }

    void RemoveChatMessage(const char* role, const char* content) override {
        if (chat_list_ == nullptr || role == nullptr || content == nullptr) {
            return;
        }
        DisplayLockGuard lock(this);
        uint32_t count = lv_obj_get_child_cnt(chat_list_);
        if (count == 0) {
            return;
        }
        lv_obj_t* last = lv_obj_get_child(chat_list_, count - 1);
        lv_obj_t* bubble = lv_obj_get_child(last, 0);
        lv_obj_t* label = bubble != nullptr ? lv_obj_get_child(bubble, 0) : nullptr;
        if (label == nullptr || strcmp(lv_label_get_text(label), content) != 0) {
            return;
        }
        lv_obj_delete(last);
        RefreshRightPanel();
    }

    void ClearChatMessages() override {
        if (chat_list_ == nullptr) {
            return;
        }
        DisplayLockGuard lock(this);
        lv_obj_clean(chat_list_);
        RefreshRightPanel();
    }

    // Called from the fetch task. Keeps the latest data so a dashboard that is
    // built after the first fetch still gets it.
    void ApplyDashboardSnapshot(const dashboard::Snapshot& snapshot) {
        DisplayLockGuard lock(this);
        last_snapshot_ = snapshot;
        has_snapshot_ = true;
        if (dashboard_ != nullptr) {
            dashboard_->Update(last_snapshot_);
        }
    }

    void SetTheme(Theme* theme) override {
        RgbLcdDisplay::SetTheme(theme);
        if (right_panel_ == nullptr) {
            return;
        }
        DisplayLockGuard lock(this);
        ApplyBackground();
        for (uint32_t i = 0; i < lv_obj_get_child_cnt(chat_list_); ++i) {
            ColorBubble(lv_obj_get_child(chat_list_, i));
        }
    }

private:
    static constexpr int32_t kEmojiSize = DISPLAY_EMOJI_COLUMN_WIDTH;
    static constexpr uint32_t kMaxChatRows = 12;
    static constexpr uint32_t kBackgroundColor = 0x18181C;
    static constexpr const char* kRoleUser = "user";
    static constexpr const char* kRoleAssistant = "assistant";
    static constexpr const char* kRoleSystem = "system";

    lv_obj_t* right_panel_ = nullptr;
    lv_obj_t* chat_list_ = nullptr;
    lv_obj_t* info_panel_ = nullptr;
    int32_t panel_width_ = 0;  // usable width inside the right panel's padding
    bool talking_ = false;
    std::unique_ptr<IdleDashboard> dashboard_;
    dashboard::Snapshot last_snapshot_;
    bool has_snapshot_ = false;

    static void OnTap(lv_event_t* e) {
        // With the repo detail open, a tap outside the card only closes it.
        auto* display = static_cast<JC8048W550Display*>(lv_event_get_user_data(e));
        if (display != nullptr && display->dashboard_ != nullptr &&
            display->dashboard_->IsDetailOpen()) {
            display->dashboard_->HideDetail();
            return;
        }
        ESP_LOGI(TAG, "tap: toggling chat state");
        // ToggleChatState only sets an event bit, so the LVGL task may call it.
        Application::GetInstance().ToggleChatState();
    }

    // RgbLcdDisplay leaves LVGL with only the 1 MB base pool. The 240x240 emoji
    // GIFs need about 300 KB each, so changing emotion during a conversation
    // exhausted it and the emoji disappeared. Add PSRAM pools, one per
    // LV_MEM_SIZE as TLSF rejects larger ones (see SpiLcdDisplay).
    void GrowLvglPool() {
        constexpr size_t kExtraPools = 2;
        for (size_t i = 0; i < kExtraPools; i++) {
            void* pool = heap_caps_malloc(LV_MEM_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (pool == nullptr || lv_mem_add_pool(pool, LV_MEM_SIZE) == nullptr) {
                heap_caps_free(pool);
                ESP_LOGE(TAG, "Failed to add LVGL PSRAM pool %u", static_cast<unsigned>(i));
                return;
            }
        }
        ESP_LOGI(TAG, "Added %u KB LVGL pool in PSRAM",
                 static_cast<unsigned>(kExtraPools * LV_MEM_SIZE / 1024));
    }

    // The theme's own background replaced by this board's fixed one; redone
    // after every theme change because the base class resets it.
    void ApplyBackground() {
        const lv_color_t background = lv_color_hex(kBackgroundColor);
        for (lv_obj_t* obj : {lv_screen_active(), container_, top_bar_, bottom_bar_}) {
            if (obj != nullptr) {
                lv_obj_set_style_bg_color(obj, background, 0);
            }
        }
    }

    static void PassTaps(lv_obj_t* obj) { lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE); }

    static bool RowHasRole(lv_obj_t* row, const char* role) {
        const char* row_role = static_cast<const char*>(lv_obj_get_user_data(row));
        return row_role != nullptr && strcmp(row_role, role) == 0;
    }

    // Only the width is limited: an emotion image keeps its own size, any
    // height, and is scaled down (keeping its proportions) only when it is wider
    // than the column or taller than the screen.
    void FitEmojiImage() {
        int32_t width = lv_image_get_src_width(emoji_image_);
        int32_t height = lv_image_get_src_height(emoji_image_);
        if (width <= 0 || height <= 0) {
            return;
        }
        if (width > kEmojiSize) {
            height = height * kEmojiSize / width;
            width = kEmojiSize;
        }
        if (height > height_) {
            width = width * height_ / height;
            height = height_;
        }
        lv_obj_set_size(emoji_image_, width, height);
        lv_image_set_inner_align(emoji_image_, LV_IMAGE_ALIGN_STRETCH);
    }

    // Left column: as wide as the emotion may be and as tall as the screen,
    // with the emotion image or glyph centred in it. The column is also the
    // touch target that toggles listening (see SetupUI).
    void LayoutEmojiColumn() {
        lv_obj_set_size(emoji_box_, kEmojiSize, height_);
        lv_obj_align(emoji_box_, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        lv_obj_remove_flag(emoji_box_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(emoji_box_, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(emoji_box_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
    }

    void BuildRightPanel() {
        auto theme = static_cast<LvglTheme*>(current_theme_);
        const int32_t pad = theme->spacing(4);
        const int32_t gap = theme->spacing(3);
        lv_obj_update_layout(top_bar_);
        const int32_t top = lv_obj_get_height(top_bar_) + pad;
        panel_width_ = width_ - kEmojiSize - 2 * pad;

        // Sits right above the container so the status bars stay on top.
        right_panel_ = lv_obj_create(lv_screen_active());
        lv_obj_set_size(right_panel_, width_ - kEmojiSize, height_);
        lv_obj_align(right_panel_, LV_ALIGN_TOP_RIGHT, 0, 0);
        lv_obj_set_style_radius(right_panel_, 0, 0);
        lv_obj_set_style_bg_opa(right_panel_, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(right_panel_, 0, 0);
        lv_obj_set_style_pad_all(right_panel_, pad, 0);
        lv_obj_set_style_pad_top(right_panel_, top, 0);
        lv_obj_remove_flag(right_panel_, LV_OBJ_FLAG_SCROLLABLE);
        PassTaps(right_panel_);
        lv_obj_move_to_index(right_panel_, 1);

        chat_list_ = lv_obj_create(right_panel_);
        lv_obj_set_size(chat_list_, lv_pct(100), lv_pct(100));
        lv_obj_set_style_radius(chat_list_, 0, 0);
        lv_obj_set_style_bg_opa(chat_list_, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(chat_list_, 0, 0);
        lv_obj_set_style_pad_all(chat_list_, 0, 0);
        lv_obj_set_style_pad_row(chat_list_, gap, 0);
        lv_obj_set_scrollbar_mode(chat_list_, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_scroll_dir(chat_list_, LV_DIR_VER);
        lv_obj_set_flex_flow(chat_list_, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(chat_list_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_START);
        PassTaps(chat_list_);

        info_panel_ = lv_obj_create(right_panel_);
        lv_obj_set_size(info_panel_, lv_pct(100), lv_pct(100));
        lv_obj_set_style_radius(info_panel_, 0, 0);
        lv_obj_set_style_bg_opa(info_panel_, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(info_panel_, 0, 0);
        lv_obj_set_style_pad_all(info_panel_, 0, 0);
        lv_obj_set_style_pad_row(info_panel_, gap, 0);
        lv_obj_set_style_pad_column(info_panel_, gap, 0);
        lv_obj_remove_flag(info_panel_, LV_OBJ_FLAG_SCROLLABLE);
        PassTaps(info_panel_);

        // The idle view takes over info_panel_ (flex column) and hosts its
        // detail overlay on the whole right panel.
        DashboardFonts fonts = {&lv_font_montserrat_16, &lv_font_montserrat_20,
                                &lv_font_montserrat_28, &font_day_72, &font_vi_20, &font_vi_16,
                                theme->icon_font()->font(), nullptr};
        dashboard_ =
            std::make_unique<IdleDashboard>(info_panel_, right_panel_, panel_width_, fonts);
        if (has_snapshot_) {
            dashboard_->Update(last_snapshot_);
        }
    }

    // Chat while talking (or while a system message such as an error or an
    // activation code is up), the idle dashboard otherwise.
    void RefreshRightPanel() {
        bool show_chat = talking_ || lv_obj_get_child_cnt(chat_list_) > 0;
        if (show_chat) {
            if (dashboard_ != nullptr) {
                dashboard_->HideDetail();
            }
            lv_obj_remove_flag(chat_list_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(info_panel_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(chat_list_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(info_panel_, LV_OBJ_FLAG_HIDDEN);
        }
    }

    void ColorBubble(lv_obj_t* row) {
        auto theme = static_cast<LvglTheme*>(current_theme_);
        lv_obj_t* bubble = lv_obj_get_child(row, 0);
        if (bubble == nullptr) {
            return;
        }
        bool is_system = RowHasRole(row, kRoleSystem);
        lv_color_t bg = RowHasRole(row, kRoleUser) ? theme->user_bubble_color()
                                                   : (is_system ? theme->system_bubble_color()
                                                                : theme->assistant_bubble_color());
        lv_obj_set_style_bg_color(bubble, bg, 0);
        lv_obj_set_style_bg_opa(bubble, LV_OPA_70, 0);
        lv_obj_set_style_text_color(
            bubble, is_system ? theme->system_text_color() : theme->text_color(), 0);
    }

    // One full-width row per message so the bubble can sit left (assistant),
    // right (user) or centred (system). Returns the row.
    lv_obj_t* AddChatBubble(const char* role, const char* content) {
        auto theme = static_cast<LvglTheme*>(current_theme_);
        const char* kind = strcmp(role, kRoleUser) == 0     ? kRoleUser
                           : strcmp(role, kRoleSystem) == 0 ? kRoleSystem
                                                            : kRoleAssistant;
        lv_flex_align_t place = kind == kRoleUser     ? LV_FLEX_ALIGN_END
                                : kind == kRoleSystem ? LV_FLEX_ALIGN_CENTER
                                                      : LV_FLEX_ALIGN_START;

        lv_obj_t* row = lv_obj_create(chat_list_);
        lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_style_radius(row, 0, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        PassTaps(row);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, place, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_user_data(row, const_cast<char*>(kind));

        const int32_t pad = theme->spacing(4);
        lv_obj_t* bubble = lv_obj_create(row);
        lv_obj_set_size(bubble, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_radius(bubble, 8, 0);
        lv_obj_set_style_border_width(bubble, 0, 0);
        lv_obj_set_style_pad_all(bubble, pad, 0);
        lv_obj_set_scrollbar_mode(bubble, LV_SCROLLBAR_MODE_OFF);
        lv_obj_remove_flag(bubble, LV_OBJ_FLAG_SCROLLABLE);
        PassTaps(bubble);

        // Measure the text on one line, then cap the width so long text wraps.
        lv_obj_t* label = lv_label_create(bubble);
        lv_label_set_text(label, content);
        lv_obj_set_width(label, LV_SIZE_CONTENT);
        lv_obj_update_layout(label);
        const int32_t max_width = panel_width_ * 85 / 100 - 2 * pad;
        lv_obj_set_width(label, std::min<int32_t>(lv_obj_get_width(label), max_width));
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);

        ColorBubble(row);
        return row;
    }
};

// The stock NoAudioCodec shifts samples right by 12, which is +24 dB for the
// 24-bit INMP441 and clips close speech. This variant uses AUDIO_MIC_SAMPLE_SHIFT
// and logs the input level so the shift can be tuned on the device.
class JC8048W550AudioCodec : public NoAudioCodecSimplex {
public:
    using NoAudioCodecSimplex::NoAudioCodecSimplex;

protected:
    virtual int Read(int16_t* dest, int samples) override {
        constexpr uint32_t kReadTimeoutMs = 200;
        constexpr int kLogIntervalSamples = AUDIO_INPUT_SAMPLE_RATE * 2;

        size_t bytes_read = 0;
        read_buffer_.resize(samples);
        if (i2s_channel_read(rx_handle_, read_buffer_.data(), samples * sizeof(int32_t),
                             &bytes_read, kReadTimeoutMs) != ESP_OK) {
            return 0;
        }

        samples = bytes_read / sizeof(int32_t);
        for (int i = 0; i < samples; i++) {
            int32_t value = read_buffer_[i] >> AUDIO_MIC_SAMPLE_SHIFT;
            if (value > INT16_MAX || value < -INT16_MAX) {
                clipped_++;
                value = value > 0 ? INT16_MAX : -INT16_MAX;
            }
            dest[i] = static_cast<int16_t>(value);
            peak_ = std::max<int32_t>(peak_, std::abs(value));
        }

        logged_samples_ += samples;
        if (logged_samples_ >= kLogIntervalSamples) {
            ESP_LOGI(TAG, "Mic peak %ld/32767, clipped %d of %d samples", static_cast<long>(peak_),
                     clipped_, logged_samples_);
            peak_ = 0;
            clipped_ = 0;
            logged_samples_ = 0;
        }
        return samples;
    }

private:
    std::vector<int32_t> read_buffer_;
    int32_t peak_ = 0;
    int clipped_ = 0;
    int logged_samples_ = 0;
};

class JC8048W550Board : public WifiBoard {
private:
    i2c_master_bus_handle_t touch_i2c_bus_ = nullptr;
    LcdDisplay* display_ = nullptr;
    std::unique_ptr<DashboardFetcher> fetcher_;

    void InitializeDisplay() {
        esp_lcd_panel_handle_t panel = nullptr;

        esp_lcd_rgb_panel_config_t rgb_config = {};
        rgb_config.clk_src = LCD_CLK_SRC_DEFAULT;
        rgb_config.timings.pclk_hz = LCD_PCLK_HZ;
        rgb_config.timings.h_res = DISPLAY_WIDTH;
        rgb_config.timings.v_res = DISPLAY_HEIGHT;
        rgb_config.timings.hsync_pulse_width = 4;
        rgb_config.timings.hsync_back_porch = 8;
        rgb_config.timings.hsync_front_porch = 8;
        rgb_config.timings.vsync_pulse_width = 4;
        rgb_config.timings.vsync_back_porch = 8;
        rgb_config.timings.vsync_front_porch = 8;
        // The vendor demo runs hsync/vsync polarity 0, which Arduino_GFX turns
        // into idle-low sync lines.
        rgb_config.timings.flags.hsync_idle_low = true;
        rgb_config.timings.flags.vsync_idle_low = true;
        rgb_config.timings.flags.pclk_active_neg = true;
        rgb_config.data_width = 16;
        rgb_config.in_color_format = LCD_COLOR_FMT_RGB565;
        rgb_config.out_color_format = LCD_COLOR_FMT_RGB565;
        rgb_config.num_fbs = 2;
        rgb_config.bounce_buffer_size_px = DISPLAY_WIDTH * 10;
        rgb_config.dma_burst_size = 64;
        rgb_config.hsync_gpio_num = LCD_PIN_HSYNC;
        rgb_config.vsync_gpio_num = LCD_PIN_VSYNC;
        rgb_config.de_gpio_num = LCD_PIN_DE;
        rgb_config.pclk_gpio_num = LCD_PIN_PCLK;
        rgb_config.disp_gpio_num = LCD_PIN_DISP;
        rgb_config.data_gpio_nums[0] = LCD_PIN_DATA0;
        rgb_config.data_gpio_nums[1] = LCD_PIN_DATA1;
        rgb_config.data_gpio_nums[2] = LCD_PIN_DATA2;
        rgb_config.data_gpio_nums[3] = LCD_PIN_DATA3;
        rgb_config.data_gpio_nums[4] = LCD_PIN_DATA4;
        rgb_config.data_gpio_nums[5] = LCD_PIN_DATA5;
        rgb_config.data_gpio_nums[6] = LCD_PIN_DATA6;
        rgb_config.data_gpio_nums[7] = LCD_PIN_DATA7;
        rgb_config.data_gpio_nums[8] = LCD_PIN_DATA8;
        rgb_config.data_gpio_nums[9] = LCD_PIN_DATA9;
        rgb_config.data_gpio_nums[10] = LCD_PIN_DATA10;
        rgb_config.data_gpio_nums[11] = LCD_PIN_DATA11;
        rgb_config.data_gpio_nums[12] = LCD_PIN_DATA12;
        rgb_config.data_gpio_nums[13] = LCD_PIN_DATA13;
        rgb_config.data_gpio_nums[14] = LCD_PIN_DATA14;
        rgb_config.data_gpio_nums[15] = LCD_PIN_DATA15;
        rgb_config.flags.fb_in_psram = 1;

        ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&rgb_config, &panel));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));

        display_ = new JC8048W550Display(nullptr, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                         DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X,
                                         DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeTouchI2c() {
        i2c_master_bus_config_t bus_config = {};
        bus_config.i2c_port = TOUCH_I2C_PORT;
        bus_config.sda_io_num = TOUCH_PIN_NUM_SDA;
        bus_config.scl_io_num = TOUCH_PIN_NUM_SCL;
        bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
        bus_config.glitch_ignore_cnt = 7;
        bus_config.flags.enable_internal_pullup = 1;
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &touch_i2c_bus_));
    }

    bool TryInitializeTouchAt(uint8_t dev_addr) {
        esp_lcd_panel_io_handle_t tp_io_handle = nullptr;
        esp_lcd_panel_io_i2c_config_t tp_io_config = {};
        tp_io_config.dev_addr = dev_addr;
        tp_io_config.control_phase_bytes = 1;
        tp_io_config.dc_bit_offset = 0;
        tp_io_config.lcd_cmd_bits = 16;
        tp_io_config.flags.disable_control_phase = 1;
        tp_io_config.scl_speed_hz = 400000;
        if (esp_lcd_new_panel_io_i2c(touch_i2c_bus_, &tp_io_config, &tp_io_handle) != ESP_OK) {
            return false;
        }

        esp_lcd_touch_config_t tp_cfg = {};
        tp_cfg.x_max = DISPLAY_WIDTH;
        tp_cfg.y_max = DISPLAY_HEIGHT;
        tp_cfg.rst_gpio_num = TOUCH_PIN_NUM_RST;
        tp_cfg.int_gpio_num = TOUCH_PIN_NUM_INT;
        tp_cfg.levels.reset = 0;
        tp_cfg.levels.interrupt = 0;
        tp_cfg.flags.swap_xy = DISPLAY_SWAP_XY;
        tp_cfg.flags.mirror_x = DISPLAY_MIRROR_X;
        tp_cfg.flags.mirror_y = DISPLAY_MIRROR_Y;

        esp_lcd_touch_handle_t tp = nullptr;
        esp_err_t err = esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &tp);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "No GT911 at 0x%02X: %s", dev_addr, esp_err_to_name(err));
            esp_lcd_panel_io_del(tp_io_handle);
            return false;
        }

        lvgl_port_touch_cfg_t touch_cfg = {};
        touch_cfg.disp = lv_display_get_default();
        touch_cfg.handle = tp;
        if (touch_cfg.disp == nullptr) {
            ESP_LOGE(TAG, "LVGL display is not initialized, touch stays inactive");
            return false;
        }
        if (lvgl_port_add_touch(&touch_cfg) == nullptr) {
            ESP_LOGE(TAG, "lvgl_port_add_touch failed, touch stays inactive");
            return false;
        }

        ESP_LOGI(TAG, "GT911 ready at 0x%02X", dev_addr);
        return true;
    }

    void InitializeTouch() {
        // INT is not wired, so the chip's I2C address is not driven: it comes
        // up on 0x5D or 0x14. Try both rather than leave touch dead silently.
        if (TryInitializeTouchAt(ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS)) {
            return;
        }
        if (TryInitializeTouchAt(ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP)) {
            return;
        }
        // Not fatal: the display stays usable.
        ESP_LOGE(TAG, "GT911 not found at 0x5D or 0x14, touch disabled");
    }

    // Powered from USB, so modem sleep only costs latency and packet loss when
    // the signal is weak. The default event loop does not exist yet when the
    // board is constructed, so poll the AP info instead of hooking IP events.
    static void WifiMonitorTick(void*) {
        static bool power_save_off = false;
        wifi_ap_record_t ap = {};
        if (esp_wifi_sta_get_ap_info(&ap) != ESP_OK) {
            return;
        }
        if (!power_save_off && esp_wifi_set_ps(WIFI_PS_NONE) == ESP_OK) {
            power_save_off = true;
            ESP_LOGI(TAG, "WiFi power save disabled");
        }
        ESP_LOGI(TAG, "WiFi RSSI %d dBm, channel %u", ap.rssi, ap.primary);
    }

    void InitializeWifiMonitor() {
        const esp_timer_create_args_t args = {
            .callback = WifiMonitorTick,
            .arg = nullptr,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "wifi_monitor",
            .skip_unhandled_events = true,
        };
        esp_timer_handle_t timer = nullptr;
        if (esp_timer_create(&args, &timer) == ESP_OK) {
            esp_timer_start_periodic(timer, 30 * 1000000ULL);
        }
    }

public:
    JC8048W550Board() {
        InitializeWifiMonitor();
        InitializeDisplay();
        InitializeTouchI2c();
        InitializeTouch();
        GetBacklight()->RestoreBrightness();
        fetcher_ = std::make_unique<DashboardFetcher>(
            [display =
                 static_cast<JC8048W550Display*>(display_)](const dashboard::Snapshot& snapshot) {
                display->ApplyDashboardSnapshot(snapshot);
            });
        fetcher_->Start();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static JC8048W550AudioCodec audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                               AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK,
                                               AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK,
                                               AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override { return display_; }

    virtual Backlight* GetBacklight() override {
        // The stock 25 kHz leaves this board's backlight dark; the vendor/ESPHome
        // definition runs it at about 1.2 kHz.
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT,
                                      DISPLAY_BACKLIGHT_PWM_HZ);
        return &backlight;
    }
};

DECLARE_BOARD(JC8048W550Board);
