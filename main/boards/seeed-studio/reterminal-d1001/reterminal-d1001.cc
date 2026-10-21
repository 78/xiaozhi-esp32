#include "wifi_board.h"

#include "application.h"
#include "assets.h"
#include "button.h"
#include "config.h"
#include "display/lcd_display.h"
#include "display/lvgl_display/lvgl_font.h"
#include "esp_lcd_touch_gsl3670.h"
#include "d1001_camera.h"
#include "lcd_init_cmds.h"
#include "reterminal_d1001_audio_codec.h"
#include "reterminal_d1001_expander.h"
#include "settings_ui.h"
#include "push_panel.h"
#include "camera_tuning.h"
#include "battery_monitor.h"
#include "face_service.h"

#include <esp_netif.h>
#include "power_save_timer.h"
#include "settings.h"

#include <atomic>
#include <driver/i2c_master.h>
#include <esp_lcd_jd9365.h>
#include <esp_lcd_mipi_dsi.h>
#include <esp_lcd_panel_ops.h>
#include <esp_ldo_regulator.h>
#include "display/lvgl_display/lvgl_theme.h"

#include <esp_log.h>
#include <esp_lvgl_port.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <material_symbols.h>
#include <ssid_manager.h>
#include <wifi_manager.h>

#include <memory>
#include <string>

#define TAG "ReTerminalD1001"

// Display subclass that keeps the stock layout and only adds the settings and
// rotation entries to the status bar.
class ReTerminalD1001Display final : public MipiLcdDisplay {
public:
    using OpenSettingsCallback = std::function<void(SettingsPage)>;

    ReTerminalD1001Display(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                           int width, int height, int offset_x, int offset_y, bool mirror_x,
                           bool mirror_y, bool swap_xy)
        : MipiLcdDisplay(panel_io, panel, width, height, offset_x, offset_y, mirror_x, mirror_y,
                         swap_xy) {}

    void SetOpenSettingsCallback(OpenSettingsCallback callback) {
        open_settings_ = std::move(callback);
    }

    void SetOnThemeChanged(std::function<void()> callback) {
        on_theme_changed_ = std::move(callback);
    }

    // The theme owns the fonts and frees them when it is reloaded, so they are
    // always read back from the theme and never cached.
    const lv_font_t* GetIconFont(bool large) const {
        auto* theme = static_cast<LvglTheme*>(current_theme_);
        if (theme == nullptr) {
            return nullptr;
        }
        auto font = large ? theme->large_icon_font() : theme->icon_font();
        return font == nullptr ? nullptr : font->font();
    }

    void SetupUI() override {
        MipiLcdDisplay::SetupUI();

        DisplayLockGuard lock(this);
        if (status_bar_ == nullptr) {
            return;
        }
        auto* theme = static_cast<LvglTheme*>(current_theme_);
        const lv_font_t* icon_font = GetIconFont(false);
        // Mirrors the stock top bar: spacing(4) of left padding before the
        // network icon, which is a single (roughly square) symbol glyph.
        const int gap = theme != nullptr ? theme->spacing(3) : 12;
        const int left_pad = theme != nullptr ? theme->spacing(4) : 16;
        const int icon_size = icon_font != nullptr ? icon_font->line_height : 24;

        // The status bar is transparent, spans the full width and is stacked
        // over the top bar, so it is the layer that actually receives taps in
        // the top strip. Keeping the entries inside it puts them next to the
        // network icon without reflowing the stock top bar layout. The minimum
        // height guarantees a 48 px tall touch band: hit testing only descends
        // into children of an object that was hit itself.
        if (lv_obj_get_style_min_height(status_bar_, LV_PART_MAIN) < kStatusBarTouchHeight) {
            lv_obj_set_style_min_height(status_bar_, kStatusBarTouchHeight, 0);
        }

        // The network icon itself lives in the stock top bar underneath, so it
        // never sees a tap. A transparent hit area of the same footprint is
        // placed over it inside the status bar and routes taps to the Wi-Fi
        // setup page.
        const int hotspot_width = left_pad + icon_size + gap;
        network_hotspot_ = lv_obj_create(status_bar_);
        lv_obj_remove_style_all(network_hotspot_);
        lv_obj_set_size(network_hotspot_, hotspot_width < kStatusBarTouchHeight
                                              ? kStatusBarTouchHeight
                                              : hotspot_width,
                        kStatusBarTouchHeight);
        // Both bars sit at the top of the screen with the same top padding, so
        // aligning to the top (instead of the middle of the taller status bar)
        // puts our entries on the network icon's own baseline.
        lv_obj_align(network_hotspot_, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_set_style_bg_opa(network_hotspot_, LV_OPA_TRANSP, 0);
        lv_obj_set_scrollbar_mode(network_hotspot_, LV_SCROLLBAR_MODE_OFF);
        lv_obj_remove_flag(network_hotspot_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(network_hotspot_, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(network_hotspot_, OnNetworkClicked, LV_EVENT_CLICKED, this);
        lv_obj_add_event_cb(network_hotspot_, OnStatusGesture, LV_EVENT_ALL, this);
        // Pulling down anywhere on the status strip opens the settings overlay,
        // so the entry no longer depends on hitting the small network hotspot.
        lv_obj_add_event_cb(status_bar_, OnStatusGesture, LV_EVENT_ALL, this);

        status_actions_ = lv_obj_create(status_bar_);
        lv_obj_remove_style_all(status_actions_);
        lv_obj_set_size(status_actions_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(status_actions_, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(status_actions_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        // The entries are full-height buttons; no extra column gap needed on
        // top of the padding each 48 px button already carries around its glyph.
        lv_obj_set_style_pad_column(status_actions_, 0, 0);
        lv_obj_set_scrollbar_mode(status_actions_, LV_SCROLLBAR_MODE_OFF);
        lv_obj_remove_flag(status_actions_, LV_OBJ_FLAG_SCROLLABLE);
        // The buttons inside are the full 48 px touch band tall with their
        // glyphs top-aligned, so a top alignment lands the glyphs on the same
        // horizontal line as the network label in the top bar.
        lv_obj_align(status_actions_, LV_ALIGN_TOP_LEFT, hotspot_width, 0);
        lv_obj_add_event_cb(status_actions_, OnStatusGesture, LV_EVENT_ALL, this);

        settings_icon_ = CreateStatusIcon(MATERIAL_SYMBOLS_SETTINGS, OnSettingsClicked);
        volume_icon_ = CreateStatusIcon(MATERIAL_SYMBOLS_VOLUME_UP, OnVolumeClicked);
        face_icon_ = CreateStatusIcon(MATERIAL_SYMBOLS_PERSON, OnFaceClicked);
        {
            Settings settings("face", false);
            ApplyFaceIconState(settings.GetInt("mode", 0));
        }

        // Apply pre-flashed CJK text font from assets partition after SetupUI() has
        // populated screen and container widgets, avoiding uninitialized widget styling.
        auto& assets = Assets::GetInstance();
        if (assets.partition_valid()) {
            void* font_ptr = nullptr;
            size_t font_size = 0;
            if (assets.GetAssetData("font_noto_sans_common_30_4.bin", font_ptr, font_size)) {
                auto text_font = std::make_shared<LvglCBinFont>(font_ptr);
                if (text_font->font() != nullptr && SetTextFont(text_font)) {
                    ESP_LOGI(TAG, "Boot CJK font (font_noto_sans_common_30_4.bin) loaded successfully");
                }
            }
        }
    }

    void SetTheme(Theme* theme) override {
        MipiLcdDisplay::SetTheme(theme);

        DisplayLockGuard lock(this);
        // The theme reload frees the previous fonts, so both entries are
        // re-pointed at the new ones before anything can draw with a dangling
        // pointer.
        ApplyStatusIconStyle(settings_icon_);
        ApplyStatusIconStyle(volume_icon_);
        ApplyStatusIconStyle(face_icon_);
        {
            Settings settings("face", false);
            ApplyFaceIconState(settings.GetInt("mode", 0));
        }
        if (on_theme_changed_) {
            on_theme_changed_();
        }
    }

    // All non-zero angles are software rotation in LVGL; the panel keeps its
    // native orientation and LVGL also rotates the touch coordinates.
    void ApplyRotation(int degrees) {
        lv_display_t* disp = lv_display_get_default();
        if (disp == nullptr) {
            return;
        }
        lv_display_rotation_t rotation = LV_DISPLAY_ROTATION_0;
        if (degrees == 90) {
            rotation = LV_DISPLAY_ROTATION_90;
        } else if (degrees == 180) {
            rotation = LV_DISPLAY_ROTATION_180;
        } else if (degrees == 270) {
            rotation = LV_DISPLAY_ROTATION_270;
        }
        if (rotation != LV_DISPLAY_ROTATION_0) {
            DisplayLockGuard lock(this);
            lv_display_set_rotation(disp, rotation);
        }
    }

    // Height of the bottom chat bar (0 if it was never created). Used by the
    // push panel to keep the chat text visible below the card. Must be called
    // with the display lock held.
    int32_t GetChatBarHeight() {
        if (bottom_bar_ == nullptr) {
            return 0;
        }
        lv_obj_update_layout(bottom_bar_);
        return lv_obj_get_height(bottom_bar_);
    }

    // Face wake toggle: callback flips the mode and returns the new one
    // (-1 = unavailable); the icon turns accent-colored while enabled.
    void SetFaceToggleCallback(std::function<int()> callback) {
        face_toggle_ = std::move(callback);
    }

    void SetFaceIconVisible(bool visible) {
        DisplayLockGuard lock(this);
        SetHidden(face_icon_, !visible);
    }

    // 0 = off (dimmed), 1 = detect-wake (full text color), 2 = recognize-wake
    // (accent).
    void ApplyFaceIconState(int mode) {
        if (face_icon_ == nullptr) {
            return;
        }
        ApplyStatusIconStyle(face_icon_);
        lv_obj_set_style_text_opa(face_icon_, mode == 0 ? LV_OPA_40 : LV_OPA_COVER, 0);
        if (mode == 2) {
            lv_obj_set_style_text_color(face_icon_, kAccentTextColor(), 0);
        }
    }

    void SetStatusBarEntriesHidden(bool hidden) {
        SetHidden(network_hotspot_, hidden);
        SetHidden(status_actions_, hidden);
    }

private:
    static constexpr int kStatusBarTouchHeight = 48;
    // Each status entry is a real 48x48 button. Extended click areas are
    // useless here (hit testing never leaves the parent's own bounds) and
    // neighboring pads overlapped, which made the rotation entry lose most
    // taps to the volume entry or the Wi-Fi hotspot.
    static constexpr int kStatusIconTouchWidth = 48;
    // A pull that travels this far down, while staying mostly vertical, opens
    // the overlay. Fired while the finger is still down so the release does
    // not also dispatch the underlying button's click.
    static constexpr int kPullOpenDistance = 40;

    static void SetHidden(lv_obj_t* obj, bool hidden) {
        if (obj == nullptr) {
            return;
        }
        if (hidden) {
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
        }
    }

    lv_obj_t* CreateStatusIcon(const char* glyph, lv_event_cb_t callback) {
        lv_obj_t* button = lv_obj_create(status_actions_);
        lv_obj_remove_style_all(button);
        lv_obj_set_size(button, kStatusIconTouchWidth, kStatusBarTouchHeight);
        lv_obj_set_style_radius(button, 16, 0);
        lv_obj_set_style_bg_opa(button, LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_color(button, lv_color_mix(kAccentColor(), ScreenBackgroundColor(), 42),
                                  LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(button, LV_OPA_40, LV_STATE_PRESSED);
        lv_obj_set_style_transform_pivot_x(button, LV_PCT(50), 0);
        lv_obj_set_style_transform_pivot_y(button, LV_PCT(50), 0);
        lv_obj_set_style_transform_scale(button, 250, LV_STATE_PRESSED);
        lv_obj_set_scrollbar_mode(button, LV_SCROLLBAR_MODE_OFF);
        lv_obj_remove_flag(button, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, this);
        lv_obj_add_event_cb(button, OnStatusGesture, LV_EVENT_ALL, this);

        lv_obj_t* label = lv_label_create(button);
        lv_label_set_text(label, glyph);
        lv_obj_remove_flag(label, LV_OBJ_FLAG_CLICKABLE);
        // Top-center: the glyph sits on the network label's baseline while the
        // button fills the whole touch band below it.
        lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 0);
        ApplyStatusIconStyle(label);
        return label;
    }

    void ApplyStatusIconStyle(lv_obj_t* label) {
        if (label == nullptr) {
            return;
        }
        const lv_font_t* icon_font = GetIconFont(false);
        if (icon_font != nullptr) {
            lv_obj_set_style_text_font(label, icon_font, 0);
        }
        auto* theme = static_cast<LvglTheme*>(current_theme_);
        if (theme != nullptr) {
            lv_obj_set_style_text_color(label, theme->text_color(), 0);
        }
        lv_obj_set_style_text_color(label, kAccentTextColor(), LV_STATE_PRESSED);
    }

    static lv_color_t ScreenBackgroundColor() {
        return lv_obj_get_style_bg_color(lv_screen_active(), LV_PART_MAIN);
    }

    // SenseCraft green, shared with the settings overlay.
    static lv_color_t kAccentColor() { return lv_color_hex(0x8CC020); }

    // Small accent glyphs need the deeper green on the light theme to keep
    // their contrast; the filled green surfaces keep using kAccentColor().
    static lv_color_t kAccentTextColor() {
        return lv_color_brightness(ScreenBackgroundColor()) > 128
                   ? lv_color_hex(0x3B5A11)
                   : kAccentColor();
    }

    static void OnNetworkClicked(lv_event_t* event) {
        Dispatch(event, SettingsPage::Wifi);
    }

    static void OnSettingsClicked(lv_event_t* event) {
        Dispatch(event, SettingsPage::Display);
    }

    static void OnRotationClicked(lv_event_t* event) {
        Dispatch(event, SettingsPage::Display);
    }

    static void OnVolumeClicked(lv_event_t* event) {
        Dispatch(event, SettingsPage::Volume);
    }

    static void OnFaceClicked(lv_event_t* event) {
        // Opens the face settings page (mode tiles + recognizer endpoint).
        Dispatch(event, SettingsPage::Face);
    }

    static void Dispatch(lv_event_t* event, SettingsPage page) {
        auto* self = static_cast<ReTerminalD1001Display*>(lv_event_get_user_data(event));
        if (self != nullptr && self->open_settings_) {
            self->open_settings_(page);
        }
    }

    // The status strip is only 48 px tall, so a pull-down swipe is detected
    // both through LVGL's gesture event and through the raw press delta. The
    // delta is evaluated while the finger is still down: calling
    // lv_indev_wait_release() from RELEASED is too late to suppress the click
    // that LVGL emits right after it, so a pull that started on an entry would
    // also fire that entry's action.
    static void OnStatusGesture(lv_event_t* event) {
        auto* self = static_cast<ReTerminalD1001Display*>(lv_event_get_user_data(event));
        if (self == nullptr) {
            return;
        }
        lv_event_code_t code = lv_event_get_code(event);
        lv_indev_t* indev = lv_indev_active();

        if (code == LV_EVENT_GESTURE) {
            if (indev != nullptr && lv_indev_get_gesture_dir(indev) == LV_DIR_BOTTOM) {
                self->gesture_tracking_ = false;
                lv_indev_wait_release(indev);
                Dispatch(event, SettingsPage::Display);
            }
            return;
        }
        if (code == LV_EVENT_PRESSED) {
            if (indev != nullptr) {
                lv_indev_get_point(indev, &self->gesture_start_);
                self->gesture_tracking_ = true;
            }
            return;
        }
        if (code == LV_EVENT_PRESS_LOST || code == LV_EVENT_INDEV_RESET ||
            code == LV_EVENT_SCROLL_BEGIN) {
            self->gesture_tracking_ = false;
            return;
        }
        if (code == LV_EVENT_PRESSING && self->gesture_tracking_ && indev != nullptr) {
            lv_point_t pos;
            lv_indev_get_point(indev, &pos);
            int dx = pos.x - self->gesture_start_.x;
            int dy = pos.y - self->gesture_start_.y;
            int adx = dx < 0 ? -dx : dx;
            // A deliberate downward pull: mostly vertical and past the
            // threshold. Scaling by 1.5 keeps a diagonal scroll from opening
            // the overlay.
            if (dy >= kPullOpenDistance && dy * 2 > adx * 3) {
                self->gesture_tracking_ = false;
                lv_indev_wait_release(indev);
                Dispatch(event, SettingsPage::Display);
            }
            return;
        }
        if (code == LV_EVENT_RELEASED && self->gesture_tracking_) {
            self->gesture_tracking_ = false;
            if (indev == nullptr) {
                return;
            }
            lv_point_t end;
            lv_indev_get_point(indev, &end);
            int dx = end.x - self->gesture_start_.x;
            int dy = end.y - self->gesture_start_.y;
            int adx = dx < 0 ? -dx : dx;
            // Fallback for firmwares where PRESSING is not emitted for a short
            // pull. wait_release cannot cancel this release's click, but the
            // overlay's own IsOpen() guard keeps the page that was opened here.
            if (dy >= kPullOpenDistance && dy * 2 > adx * 3) {
                lv_indev_wait_release(indev);
                Dispatch(event, SettingsPage::Wifi);
            }
        }
    }

    OpenSettingsCallback open_settings_;
    std::function<int()> face_toggle_;
    std::function<void()> on_theme_changed_;
    lv_obj_t* network_hotspot_ = nullptr;
    lv_obj_t* status_actions_ = nullptr;
    lv_obj_t* settings_icon_ = nullptr;
    lv_obj_t* volume_icon_ = nullptr;
    lv_obj_t* face_icon_ = nullptr;
    lv_point_t gesture_start_ = {};
    bool gesture_tracking_ = false;
};

class ReTerminalD1001Board : public WifiBoard {
private:
    ReTerminalD1001Expander expander_;
    ReTerminalD1001AudioCodec* audio_codec_ = nullptr;
    Button boot_button_;
    ReTerminalD1001Display* display_ = nullptr;
    std::unique_ptr<SettingsUi> settings_ui_;
    std::unique_ptr<PushPanel> push_panel_;
    std::unique_ptr<ReTerminalD1001BatteryMonitor> battery_;
    std::unique_ptr<FaceService> face_service_;
    PowerSaveTimer* power_save_timer_ = nullptr;
    i2c_master_bus_handle_t touch_i2c_bus_ = nullptr;
    gsl3670_driver_config_t touch_driver_config_ = {};
    D1001Camera* camera_ = nullptr;
    std::atomic<bool> powering_off_{false};
    bool power_off_armed_ = false;

    void EnableDsiPhyPower() {
        // The MIPI DSI PHY is fed by LDO3; without it the PHY stays in the
        // "no power" state and the DSI bus cannot be created.
        static esp_ldo_channel_handle_t phy_pwr_chan = nullptr;
        esp_ldo_channel_config_t ldo_cfg = {
            .chan_id = DISPLAY_MIPI_DSI_PHY_LDO_CHANNEL,
            .voltage_mv = DISPLAY_MIPI_DSI_PHY_LDO_VOLTAGE_MV,
        };
        ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo_cfg, &phy_pwr_chan));
        ESP_LOGI(TAG, "MIPI DSI PHY powered on");
    }

    void InitializeMipiDisplay(const RotationProfile& rotation) {
        EnableDsiPhyPower();
        expander_.PowerUpDisplayRails();

        esp_lcd_dsi_bus_handle_t mipi_dsi_bus = nullptr;
        esp_lcd_dsi_bus_config_t bus_config = {
            .bus_id = 0,
            .num_data_lanes = DISPLAY_MIPI_DSI_LANE_NUM,
            .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
            .lane_bit_rate_mbps = DISPLAY_MIPI_DSI_LANE_BITRATE_MBPS,
        };
        ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus));

        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_dbi_io_config_t dbi_config = {
            .virtual_channel = 0,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &panel_io));

        esp_lcd_dpi_panel_config_t dpi_config = {
            .virtual_channel = 0,
            .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
            .dpi_clock_freq_mhz = DISPLAY_DPI_CLOCK_MHZ,
            .in_color_format = LCD_COLOR_FMT_RGB565,
            .out_color_format = LCD_COLOR_FMT_RGB565,
            .num_fbs = 1,
            .video_timing =
                {
                    .h_size = DISPLAY_WIDTH,
                    .v_size = DISPLAY_HEIGHT,
                    .hsync_pulse_width = DISPLAY_HSYNC_PULSE_WIDTH,
                    .hsync_back_porch = DISPLAY_HSYNC_BACK_PORCH,
                    .hsync_front_porch = DISPLAY_HSYNC_FRONT_PORCH,
                    .vsync_pulse_width = DISPLAY_VSYNC_PULSE_WIDTH,
                    .vsync_back_porch = DISPLAY_VSYNC_BACK_PORCH,
                    .vsync_front_porch = DISPLAY_VSYNC_FRONT_PORCH,
                },
        };

        jd9365_vendor_config_t vendor_config = {
            .init_cmds = lcd_init_cmds,
            .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
            .mipi_config =
                {
                    .dsi_bus = mipi_dsi_bus,
                    .dpi_config = &dpi_config,
                    .lane_num = DISPLAY_MIPI_DSI_LANE_NUM,
                },
        };
        // Field order differs between IDF 5.5 and 6.0, so assign instead of
        // using designated initializers (-Wpedantic rejects out-of-order ones).
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;  // reset is behind the PCA9535
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        panel_config.vendor_config = &vendor_config;

        esp_lcd_panel_handle_t panel = nullptr;
        ESP_ERROR_CHECK(esp_lcd_new_panel_jd9365(panel_io, &panel_config, &panel));

        // The panel reset line hangs off the expander, so the driver cannot
        // pulse it itself: do it here before the init sequence is sent.
        expander_.ResetLcdPanel();

        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

        display_ = new ReTerminalD1001Display(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                              DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
                                              rotation.lcd_mirror_x, rotation.lcd_mirror_y,
                                              rotation.lcd_swap_xy);
        display_->SetOpenSettingsCallback([this](SettingsPage page) { OpenSettings(page); });
        display_->ApplyRotation(rotation.degrees);
        ESP_LOGI(TAG, "Display initialized, rotation=%d", rotation.degrees);
    }

    void InitializeTouch(const RotationProfile& rotation) {
        i2c_master_bus_config_t bus_config = {
            .i2c_port = TOUCH_I2C_PORT,
            .sda_io_num = TOUCH_I2C_SDA_PIN,
            .scl_io_num = TOUCH_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags =
                {
                    .enable_internal_pullup = 1,
                },
        };
        esp_err_t ret = i2c_new_master_bus(&bus_config, &touch_i2c_bus_);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create the touch I2C bus: %s", esp_err_to_name(ret));
            return;
        }

        esp_lcd_panel_io_handle_t touch_io = nullptr;
        esp_lcd_panel_io_i2c_config_t touch_io_config = ESP_LCD_TOUCH_IO_I2C_GSL3670_CONFIG();
        touch_io_config.scl_speed_hz = TOUCH_I2C_FREQ_HZ;
        ret = esp_lcd_new_panel_io_i2c(touch_i2c_bus_, &touch_io_config, &touch_io);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create the GSL3670 panel IO: %s", esp_err_to_name(ret));
            return;
        }

        // The touch reset line is on the PCA9535, so hand the driver a hook
        // instead of a GPIO number.
        touch_driver_config_.ctx = &expander_;
        touch_driver_config_.set_reset = [](void* ctx, bool asserted) {
            static_cast<ReTerminalD1001Expander*>(ctx)->SetTouchReset(asserted);
        };

        esp_lcd_touch_config_t touch_config = {
            .x_max = DISPLAY_WIDTH - 1,
            .y_max = DISPLAY_HEIGHT - 1,
            .rst_gpio_num = GPIO_NUM_NC,
            .int_gpio_num = GPIO_NUM_NC,  // the BSP polls the controller
            .levels =
                {
                    .reset = 0,
                    .interrupt = 0,
                },
            .flags =
                {
                    // x_max/y_max stay at the native panel size: the driver
                    // mirrors raw coordinates before the optional swap.
                    .swap_xy = rotation.touch_swap_xy,
                    .mirror_x = rotation.touch_mirror_x,
                    .mirror_y = rotation.touch_mirror_y,
                },
            .driver_data = &touch_driver_config_,
        };

        esp_lcd_touch_handle_t touch = nullptr;
        ret = esp_lcd_touch_new_i2c_gsl3670(touch_io, &touch_config, &touch);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize the GSL3670: %s", esp_err_to_name(ret));
            return;
        }

        const lvgl_port_touch_cfg_t lvgl_touch_config = {
            .disp = lv_display_get_default(),
            .handle = touch,
        };
        lv_indev_t* indev = lvgl_port_add_touch(&lvgl_touch_config);
        if (indev == nullptr) {
            ESP_LOGE(TAG, "Failed to register the touch panel with LVGL");
            return;
        }
        // The GSL3670 is polled; a scroll threshold of 18 absorbs finger jitter
        // on taps without turning them into unwanted scroll gestures.
        lv_indev_set_scroll_limit(indev, 18);
        // Poll the touch controller more often than the 30ms default so drags
        // track the finger.
        lv_timer_t* read_timer = lv_indev_get_read_timer(indev);
        if (read_timer != nullptr) {
            lv_timer_set_period(read_timer, 15);
        }
        // Any touch resets (and wakes) the power save timer.
        lv_indev_add_event_cb(
            indev,
            [](lv_event_t* event) {
                auto* self = static_cast<ReTerminalD1001Board*>(lv_event_get_user_data(event));
                if (self->power_save_timer_ != nullptr) {
                    self->power_save_timer_->WakeUp();
                }
            },
            LV_EVENT_PRESSED, this);
        ESP_LOGI(TAG, "Touch panel initialized");
    }

    void OpenSettings(SettingsPage page) {
        if (settings_ui_ == nullptr) {
            settings_ui_.reset(new SettingsUi(
                display_,
                [this](const std::string& ssid, const std::string& password) {
                    ConnectFromSettings(ssid, password);
                },
                [this]() { display_->SetStatusBarEntriesHidden(false); }));
            settings_ui_->SetIconFontProvider(
                [this](bool large) { return display_->GetIconFont(large); });
            settings_ui_->SetPowerSaveChangedCallback([this]() { ApplyPowerSaveConfig(); });
            settings_ui_->SetFaceCallbacks(
                [this](int& mode, std::string& endpoint) {
                    mode = face_service_ != nullptr ? face_service_->GetMode() : 0;
                    endpoint =
                        face_service_ != nullptr ? face_service_->GetEndpoint() : "";
                },
                [this](int mode, const std::string& endpoint) {
                    if (face_service_ != nullptr) {
                        face_service_->SetModeAndEndpoint(mode, endpoint);
                        face_service_->ReloadConfig();
                        display_->ApplyFaceIconState(face_service_->GetMode());
                    }
                });
            display_->SetOnThemeChanged([this]() {
                if (settings_ui_ != nullptr) {
                    settings_ui_->RefreshIconFonts();
                }
            });
        }
        if (settings_ui_->IsOpen()) {
            return;
        }
        display_->SetStatusBarEntriesHidden(true);
        settings_ui_->Open(page);
    }

    // Runs on a worker task: swap the credentials in, restart the station and
    // wait for the link. On failure the previous credentials are restored.
    void ConnectFromSettings(const std::string& ssid, const std::string& password) {
        auto* request = new ConnectRequest{this, ssid, password};
        BaseType_t ok = xTaskCreate(
            [](void* arg) {
                std::unique_ptr<ConnectRequest> request(static_cast<ConnectRequest*>(arg));
                request->board->RunConnect(*request);
                vTaskDelete(nullptr);
            },
            "d1001_wifi_set", 4096, request, 3, nullptr);
        if (ok != pdPASS) {
            delete request;
            settings_ui_->OnConnectResult(false, "无法启动连接任务");
        }
    }

    struct ConnectRequest {
        ReTerminalD1001Board* board;
        std::string ssid;
        std::string password;
    };

    void RunConnect(const ConnectRequest& request) {
        auto& ssid_manager = SsidManager::GetInstance();

        // Snapshot the previous entry so a failed attempt can be rolled back.
        bool existed = false;
        std::string old_password;
        for (const auto& item : ssid_manager.GetSsidList()) {
            if (item.ssid == request.ssid) {
                existed = true;
                old_password = item.password;
                break;
            }
        }
        ssid_manager.AddSsid(request.ssid, request.password);

        auto& wifi_manager = WifiManager::GetInstance();
        auto& app = Application::GetInstance();
        auto state = app.GetDeviceState();
        if (state == kDeviceStateIdle || state == kDeviceStateListening ||
            state == kDeviceStateSpeaking) {
            // Tear the protocol down before the link is taken away.
            EnterWifiConfigMode();
            for (int i = 0; i < 60 && !IsInWifiConfigMode(); i++) {
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        }

        ESP_LOGI(TAG, "Connecting to %s from the settings UI", request.ssid.c_str());
        wifi_manager.StartStation();

        bool success = false;
        for (int i = 0; i < 200; i++) {
            if (wifi_manager.IsConnected() && wifi_manager.GetSsid() == request.ssid) {
                success = true;
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        std::string message;
        if (success) {
            message = "已连接到 " + request.ssid + "\nIP: " + wifi_manager.GetIpAddress();
        } else {
            message = "无法连接到 " + request.ssid + "，请检查密码后重试";
            // Roll the credentials back so a wrong password is not kept.
            const auto& list = ssid_manager.GetSsidList();
            for (size_t i = 0; i < list.size(); i++) {
                if (list[i].ssid == request.ssid) {
                    ssid_manager.RemoveSsid((int)i);
                    break;
                }
            }
            if (existed) {
                ssid_manager.AddSsid(request.ssid, old_password);
            }
        }
        settings_ui_->OnConnectResult(success, message);
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            if (power_save_timer_ != nullptr) {
                power_save_timer_->WakeUp();
            }
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
        // The GPIO can read as "pressed" from the moment the button task
        // starts (seen on hardware: a phantom LONG_PRESS fired 3.6 s after
        // boot and powered the board off), so the power-off long press is
        // armed only after one real press/release cycle has been observed.
        boot_button_.OnPressUp([this]() {
            if (power_save_timer_ != nullptr) {
                power_save_timer_->WakeUp();
            }
            power_off_armed_ = true;
        });
        boot_button_.OnLongPress([this]() {
            if (!power_off_armed_) {
                ESP_LOGW(TAG, "Ignoring long press before the first release (boot-time phantom)");
                return;
            }
            StartPowerOff();
        });
    }

    // Runs in the button task, so the shutdown itself is handed to a short
    // worker: the power amplifier has to be muted before the rail drops or the
    // speaker pops, and that needs a delay the button task must not sit in.
    void StartPowerOff() {
        bool expected = false;
        if (!powering_off_.compare_exchange_strong(expected, true)) {
            return;
        }
        ESP_LOGW(TAG, "Long press on GPIO3: powering off");
        if (display_ != nullptr) {
            display_->ShowNotification("正在关机 ...", 10000);
        }
        BaseType_t ok = xTaskCreate(
            [](void* arg) {
                auto* board = static_cast<ReTerminalD1001Board*>(arg);
                board->expander_.SetPowerAmp(false);
                vTaskDelay(pdMS_TO_TICKS(500));
                board->expander_.PowerOff();
                vTaskDelay(pdMS_TO_TICKS(500));
                // Reached only when the board is kept alive from USB: the
                // PCA9535 power-hold bit does not cut an externally fed rail.
                // Restore the rails so the device is not left half dead and
                // silent (the PA and power hold were just dropped).
                ESP_LOGW(TAG, "Still running after PowerOff(); external power - restoring rails");
                board->expander_.SetPowerHold(true);
                if (board->audio_codec_ != nullptr && board->audio_codec_->output_enabled()) {
                    board->expander_.SetPowerAmp(true);
                }
                if (board->display_ != nullptr) {
                    board->display_->ShowNotification("外部供电，无法关机", 3000);
                }
                board->powering_off_ = false;
                vTaskDelete(nullptr);
            },
            "d1001_poweroff", 3072, this, 5, nullptr);
        if (ok != pdPASS) {
            powering_off_ = false;
        }
    }

    void InitializeCamera() {
        if (touch_i2c_bus_ == nullptr) {
            ESP_LOGE(TAG, "camera skipped: I2C0 is unavailable");
            return;
        }

        expander_.PowerUpCamera();

        // esp_video aborts the whole init when no sensor answers, so probe the
        // SCCB address first: that keeps a missing or unpowered module from
        // looking like a driver bug, and lets GetCamera() stay null.
        esp_err_t ret = i2c_master_probe(touch_i2c_bus_, CAMERA_SCCB_ADDRESS, 100);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "no camera sensor at SCCB 0x%02x: %s", CAMERA_SCCB_ADDRESS,
                     esp_err_to_name(ret));
            expander_.PowerDownCamera();
            return;
        }
        ESP_LOGI(TAG, "camera sensor found at SCCB 0x%02x", CAMERA_SCCB_ADDRESS);

        // The sensor shares I2C0 with the GSL3670 touch controller, so hand
        // esp_video the existing bus handle instead of creating a second
        // master on the same pins. Reset and power-down live on the PCA9535,
        // so there is no GPIO for esp_video to drive.
        esp_video_init_csi_config_t csi_config = {
            .sccb_config =
                {
                    .init_sccb = false,
                    .i2c_handle = touch_i2c_bus_,
                    .freq = CAMERA_SCCB_FREQ_HZ,
                },
            .reset_pin = GPIO_NUM_NC,
            .pwdn_pin = GPIO_NUM_NC,
        };
        esp_video_init_config_t camera_config = {
            .csi = &csi_config,
        };

        camera_ = new D1001Camera(camera_config);
        camera_->SetHMirror(false);
        camera_->SetVFlip(false);
        ESP_LOGI(TAG, "Camera initialized");
    }

public:
    // A 3 s hold powers the board down; a short press keeps its old meaning
    // (Wi-Fi config while starting, chat toggle afterwards).
    static constexpr uint16_t kPowerOffLongPressMs = 3000;

    // The Seeed BSP drives this button ACTIVE HIGH with the pull disabled
    // (esp32_p4_re_terminal_d1001.c: active_level = 1, disable_pull = true);
    // with the polarity inverted the idle line reads as "held" (the boot-time
    // phantom long press) and real presses read as releases.
    ReTerminalD1001Board() : boot_button_(BOOT_BUTTON_GPIO, true, kPowerOffLongPressMs, 100) {
        ESP_LOGI(TAG, "initializing reTerminal D1001");

        // Minimal power bring-up: I2C1 + PCA9535 and the system power hold.
        // The power amplifier stays off until the audio path is ready
        // (pop-free order) and the panel rails come up with the display.
        expander_.Initialize();
        expander_.ApplyMinimalPowerSequence();

        battery_.reset(new ReTerminalD1001BatteryMonitor(&expander_));
        if (!battery_->Initialize()) {
            battery_.reset();
        }

        audio_codec_ = new ReTerminalD1001AudioCodec(expander_.GetI2cBus(), AUDIO_INPUT_SAMPLE_RATE,
                                                     AUDIO_OUTPUT_SAMPLE_RATE);
        audio_codec_->SetPowerAmpCallback([this](bool on) { expander_.SetPowerAmp(on); });

        RotationProfile rotation = SettingsUi::LoadRotationProfile();
        ESP_LOGI(TAG, "Screen rotation=%d", rotation.degrees);

        InitializeMipiDisplay(rotation);
        InitializeTouch(rotation);
        InitializeCamera();
        InitializeButtons();

        GetBacklight()->RestoreBrightness();

        ApplyPowerSaveConfig();
    }

    // (Re)builds the power save timer from Settings("power"): dim_s seconds
    // until the screen dims (0 = never), off_s seconds until shutdown
    // (0 = never). Called at boot and whenever the settings UI changes them.
    void ApplyPowerSaveConfig() {
        if (power_save_timer_ != nullptr) {
            power_save_timer_->SetEnabled(false);
            delete power_save_timer_;
            power_save_timer_ = nullptr;
        }
        Settings settings("power", false);
        int dim_s = settings.GetInt("dim_s", 60);
        int off_s = settings.GetInt("off_s", 0);
        if (dim_s == 0 && off_s == 0) {
            return;
        }
        power_save_timer_ = new PowerSaveTimer(-1, dim_s > 0 ? dim_s : -1,
                                               off_s > 0 ? off_s : -1);
        power_save_timer_->OnEnterSleepMode([this]() {
            GetBacklight()->SetBrightness(8);
        });
        power_save_timer_->OnExitSleepMode([this]() {
            GetBacklight()->RestoreBrightness();
        });
        power_save_timer_->OnShutdownRequest([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateIdle) {
                StartPowerOff();
            } else {
                if (power_save_timer_ != nullptr) {
                    power_save_timer_->WakeUp();
                }
            }
        });
        power_save_timer_->SetEnabled(true);
    }

    // The push panel's HTTP server needs lwip up, which only holds once the
    // network stack has been started.
    // Applies a user-configured static IP (NVS namespace "wifi": static_en,
    // static_ip/gw/mask/dns) to the station netif, replacing DHCP. Runs right
    // after the network stack starts, before an address is acquired.
    void ApplyStaticIpIfConfigured() {
        Settings settings("wifi", false);
        if (settings.GetInt("static_en", 0) == 0) {
            return;
        }
        std::string ip = settings.GetString("static_ip", "");
        std::string gw = settings.GetString("static_gw", "");
        std::string mask = settings.GetString("static_mask", "");
        std::string dns = settings.GetString("static_dns", "");
        esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (netif == nullptr || ip.empty() || gw.empty() || mask.empty()) {
            ESP_LOGW(TAG, "Static IP enabled but incomplete, keeping DHCP");
            return;
        }
        esp_netif_ip_info_t info = {};
        if (esp_netif_str_to_ip4(ip.c_str(), &info.ip) != ESP_OK ||
            esp_netif_str_to_ip4(gw.c_str(), &info.gw) != ESP_OK ||
            esp_netif_str_to_ip4(mask.c_str(), &info.netmask) != ESP_OK) {
            ESP_LOGW(TAG, "Invalid static IP config, keeping DHCP");
            return;
        }
        esp_netif_dhcpc_stop(netif);
        esp_netif_set_ip_info(netif, &info);
        esp_netif_dns_info_t dns_info = {};
        dns_info.ip.type = IPADDR_TYPE_V4;
        if (dns.empty() ||
            esp_netif_str_to_ip4(dns.c_str(), &dns_info.ip.u_addr.ip4) != ESP_OK) {
            dns_info.ip.u_addr.ip4 = info.gw;  // default DNS = gateway
        }
        esp_netif_set_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns_info);
        ESP_LOGI(TAG, "Static IP %s (gw %s) applied, DHCP disabled", ip.c_str(), gw.c_str());
    }

    virtual void StartNetwork() override {
        WifiBoard::StartNetwork();
        ApplyStaticIpIfConfigured();
        if (push_panel_ == nullptr && display_ != nullptr) {
#if !CONFIG_ESP_VIDEO_ENABLE_ISP_PIPELINE_CONTROLLER
            // Without esp_ipa there is no AE/AWB; program static indoor
            // defaults (tunable at runtime via POST /camera/tune).
            if (camera_ != nullptr) {
                CameraTuning tuning;
                tuning.exposure_pct = 60;
                tuning.gain_index = 62;  // ~3.9x analog gain
                tuning.red_milli = 1300;
                tuning.blue_milli = 1300;
                ApplyCameraTuning(tuning);
            }
#endif
            push_panel_.reset(new PushPanel(display_));
            push_panel_->SetBottomInsetProvider(
                [this]() -> int32_t { return display_->GetChatBarHeight(); });
            push_panel_->SetCameraHooks(
                [this](std::vector<uint8_t>& out) {
                    return camera_ != nullptr && camera_->CaptureToJpeg(out);
                },
                [](int exp_pct, int gain_index, int red_milli, int blue_milli) {
                    CameraTuning tuning;
                    tuning.exposure_pct = exp_pct;
                    tuning.gain_index = gain_index;
                    tuning.red_milli = red_milli;
                    tuning.blue_milli = blue_milli;
                    return ApplyCameraTuning(tuning);
                },
                [this](std::vector<uint8_t>& bytes, std::string& meta) {
                    if (camera_ == nullptr) {
                        return false;
                    }
                    D1001Camera::RawFrame info;
                    if (!camera_->CaptureRawCopy(bytes, info)) {
                        return false;
                    }
                    char text[48];
                    snprintf(text, sizeof(text), "%u %u 0x%08lx", info.width, info.height,
                             (unsigned long)info.v4l2_format);
                    meta = text;
                    return true;
                });
            if (camera_ != nullptr) {
                face_service_.reset(new FaceService(camera_));
                face_service_->Start();
                display_->SetFaceToggleCallback([this]() {
                    return face_service_ != nullptr ? face_service_->ToggleMode() : -1;
                });
                push_panel_->SetFaceHooks(
                    [this]() {
                        return face_service_ != nullptr ? face_service_->StatusJson()
                                                        : std::string("{}");
                    },
                    [this](const std::string& body, std::string* error) {
                        return face_service_ != nullptr &&
                               face_service_->ApplyConfigJson(body, error);
                    });
            }
            if (camera_ == nullptr) {
                display_->SetFaceIconVisible(false);
            }
            push_panel_->Start();
        }
    }

    virtual void SetPowerSaveLevel(PowerSaveLevel level) override {
        if (level != PowerSaveLevel::LOW_POWER) {
            if (power_save_timer_ != nullptr) {
                power_save_timer_->WakeUp();
            }
        }
        WifiBoard::SetPowerSaveLevel(level);
    }

    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        return battery_ != nullptr && battery_->GetLevel(level, charging, discharging);
    }


    virtual AudioCodec* GetAudioCodec() override { return audio_codec_; }

    virtual Display* GetDisplay() override { return display_; }

    virtual Camera* GetCamera() override { return camera_; }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }
};

DECLARE_BOARD(ReTerminalD1001Board);
