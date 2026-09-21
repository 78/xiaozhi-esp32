#include "dual_network_board.h"
#include "codecs/box_audio_codec.h"
#include "display/display.h"
#include "display/emote_display.h"
#include "display/lcd_display.h"
#include "esp_lcd_ili9341.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "lvgl_theme.h"

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <driver/gpio.h>
#include "power_save_timer.h"

#include <esp_lcd_touch_gt911.h>
#include <esp_lcd_touch_ft5x06.h>
#include <esp_lvgl_port.h>
#include <lvgl.h>

#include "esp32_camera.h"
#include "power_manager.h"

#include <nvs_flash.h>
#include "system_reset.h"

#include "mcp_server.h"
#include "settings.h"

#include <functional>

#define TAG "Chd-Esp32s3-Box-Board"

// Init ili9341 by custom cmd // 和st7789兼容
static const ili9341_lcd_init_cmd_t vendor_specific_init[] = {
    {0xC8, (uint8_t []){0xFF, 0x93, 0x42}, 3, 0},
    {0xC0, (uint8_t []){0x0E, 0x0E}, 2, 0},
    {0xC5, (uint8_t []){0xD0}, 1, 0},
    {0xC1, (uint8_t []){0x02}, 1, 0},
    {0xB4, (uint8_t []){0x02}, 1, 0},
    {0xE0, (uint8_t []){0x00, 0x03, 0x08, 0x06, 0x13, 0x09, 0x39, 0x39, 0x48, 0x02, 0x0a, 0x08, 0x17, 0x17, 0x0F}, 15, 0},
    {0xE1, (uint8_t []){0x00, 0x28, 0x29, 0x01, 0x0d, 0x03, 0x3f, 0x33, 0x52, 0x04, 0x0f, 0x0e, 0x37, 0x38, 0x0F}, 15, 0},

    {0xB1, (uint8_t []){00, 0x1B}, 2, 0},
    {0x36, (uint8_t []){0x08}, 1, 0},
    {0x3A, (uint8_t []){0x55}, 1, 0},
    {0xB7, (uint8_t []){0x06}, 1, 0},

    {0x11, (uint8_t []){0}, 0x80, 0},
    {0x29, (uint8_t []){0}, 0x80, 0},

    {0, (uint8_t []){0}, 0xff, 0},
};

class CustomLcdDisplay : public SpiLcdDisplay {
public:
    lv_obj_t* volume_slider_ = nullptr;
    lv_obj_t* setting_box_ = nullptr;
    using ClickHandler = std::function<void()>;

    CustomLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                       int width, int height, int offset_x, int offset_y,
                       bool mirror_x, bool mirror_y, bool swap_xy)
        : SpiLcdDisplay(panel_io, panel, width, height, offset_x, offset_y,
                        mirror_x, mirror_y, swap_xy) {}

    void SetClickHandler(ClickHandler handler) {
        click_handler_ = std::move(handler);
    }

    static void gesture_event_cb(lv_event_t * e) {
        auto* self = static_cast<CustomLcdDisplay*>(lv_event_get_user_data(e));// CustomLcdDisplay* self = (CustomLcdDisplay*)Board::GetInstance().GetDisplay();

        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
        if (dir == LV_DIR_NONE) {
            ESP_LOGE(TAG, "Gesture: dir == LV_DIR_NONE");
            return;
        }
        lv_point_t point;
        lv_indev_get_point(lv_indev_active(), &point);      // 获取手势大概位置
        ESP_LOGI(TAG, "Gesture: %d at x=%d, y=%d", dir, point.x, point.y);

        DisplayLockGuard lock(self);
        auto& app = Application::GetInstance();
        auto dev_state = app.GetDeviceState();
        if (lv_obj_is_valid(self->setting_box_)) {      // 优先处理设置栏
            if (dir != LV_DIR_BOTTOM) {
                lv_obj_delete(self->setting_box_);
                self->setting_box_ = NULL;
                return;
            }
        }
        else {
            if ((dir == LV_DIR_BOTTOM) && (dev_state == kDeviceStateIdle)) {
                self->create_setting_box();
                return;
            }
        }

        switch (dir) {
            case LV_DIR_LEFT:
                ESP_LOGI(TAG, "Swipe Left detected");   // 执行向左滑动的逻辑，如切换页面
                if (dev_state == kDeviceStateIdle) {
                }
                break;
            case LV_DIR_RIGHT:
                ESP_LOGI(TAG, "Swipe Right detected");  // 执行向右滑动的逻辑  , 退出

                break;
            case LV_DIR_TOP:
                ESP_LOGI(TAG, "Swipe Up detected");
                app.ToggleChatState();
                break;
            case LV_DIR_BOTTOM:
                ESP_LOGI(TAG, "Swipe Down detected");
                // display->set_volume_slider(output_volume_);
                break;
            default:
                break;
        }
    }

    static void volume_slider_cb(lv_event_t* e) {
        lv_obj_t * volume_slider_t = (lv_obj_t *)lv_event_get_target(e);     // volume_slider_
        auto* self = static_cast<CustomLcdDisplay*>(lv_event_get_user_data(e));
        DisplayLockGuard lock(self);
        int volume_value = lv_slider_get_value(volume_slider_t);
        auto audio_codec = Board::GetInstance().GetAudioCodec();
        if (audio_codec && (audio_codec->output_volume() != volume_value)) {
            audio_codec->SetOutputVolume(volume_value);
        }
    }

    void set_volume_slider(int volume_value) {                      // 同步音量图标
        DisplayLockGuard lock(this);
        ESP_LOGI(TAG, "set_volume_slider %d", volume_value);
        if (lv_obj_is_valid(volume_slider_)) {
            int slider_value = lv_slider_get_value(volume_slider_);
            ESP_LOGI(TAG, "lv_slider_get_value %d", slider_value);
            uint8_t value = 0;
            if (volume_value > 0) {
                if (volume_value > 100) {
                    value = 100;
                }
                else {
                    value = volume_value;
                }
            }
            if (value != slider_value) {
                ESP_LOGI(TAG, "set_volume_slider %d", value);
                lv_slider_set_value(volume_slider_, value, LV_ANIM_OFF);
            }
        }
    }

    virtual void SetupUI() override {
        ESP_LOGI(TAG, "SetupUI >>>>>>>>>");

        SpiLcdDisplay::SetupUI();   // Call parent SetupUI() first to create all lvgl objects

        DisplayLockGuard lock(this);
        auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
        auto text_font = lvgl_theme->text_font()->font();
        auto icon_font = lvgl_theme->icon_font()->font();
        auto screen = lv_screen_active();

        lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);                               // 将滚动条模式设置为 OFF，这样无论是否滚动，滚动条都不会显示
        lv_obj_set_scroll_dir(screen, LV_DIR_NONE);                                             // lv_obj_set_scroll_dir(screen, LV_DIR_HOR);
        lv_obj_add_event_cb(lv_scr_act(), gesture_event_cb, LV_EVENT_GESTURE, this);
    }

    static void aec_switch_event_cb(lv_event_t * e) {
        CustomLcdDisplay* instance = static_cast<CustomLcdDisplay*>(lv_event_get_user_data(e)); // 1. 获取用户数据（即 this 指针）
        if (!instance) return;
        lv_obj_t * sw = static_cast<lv_obj_t*>(lv_event_get_target(e));                         // 2. 获取触发事件的对象（开关本身）
        bool is_on = lv_obj_has_state(sw, LV_STATE_CHECKED);                                    // 3. 获取当前开关状态 // lv_obj_has_state 返回 true 表示处于 LV_STATE_CHECKED (打开)
        auto& app = Application::GetInstance();
        auto dev_state = app.GetDeviceState();
        if (dev_state == kDeviceStateIdle) {
            app.SetAecMode(is_on ? kAecOnDeviceSide : kAecOff);
            Settings settings("audio", true);
            settings.SetInt("aec_mode", app.GetAecMode());
        }
    }

    void create_setting_box(void) {      // 下滑，打开设置页面
        ESP_LOGW(TAG, "create_setting_box in");
        DisplayLockGuard lock(this);
        if (lv_obj_is_valid(setting_box_)) {
            ESP_LOGW(TAG, "create_setting_box already run");
            return;
        }

        auto& app = Application::GetInstance();

        auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
        auto text_font = lvgl_theme->text_font()->font();
        auto icon_font = lvgl_theme->icon_font()->font();

        setting_box_ = lv_obj_create(lv_screen_active());
        lv_obj_remove_style_all(setting_box_);

        lv_obj_set_style_bg_color(setting_box_, lv_color_hex(0xF5F5F5), 0);
        lv_obj_set_style_bg_opa(setting_box_, LV_OPA_90, 0);
        lv_obj_set_size(setting_box_, lv_pct(100), lv_pct(100));
        lv_obj_add_flag(setting_box_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(setting_box_, LV_DIR_VER);        // 竖向滚动
        lv_obj_set_style_pad_bottom(setting_box_, 40, 0);       // 底部预留40空间
        lv_obj_align(setting_box_, LV_ALIGN_CENTER, 0, 0);
#ifdef USE_LCD_2_8
        static const int32_t grid_col[] = {LV_GRID_FR(1), LV_GRID_FR(14), LV_GRID_FR(14), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
#else
        static const int32_t grid_col[] = {LV_GRID_FR(2), LV_GRID_FR(4), LV_GRID_FR(4), LV_GRID_FR(2), LV_GRID_TEMPLATE_LAST};
#endif
        static const int32_t grid_row[] = {80, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};     // 第一行高度固定80
        lv_obj_set_grid_dsc_array(setting_box_, grid_col, grid_row);

        int grid_row_id = 0;            // --- 开关 1: AEC 语音打断 ---     // 第一行
        lv_obj_t *label_set = lv_label_create(setting_box_);
        lv_obj_set_style_pad_all(label_set, 4, LV_PART_MAIN);
        lv_obj_set_style_pad_top(label_set, 6, LV_PART_MAIN);
        lv_obj_set_style_border_width(label_set, 2, 0);
        lv_obj_set_style_border_color(label_set, lv_color_hex(0x0000FF), 0);
        lv_obj_set_style_radius(label_set, 4, 0);
        lv_obj_set_style_text_font(label_set, text_font, LV_PART_MAIN);
        lv_label_set_text_static(label_set, "系统设置");
        lv_obj_set_grid_cell(label_set, LV_GRID_ALIGN_CENTER, 1, 2, LV_GRID_ALIGN_CENTER, grid_row_id++, 1);

        lv_obj_t *aec_box = lv_obj_create(setting_box_);
        lv_obj_set_size(aec_box, lv_pct(100), 50); // 宽度设为100%，自动占满父级单元格宽度
        lv_obj_set_style_bg_opa(aec_box, LV_OPA_TRANSP, 0);
        lv_obj_set_style_pad_all(aec_box, 0, 0);
        lv_obj_set_style_border_width(aec_box, 0, 0);
        lv_obj_remove_flag(aec_box, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(aec_box, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *label_aec = lv_label_create(aec_box);
        lv_obj_set_style_text_font(label_aec, text_font, LV_PART_MAIN);
        lv_label_set_text_static(label_aec, "语音打断");
        lv_obj_align(label_aec, LV_ALIGN_LEFT_MID, 0, 0);

        auto aec_switch_ = lv_switch_create(aec_box);   // 开关
        lv_obj_set_size(aec_switch_, 50, 25);
        lv_obj_set_ext_click_area(aec_switch_, 10);
        lv_obj_align(aec_switch_, LV_ALIGN_RIGHT_MID, 0, 0);
#if CONFIG_USE_DEVICE_AEC
        if (AecMode::kAecOff != app.GetAecMode()) {
            lv_obj_add_state(aec_switch_, LV_STATE_CHECKED);
        }
        lv_obj_add_event_cb(aec_switch_, aec_switch_event_cb, LV_EVENT_VALUE_CHANGED, this);
#endif
        lv_obj_set_grid_cell(aec_box, LV_GRID_ALIGN_STRETCH,  // 替换原来的LV_GRID_ALIGN_CENTER，自动拉伸填满单元格宽度
            1, 2, LV_GRID_ALIGN_CENTER, grid_row_id++, 1);

        // 第二行：音量控制
        auto volume_box_ = lv_obj_create(setting_box_);
        lv_obj_set_size(volume_box_, lv_pct(100), 50);
        lv_obj_set_style_bg_opa(volume_box_, LV_OPA_TRANSP, 0);
        lv_obj_set_style_pad_all(volume_box_, 0, 0);
        lv_obj_set_style_border_width(volume_box_, 0, 0);
        lv_obj_clear_flag(volume_box_, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(volume_box_, LV_OBJ_FLAG_GESTURE_BUBBLE);

        lv_obj_t *lab_vol_min = lv_label_create(volume_box_);
        lv_obj_set_style_text_font(lab_vol_min, text_font, LV_PART_MAIN);
        lv_label_set_text_static(lab_vol_min, "音量");
        lv_obj_align(lab_vol_min, LV_ALIGN_LEFT_MID, 0, 0);

        volume_slider_ = lv_slider_create(volume_box_);
        lv_obj_set_size(volume_slider_, lv_pct(75), 10);
        lv_obj_set_ext_click_area(volume_slider_, 15);
        lv_obj_align_to(volume_slider_, lab_vol_min, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
        lv_slider_set_range(volume_slider_, 0, 100);
        int volume_value = 70;
        auto audio_codec = Board::GetInstance().GetAudioCodec();
        if (audio_codec) {
            volume_value = audio_codec->output_volume();
        }
        lv_slider_set_value(volume_slider_, volume_value, LV_ANIM_OFF);
        lv_obj_add_event_cb(volume_slider_, volume_slider_cb, LV_EVENT_VALUE_CHANGED, this);

        lv_obj_t *lab_vol_max = lv_label_create(volume_box_);
        lv_obj_set_style_text_font(lab_vol_max, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_label_set_text_static(lab_vol_max, LV_SYMBOL_VOLUME_MAX);
        lv_obj_align(lab_vol_max, LV_ALIGN_RIGHT_MID, 0, 0);

        lv_obj_set_grid_cell(volume_box_, LV_GRID_ALIGN_STRETCH, 1, 2, LV_GRID_ALIGN_CENTER, grid_row_id++, 1);

        // ================= 第三行：亮度控制 =================
        auto brightness_box_ = lv_obj_create(setting_box_);
        lv_obj_set_size(brightness_box_, lv_pct(100), 50);
        lv_obj_set_style_bg_opa(brightness_box_, LV_OPA_TRANSP, 0);
        lv_obj_set_style_pad_all(brightness_box_, 0, 0);
        lv_obj_set_style_border_width(brightness_box_, 0, 0);
        lv_obj_clear_flag(brightness_box_, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(brightness_box_, LV_OBJ_FLAG_GESTURE_BUBBLE);

        lv_obj_t *lab_bright_min = lv_label_create(brightness_box_);
        lv_obj_set_style_text_font(lab_bright_min, text_font, LV_PART_MAIN);
        lv_label_set_text_static(lab_bright_min, "亮度");
        lv_obj_align(lab_bright_min, LV_ALIGN_LEFT_MID, 0, 0);

        auto brightness_slider_ = lv_slider_create(brightness_box_);
        lv_obj_set_size(brightness_slider_, lv_pct(75), 10);
        lv_obj_set_ext_click_area(brightness_slider_, 15);
        lv_obj_align_to(brightness_slider_, lab_bright_min, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
        lv_slider_set_range(brightness_slider_, 1, 100);

        auto brightness = Board::GetInstance().GetBacklight()->brightness();
        lv_slider_set_value(brightness_slider_, brightness, LV_ANIM_OFF);
        lv_obj_add_event_cb(brightness_slider_, [](lv_event_t* e) {
            auto* self = static_cast<CustomLcdDisplay*>(lv_event_get_user_data(e));
            lv_obj_t * slider_ = (lv_obj_t *)lv_event_get_target(e);
            if (self) {
                int slider_value_ = lv_slider_get_value(slider_);
                DisplayLockGuard lock(self);
                Board::GetInstance().GetBacklight()->SetBrightness(slider_value_);
            }
        }, LV_EVENT_VALUE_CHANGED, this);

        lv_obj_t *lab_bright_max = lv_label_create(brightness_box_);
        lv_obj_set_style_text_font(lab_bright_max, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_label_set_text_static(lab_bright_max, LV_SYMBOL_PLUS);
        lv_obj_align(lab_bright_max, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_set_grid_cell(brightness_box_, LV_GRID_ALIGN_STRETCH, 1, 2, LV_GRID_ALIGN_CENTER, grid_row_id++, 1);

        ESP_LOGW(TAG, "create_setting_box out");
    }

private:
    ClickHandler click_handler_;
};

class ChdEsp32s3BoxBoard : public DualNetworkBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    Button boot_button_;
    Display* display_;
    CustomLcdDisplay* touch_display_;
    Esp32Camera* camera_;
    PowerManager* power_manager_;
    PowerSaveTimer* power_save_timer_;

    void InitializePowerSaveTimer() {
        power_save_timer_ = new PowerSaveTimer(-1, 180, -1);
        power_save_timer_->OnEnterSleepMode([this]() {
            GetDisplay()->SetPowerSaveMode(true);
            GetBacklight()->SetBrightness(20); });
        power_save_timer_->OnExitSleepMode([this]() {
            GetDisplay()->SetPowerSaveMode(false);
            GetBacklight()->RestoreBrightness(); });
        power_save_timer_->SetEnabled(true);
    }

    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)1,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_GPIO;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_CLK_GPIO;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void ResetNvsFlash() {
        ESP_LOGI(TAG, "Resetting NVS flash");
        esp_err_t ret = nvs_flash_erase();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to erase NVS flash");
        }
        ret = nvs_flash_init();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize NVS flash");
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        esp_restart();
    }

    void HandlePrimaryButtonClick() {
        auto& app = Application::GetInstance();
        if (GetNetworkType() == NetworkType::WIFI) {
            if (app.GetDeviceState() == kDeviceStateStarting) {
                auto& wifi_board = static_cast<WifiBoard&>(GetCurrentBoard());
                wifi_board.EnterWifiConfigMode();
                return;
            }
        }
        app.ToggleChatState();
        power_save_timer_->WakeUp();
    }

    void HandlePrimaryButtonDoubleClick() {
        auto& app = Application::GetInstance();
#if CONFIG_USE_DEVICE_AEC
        if (app.GetDeviceState() == kDeviceStateIdle) {
            app.SetAecMode(app.GetAecMode() == kAecOff ? kAecOnDeviceSide : kAecOff);
        }
#endif
        if (app.GetDeviceState() == kDeviceStateStarting || app.GetDeviceState() == kDeviceStateWifiConfiguring) {
            SwitchNetworkType();
        }
    }

    void HandlePrimaryButtonMultipleClick() {
        ESP_LOGD(TAG, "OnMultipleClick");
        auto& app = Application::GetInstance();
        if (app.GetDeviceState() == kDeviceStateWifiConfiguring) {      // 测试相机，测试完要重启一次
            if (camera_&& camera_->Capture()) {
                ESP_LOGI(TAG, "chd camera capture OK");
            }
            else {
                ESP_LOGE(TAG, "chd camera capture ERROR");
            }
        }
        else {
            ResetNvsFlash();
        }
        ESP_LOGD(TAG, "OnMultipleClick");
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() { HandlePrimaryButtonClick(); });
        boot_button_.OnDoubleClick([this]() { HandlePrimaryButtonDoubleClick(); });
        boot_button_.OnMultipleClick([this]() { HandlePrimaryButtonMultipleClick(); }, 5);
    }

    void InitializeDisplay() {
        esp_lcd_panel_io_handle_t   panel_io = nullptr;
        esp_lcd_panel_handle_t      panel = nullptr;

        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_GPIO;
        io_config.dc_gpio_num = DISPLAY_DC_GPIO;
        io_config.spi_mode    = 0;
        io_config.pclk_hz     = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits      = 8;
        io_config.lcd_param_bits    = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));

        Settings settings("chd_esp_box3", false); // 横屏模式1 竖屏0
        bool display_mode = static_cast<bool>(settings.GetInt("display_mode", 1));
        int width = DISPLAY_WIDTH;
        int height = DISPLAY_HEIGHT;
        bool mirror_x = DISPLAY_MIRROR_X;
        bool mirror_y = DISPLAY_MIRROR_Y;
        bool swap_xy = DISPLAY_SWAP_XY;
        if (display_mode) {         // 横屏模式
            width = DISPLAY_HEIGHT;
            height = DISPLAY_WIDTH;
            mirror_x = mirror_x ? false : true;
            // mirror_y = DISPLAY_MIRROR_Y;
            swap_xy = swap_xy ? false : true;
        }

        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
        esp_lcd_panel_swap_xy(panel, swap_xy);
        esp_lcd_panel_mirror(panel, mirror_x, mirror_y);
        esp_lcd_panel_disp_on_off(panel, true);

        touch_display_ = new CustomLcdDisplay(panel_io, panel,
            width, height, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, mirror_x, mirror_y, swap_xy);
        display_ = touch_display_;
        touch_display_->SetClickHandler([this]() { HandlePrimaryButtonClick(); });
    }

    void InitializeTouch_gt911() {
        esp_lcd_touch_handle_t tp;
        Settings settings("chd_esp_box3", false); // 横屏模式1 竖屏0
        bool display_mode = static_cast<bool>(settings.GetInt("display_mode", 1));
        uint16_t width = DISPLAY_WIDTH;
        uint16_t height = DISPLAY_HEIGHT;

    #ifdef USE_LCD_3_5                   // 90度，竖屏
        bool mirror_x = DISPLAY_MIRROR_X ? false : true;
        bool mirror_y = DISPLAY_MIRROR_Y;
        bool swap_xy = DISPLAY_SWAP_XY;
    #else
        bool mirror_x = DISPLAY_MIRROR_X;
        bool mirror_y = DISPLAY_MIRROR_Y;
        bool swap_xy = DISPLAY_SWAP_XY ? false : true;
    #endif
        if (display_mode) {         // 横屏模式
            width = DISPLAY_HEIGHT;
            height = DISPLAY_WIDTH;
            mirror_x = mirror_x ? false : true;
            mirror_y = mirror_y;
            swap_xy = swap_xy ? false : true;
        }

        esp_lcd_touch_config_t tp_cfg = {
            .x_max = width,
            .y_max = height,
            .rst_gpio_num = GPIO_NUM_NC, // Shared with LCD reset
            .int_gpio_num = GPIO_NUM_NC,
            .levels = {
                .reset = 0,
                .interrupt = 0,
            },
            .flags = {
                .swap_xy = swap_xy,
                .mirror_x = mirror_x,
                .mirror_y = mirror_y,
            },
        };

        esp_lcd_panel_io_handle_t tp_io_handle = NULL;
        esp_lcd_panel_io_i2c_config_t tp_io_config = {
            .dev_addr = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS,
            .control_phase_bytes = 1,
            .dc_bit_offset = 0,
            .lcd_cmd_bits = 16,
            .flags = {
                .disable_control_phase = 1,
            }
	    };

        tp_io_config.scl_speed_hz = 400 * 1000;
        ESP_LOGI(TAG, "Initialize touch controller");

        esp_err_t ret = esp_lcd_new_panel_io_i2c(i2c_bus_, &tp_io_config, &tp_io_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create I2C panel IO: %s", esp_err_to_name(ret));
            return;
        }

        ret = esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &tp);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create GT911 touch controller: %s", esp_err_to_name(ret));
            return;
        }
        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = lv_display_get_default(),
            .handle = tp,
        };
        lvgl_port_add_touch(&touch_cfg);
        ESP_LOGI(TAG, "Touch panel initialized successfully");
    }

    void InitializeTouch_ft6336(void) {
        esp_lcd_touch_handle_t tp;
        esp_lcd_touch_config_t tp_cfg = {
            .x_max = DISPLAY_WIDTH,
            .y_max = DISPLAY_HEIGHT,
            .rst_gpio_num = GPIO_NUM_NC, // Shared with LCD reset
            .int_gpio_num = GPIO_NUM_NC,
            .levels = {
                .reset = 0,
                .interrupt = 0,
            },
            .flags = {
                .swap_xy = 1,
                .mirror_x = 1,
                .mirror_y = 0,
            },
        };
        esp_lcd_panel_io_handle_t tp_io_handle = NULL;
        esp_lcd_panel_io_i2c_config_t tp_io_config = {
            .dev_addr = ESP_LCD_TOUCH_IO_I2C_FT5x06_ADDRESS,
            .control_phase_bytes = 1,
            .dc_bit_offset = 0,
            .lcd_cmd_bits = 8,
            .flags = { .disable_control_phase = 1,}
        };
        tp_io_config.scl_speed_hz = 400000;

        esp_lcd_new_panel_io_i2c(i2c_bus_, &tp_io_config, &tp_io_handle);
        esp_lcd_touch_new_i2c_ft5x06(tp_io_handle, &tp_cfg, &tp);
        assert(tp);

        /* Add touch input (for selected screen) */
        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = lv_display_get_default(),
            .handle = tp,
        };

        if(touch_cfg.disp) {
            lvgl_port_add_touch(&touch_cfg);
        } else {
            ESP_LOGE(TAG, "Touch display is not initialized");
        }
    }

    void InitializeTouch() {
        esp_err_t i2c_ret = i2c_master_probe(i2c_bus_, ESP_LCD_TOUCH_IO_I2C_FT5x06_ADDRESS, 50);
        if (i2c_ret == ESP_OK) {
            ESP_LOGI(TAG, "Touch panel ft6336");
            InitializeTouch_ft6336();
        } else {
            i2c_ret = i2c_master_probe(i2c_bus_, ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS, 50);
            if (i2c_ret == ESP_OK) {
                ESP_LOGI(TAG, "Touch panel gt911");
                InitializeTouch_gt911();
            } else {
                ESP_LOGI(TAG, "no touch panel");
            }
        }
    }

    void InitializeCamera() {
        camera_config_t camera_config = {
            .pin_pwdn       = CAMERA_PIN_PWDN,
            .pin_reset      = CAMERA_PIN_RESET,
            .pin_xclk       = CAMERA_PIN_XCLK,
            .pin_sscb_sda   = BSP_I2C_SDA,
            .pin_sscb_scl   = BSP_I2C_SCL,

            .pin_d7         = CAMERA_PIN_D7,
            .pin_d6         = CAMERA_PIN_D6,
            .pin_d5         = CAMERA_PIN_D5,
            .pin_d4         = CAMERA_PIN_D4,
            .pin_d3         = CAMERA_PIN_D3,
            .pin_d2         = CAMERA_PIN_D2,
            .pin_d1         = CAMERA_PIN_D1,
            .pin_d0         = CAMERA_PIN_D0,
            .pin_vsync      = CAMERA_PIN_VSYNC,
            .pin_href       = CAMERA_PIN_HREF,
            .pin_pclk       = CAMERA_PIN_PCLK,

            .xclk_freq_hz   = CAMERA_XCLK_FREQ,
            .ledc_timer     = LEDC_TIMER_2,
            .ledc_channel   = LEDC_CHANNEL_0,
            .pixel_format   = PIXFORMAT_RGB565,
            .frame_size     = FRAMESIZE_VGA,         // FRAMESIZE_240X240,

            // .pixel_format = PIXFORMAT_JPEG,     // ov3660摄像头
            // .frame_size   = FRAMESIZE_QXGA,     // 2048x1536 FRAMESIZE_UXGA,     // 1600x1200  FRAMESIZE_UXGA,     // 1600x1200        FRAMESIZE_QVGA,     // 320x240
            // .jpeg_quality = 8,
            .fb_count     = 1,
            .fb_location  = CAMERA_FB_IN_PSRAM,
            .grab_mode    = CAMERA_GRAB_WHEN_EMPTY,
            .sccb_i2c_port = BSP_I2C_PORT,
        };

        camera_ = new Esp32Camera(camera_config);
        sensor_t* s = esp_camera_sensor_get();
        if (s == NULL) {
            ESP_LOGE(TAG, "chd camera esp_camera_sensor_get ERROR");
            return;
        }

        if (s->id.PID == OV3660_PID) {
            s->set_brightness(s, 1);   // up the blightness just a bit
            s->set_saturation(s, -2);  // lower the saturation
        }

        Settings settings("chd_esp_box3", false); // 考虑有的批次摄像头需要翻转
        bool camera_flipped = static_cast<bool>(settings.GetInt("camera-flipped", 0));
        camera_->SetHMirror(camera_flipped);
        camera_->SetVFlip(camera_flipped);

        camera_fb_t *fb = esp_camera_fb_get(); // 第一帧的数据不使用
        if (fb) {
            ESP_LOGI(TAG, "chd camera fb get OK");
            esp_camera_fb_return(fb);
        }
        else {
            ESP_LOGE(TAG, "chd camera fb get ERROR");
        }
    }

    void InitializePowerManager() {
        power_manager_ = new PowerManager(BAT_CHARGING_PIN);
        power_manager_->OnChargingStatusChanged([this](bool is_charging) {
            if (is_charging) {
                // power_save_timer_->SetEnabled(false);
            } else {
                // power_save_timer_->SetEnabled(true);
            }
        });
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeTools() {
        auto& mcp_server = McpServer::GetInstance();    // 定义设备的属性
        mcp_server.AddTool("self.camera.set_camera_flipped", "翻转摄像头图像方向", PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
            Settings settings("chd_esp_box3", true);
            bool flipped = !static_cast<bool>(settings.GetInt("camera-flipped", 1));
            camera_->SetHMirror(flipped);
            camera_->SetVFlip(flipped);
            settings.SetInt("camera-flipped", flipped ? 1 : 0);
            return true;
        });

        mcp_server.AddTool(
            "self.AEC.set_mode",
            "设置AEC语音打断模式。当用户意图切换语音打断模式时或者用户觉得ai对话容易被打断时或者用户觉得无法实现对话打断时都使用此工具。\n"
            "参数：\n"
            "   `mode`: 语音打断模式，可选值只有`kAecOff`(关闭）和`kAecOnDeviceSide`（开启）\n"
            "返回值：\n"
            "   反馈状态信息，不需要确认，立即播报相关数据\n",
            PropertyList({
                Property("mode", kPropertyTypeString)
            }),
            [](const PropertyList& properties) -> ReturnValue {
                auto mode = properties["mode"].value<std::string>();
                auto& app = Application::GetInstance();
                vTaskDelay(pdMS_TO_TICKS(2000));
                if (mode == "kAecOff") {
                    app.SetAecMode(kAecOff);
                    return "{\"success\": true, \"message\": \"AEC对话打断模式已关闭\"}";
                }else {
                    auto& board = Board::GetInstance();
                    app.SetAecMode(kAecOnDeviceSide);

                    return "{\"success\": true, \"message\": \"AEC对话打断模式已开启\"}";
                }
            }
        );

        mcp_server.AddTool(
            "self.AEC.get_mode",
            "获取AEC语音打断模式状态。当用户意图获取语音打断模式状态时使用此工具。\n"
            "返回值：\n"
            "   反馈状态信息，不需要确认，立即播报相关数据\n",
            PropertyList(),
            [](const PropertyList&) -> ReturnValue {
                auto& app = Application::GetInstance();
                const bool is_currently_off = (app.GetAecMode() == kAecOff);
            if (is_currently_off) {
                    return "{\"success\": true, \"message\": \"AEC语音打断模式处于关闭状态\"}";
                }else {
                    return "{\"success\": true, \"message\": \"AEC语音打断模式处于开启状态\"}";
                }
            }
        );
        auto& app = Application::GetInstance();
        if (GetNetworkType() == NetworkType::WIFI) {
            mcp_server.AddTool("self.system.reconfigure_wifi",
                "End this conversation and enter WiFi configuration mode.\n"
                "**CAUTION** You must ask the user to confirm this action.",
                PropertyList(), [this](const PropertyList& properties) {
                    auto& wifi_board = static_cast<WifiBoard&>(GetCurrentBoard());
                    wifi_board.EnterWifiConfigMode();
                    return true;
                });
        }

        mcp_server.AddTool("self.screen.set_display_mode",
            "Set display mode to landscape or portrait: 0=portrait, 1=landscape\n"
            "Set display mode to landscape, display_mode = 1\n"
            "Set display mode to portrait, display_mode = 0\n"
            "最后提示：重新开机生效",
            PropertyList({
                Property("display_mode", kPropertyTypeInteger, 0, 1)
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                int is_landscape = static_cast<bool>(properties["display_mode"].value<int>());
                Settings settings("chd_esp_box3", true);
                settings.SetInt("display_mode", is_landscape ? 1 : 0);
                ESP_LOGE(TAG, "set display_mode %d changed, restart to apply", is_landscape ? 1 : 0);
                return true;
            }
        );
        mcp_server.AddTool("self.screen.get_display_mode",
            "Get display mode: 0=portrait, 1=landscape",
            PropertyList(),
            [this](const PropertyList& properties) -> ReturnValue {
                Settings settings("chd_esp_box3", false);
                int display_mode = static_cast<int>(settings.GetInt("display_mode", 1));
                return display_mode;
            }
        );
    }

public:
    ChdEsp32s3BoxBoard() : DualNetworkBoard(ML307_TX_PIN, ML307_RX_PIN, GPIO_NUM_NC, 0),
        boot_button_(BOOT_BUTTON_GPIO), display_(nullptr), touch_display_(nullptr) {
        InitializeI2c();
        InitializeSpi();
        InitializeDisplay();
        InitializePowerSaveTimer();
        InitializeButtons();
        InitializeTouch();
        InitializeCamera();
        InitializePowerManager();
        InitializeTools();
        GetBacklight()->RestoreBrightness();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static BoxAudioCodec audio_codec(
            i2c_bus_,
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK,
            AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN,
            AUDIO_CODEC_ES8311_ADDR,
            AUDIO_CODEC_ES7210_ADDR,
            AUDIO_INPUT_REFERENCE);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    virtual Camera* GetCamera() override {
        return camera_;
    }

    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        static bool last_discharging = false;
        charging = power_manager_->IsCharging();
        discharging = power_manager_->IsDischarging();
        if (discharging != last_discharging) {
            power_save_timer_->SetEnabled(discharging);
            last_discharging = discharging;
        }
        level = power_manager_->GetBatteryLevel();
        return true;
    }

    virtual void SetPowerSaveLevel(PowerSaveLevel level) override {
        if (level != PowerSaveLevel::LOW_POWER) {
            power_save_timer_->WakeUp();
        }
        DualNetworkBoard::SetPowerSaveLevel(level);
    }
};

DECLARE_BOARD(ChdEsp32s3BoxBoard);
