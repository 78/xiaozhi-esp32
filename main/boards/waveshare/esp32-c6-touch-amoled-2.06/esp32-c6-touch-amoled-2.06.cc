#include "wifi_board.h"
#include "display/lcd_display.h"
#include "esp_lcd_sh8601.h"

#include "codecs/es8311_audio_codec.h"
#include "application.h"
#include "button.h"
#include "led/single_led.h"
#include "mcp_server.h"
#include "config.h"
#include "power_save_timer.h"
#include "axp2101.h"
#include "i2c_device.h"
#include "lvgl_theme.h"
#include "esp_io_expander_tca9554.h"

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_timer.h>
#include <cJSON.h>
#include <driver/i2c_master.h>
#include <driver/spi_master.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include "settings.h"
#include <font_awesome.h>
#include <wifi_manager.h>
#include <ssid_manager.h>

#include <esp_lcd_touch_cst9217.h>
#include <esp_lcd_touch_cst816s.h>
#include <esp_lcd_touch_ft5x06.h>
#include <esp_lvgl_port.h>
#include <lvgl.h>

#include <cstring>
#include <ctime>
#include <cctype>
#include <cstdint>
#include <algorithm>
#include <functional>
#include <string>
#include <vector>

#define TAG "WaveshareEsp32c6TouchAMOLED2inch06"

class Pmic : public Axp2101 {
public:
    Pmic(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : Axp2101(i2c_bus, addr) {
        WriteReg(0x22, 0b110); // PWRON > OFFLEVEL as POWEROFF Source enable
        WriteReg(0x27, 0x10);  // hold 4s to power off

        // Disable All DCs but DC1
        WriteReg(0x80, 0x01);
        // Disable All LDOs
        WriteReg(0x90, 0x00);
        WriteReg(0x91, 0x00);

        // Set DC1 to 3.3V
        WriteReg(0x82, (3300 - 1500) / 100);

        // Set ALDO1 to 3.3V
        WriteReg(0x92, (3300 - 500) / 100);
        WriteReg(0x93, (3300 - 500) / 100);

        WriteReg(0x90, 0x03);

        // Official Waveshare AXP2101 examples mark BLDO1 as OLED VDD.
        // Keep it enabled so the AMOLED panel has its dedicated 3.3V rail.
        WriteReg(0x96, (3300 - 500) / 100);
        WriteReg(0x91, 0x01);

        WriteReg(0x64, 0x02); // CV charger voltage setting to 4.1V

        WriteReg(0x61, 0x02); // set Main battery precharge current to 50mA
        WriteReg(0x62, 0x0A); // set Main battery charger current to 400mA
        WriteReg(0x63, 0x01); // set Main battery term charge current to 25mA
    }
};

#define LCD_OPCODE_WRITE_CMD (0x02ULL)
#define LCD_OPCODE_READ_CMD (0x03ULL)
#define LCD_OPCODE_WRITE_COLOR (0x32ULL)

static const sh8601_lcd_init_cmd_t vendor_specific_init[] = {
    {0x11, (uint8_t[]){0x00}, 0, 120},
    {0x44, (uint8_t[]){0x01, 0xD1}, 2, 0},
    {0x35, (uint8_t[]){0x00}, 1, 0},
    {0x53, (uint8_t[]){0x20}, 1, 10},
    {0x51, (uint8_t[]){0x00}, 1, 10},
    {0x2A, (uint8_t[]){0x00, 0x00, 0x01, 0x6F}, 4, 0},
    {0x2B, (uint8_t[]){0x00, 0x00, 0x01, 0xBF}, 4, 0},
    {0x29, (uint8_t[]){0x00}, 0, 10},
    {0x51, (uint8_t[]){0xFF}, 1, 0},
};

// ============================================================================
// CustomLcdDisplay — PRD 368x448 4-tab navigation UI
// The SH8601 window, LVGL display, touch coordinates, and PRD viewport are
// all kept at 368x448. A previous 410x502+gap setup caused striped output
// on the physical AMOLED panel.
// ============================================================================

// Layout constants
static const int kPrdUiW      = 368;
static const int kPrdUiH      = 448;
static const int kTopBarH     = 28;
static const int kBottomBarH  = 52;
static const int kContentH    = 368;
static const int kPagePadX    = 14;
static const int kMaxSavedWifiRows = 10;

// Colour palette
static const lv_color_t kBgBlack     = LV_COLOR_MAKE(0x00, 0x00, 0x00);
static const lv_color_t kTabBarBg    = LV_COLOR_MAKE(0x02, 0x02, 0x02);
static const lv_color_t kWhite       = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF);
static const lv_color_t kTextMuted   = LV_COLOR_MAKE(0x55, 0x55, 0x55);
static const lv_color_t kTextSoft    = LV_COLOR_MAKE(0xAA, 0xAA, 0xAA);
static const lv_color_t kTabActive   = LV_COLOR_MAKE(0x60, 0xA5, 0xFA);
static const lv_color_t kJarvisBlue  = LV_COLOR_MAKE(0x25, 0x63, 0xEB);
static const lv_color_t kHermesPurp  = LV_COLOR_MAKE(0x8B, 0x5C, 0xF6);
static const lv_color_t kOnlineGreen = LV_COLOR_MAKE(0x34, 0xD3, 0x99);
static const lv_color_t kBusyYellow  = LV_COLOR_MAKE(0xFB, 0xBF, 0x24);
static const lv_color_t kOfflineGray = LV_COLOR_MAKE(0x55, 0x55, 0x55);
static const lv_color_t kDimGray     = LV_COLOR_MAKE(0x44, 0x44, 0x44);
static const lv_color_t kCardBg      = LV_COLOR_MAKE(0x0A, 0x0A, 0x0A);
static const lv_color_t kProgressBg  = LV_COLOR_MAKE(0x22, 0x22, 0x22);

#if 1
class CustomLcdDisplay : public SpiLcdDisplay {
public:
    // SH8601 2-pixel alignment rounder (required for 2.06" panel)
    static void rounder_event_cb(lv_event_t* e) {
        lv_area_t* area = (lv_area_t*)lv_event_get_param(e);
        area->x1 = (area->x1 >> 1) << 1;
        area->y1 = (area->y1 >> 1) << 1;
        area->x2 = ((area->x2 >> 1) << 1) + 1;
        area->y2 = ((area->y2 >> 1) << 1) + 1;
    }

    CustomLcdDisplay(esp_lcd_panel_io_handle_t io_handle,
                    esp_lcd_panel_handle_t panel_handle,
                    int width, int height,
                    int offset_x, int offset_y,
                    bool mirror_x, bool mirror_y, bool swap_xy)
        : SpiLcdDisplay(io_handle, panel_handle, width, height,
                        offset_x, offset_y, mirror_x, mirror_y, swap_xy) {
        lv_display_add_event_cb(display_, rounder_event_cb, LV_EVENT_INVALIDATE_AREA, NULL);
        ESP_LOGI(TAG, "LVGL port flush active: SH8601 uses esp_lvgl_port RGB565 swap_bytes path");
    }

    static bool DirectFlushReadyCallback(esp_lcd_panel_io_handle_t panel_io,
                                         esp_lcd_panel_io_event_data_t* edata,
                                         void* user_ctx) {
        (void)panel_io;
        (void)edata;
        auto* self = static_cast<CustomLcdDisplay*>(user_ctx);
        if (self != nullptr && self->flush_done_sem_ != nullptr) {
            BaseType_t task_awake = pdFALSE;
            xSemaphoreGiveFromISR(self->flush_done_sem_, &task_awake);
            return task_awake == pdTRUE;
        }
        return false;
    }

    static void DirectFlushCallback(lv_display_t* display, const lv_area_t* area, uint8_t* color_map) {
        auto* self = static_cast<CustomLcdDisplay*>(lv_display_get_user_data(display));
        if (self == nullptr || self->panel_ == nullptr || self->dma_flush_buffer_ == nullptr ||
            self->flush_done_sem_ == nullptr) {
            lv_display_flush_ready(display);
            return;
        }

        const int area_width = lv_area_get_width(area);
        const auto* source = reinterpret_cast<const uint16_t*>(color_map);

        for (int y = area->y1; y <= area->y2; y += kFlushRows) {
            const int rows = std::min(kFlushRows, static_cast<int>(area->y2 - y + 1));
            const size_t pixels = static_cast<size_t>(area_width) * rows;
            const size_t offset_pixels = static_cast<size_t>(y - area->y1) * area_width;
            std::memcpy(self->dma_flush_buffer_, source + offset_pixels, pixels * sizeof(uint16_t));
            lv_draw_sw_rgb565_swap(reinterpret_cast<uint8_t*>(self->dma_flush_buffer_), pixels);

            xSemaphoreTake(self->flush_done_sem_, 0);
            esp_err_t ret = esp_lcd_panel_draw_bitmap(self->panel_,
                                                      area->x1,
                                                      y,
                                                      area->x2 + 1,
                                                      y + rows,
                                                      self->dma_flush_buffer_);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Direct LVGL flush failed: %s", esp_err_to_name(ret));
                lv_display_flush_ready(display);
                return;
            }
            if (xSemaphoreTake(self->flush_done_sem_, pdMS_TO_TICKS(500)) != pdTRUE) {
                ESP_LOGE(TAG, "Timed out waiting for LCD flush");
                lv_display_flush_ready(display);
                return;
            }
        }

        lv_display_flush_ready(display);
    }

    static constexpr int kFlushRows = 20;
    lv_color_t* frame_buffer_ = nullptr;
    uint16_t* dma_flush_buffer_ = nullptr;
    SemaphoreHandle_t flush_done_sem_ = nullptr;

    void EnableDirectLvglFlush() {
        if (display_ == nullptr || panel_io_ == nullptr || panel_ == nullptr) {
            ESP_LOGW(TAG, "Direct LVGL flush unavailable: display or panel is not initialized");
            return;
        }

        dma_flush_buffer_ = static_cast<uint16_t*>(heap_caps_malloc(
            width_ * kFlushRows * sizeof(uint16_t),
            MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
        flush_done_sem_ = xSemaphoreCreateBinary();
        if (dma_flush_buffer_ == nullptr || flush_done_sem_ == nullptr) {
            ESP_LOGW(TAG, "Direct LVGL flush disabled: failed to allocate DMA buffer or semaphore");
            if (dma_flush_buffer_ != nullptr) {
                heap_caps_free(dma_flush_buffer_);
                dma_flush_buffer_ = nullptr;
            }
            if (flush_done_sem_ != nullptr) {
                vSemaphoreDelete(flush_done_sem_);
                flush_done_sem_ = nullptr;
            }
            return;
        }

        const esp_lcd_panel_io_callbacks_t callbacks = {
            .on_color_trans_done = DirectFlushReadyCallback,
        };
        ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(panel_io_, &callbacks, this));
        lv_display_set_user_data(display_, this);
        lv_display_set_flush_cb(display_, DirectFlushCallback);
        ESP_LOGI(TAG,
                 "Direct LVGL flush enabled: rows=%d dma_bytes=%u swap=manual",
                 kFlushRows,
                 static_cast<unsigned>(width_ * kFlushRows * sizeof(uint16_t)));
    }

    static uint16_t Rgb565(uint8_t r, uint8_t g, uint8_t b) {
        return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    }

    void FillRectDirect(int x, int y, int w, int h, uint16_t color) {
        if (w <= 0 || h <= 0) {
            return;
        }
        x = std::max(0, x);
        y = std::max(0, y);
        w = std::min(w, width_ - x);
        h = std::min(h, height_ - y);
        std::vector<uint16_t> line(w, color);
        for (int row = y; row < y + h; ++row) {
            ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_, x, row, x + w, row + 1, line.data()));
        }
    }

    void DrawDirectDashboard() {
        ESP_LOGI(TAG, "LCD diagnostic: drawing direct RGB565 dashboard pattern");
        const uint16_t black = Rgb565(0, 0, 0);
        const uint16_t blue = Rgb565(37, 99, 235);
        const uint16_t purple = Rgb565(139, 92, 246);
        const uint16_t green = Rgb565(52, 211, 153);
        const uint16_t yellow = Rgb565(251, 191, 36);
        const uint16_t dark = Rgb565(18, 18, 18);
        const uint16_t card = Rgb565(10, 10, 10);
        const uint16_t white = Rgb565(255, 255, 255);
        const uint16_t gray = Rgb565(80, 80, 80);

        FillRectDirect(0, 0, width_, height_, black);
        FillRectDirect(0, 0, width_, 34, blue);
        FillRectDirect(12, 10, 86, 14, white);
        FillRectDirect(width_ - 70, 10, 16, 14, green);
        FillRectDirect(width_ - 46, 10, 34, 14, white);

        FillRectDirect(18, 54, width_ - 36, 78, card);
        FillRectDirect(30, 70, 120, 18, blue);
        FillRectDirect(30, 98, 250, 12, gray);
        FillRectDirect(298, 72, 58, 40, green);

        FillRectDirect(18, 150, 178, 122, card);
        FillRectDirect(34, 168, 74, 16, purple);
        FillRectDirect(34, 202, 132, 12, gray);
        FillRectDirect(34, 228, 96, 18, blue);

        FillRectDirect(214, 150, 178, 122, card);
        FillRectDirect(230, 168, 74, 16, green);
        FillRectDirect(230, 202, 132, 12, gray);
        FillRectDirect(230, 228, 96, 18, yellow);

        FillRectDirect(18, 292, width_ - 36, 118, card);
        FillRectDirect(34, 314, 100, 16, blue);
        FillRectDirect(34, 346, 306, 12, gray);
        FillRectDirect(34, 374, 220, 14, green);

        FillRectDirect(0, height_ - 54, width_, 54, dark);
        FillRectDirect(22, height_ - 38, 52, 22, blue);
        FillRectDirect(124, height_ - 38, 52, 22, gray);
        FillRectDirect(226, height_ - 38, 52, 22, gray);
        FillRectDirect(328, height_ - 38, 52, 22, gray);
        ESP_LOGI(TAG, "LCD diagnostic: direct RGB565 dashboard pattern drawn");
    }

    void DrawTransportDiagnosticFromFallback() {
        if (display_ == nullptr) {
            return;
        }
        DisplayLockGuard lock(this);
        DrawDirectDashboard();
    }

    // Chat state machine (forward-declared for use in function signatures)
    enum ChatState { kChatIdle, kChatRecording, kChatThinking, kChatReply, kChatError };

    // ------------------------------------------------------------------
    // SetupUI — called once after LVGL & display are initialised
    // ------------------------------------------------------------------
    void VerifyPrdLayout(lv_obj_t* screen) {
        lv_obj_update_layout(screen);
        const int container_w = lv_obj_get_width(container_);
        const int container_h = lv_obj_get_height(container_);
        const int top_w = lv_obj_get_width(top_status_bar_);
        const int top_h = lv_obj_get_height(top_status_bar_);
        const int content_w = lv_obj_get_width(content_area_);
        const int content_h = lv_obj_get_height(content_area_);
        const int tab_w = lv_obj_get_width(tab_bar_);
        const int tab_h = lv_obj_get_height(tab_bar_);
        int min_tab_btn_w = kPrdUiW;
        int min_tab_btn_h = kBottomBarH;
        for (lv_obj_t* btn : tab_btns_) {
            if (!btn) {
                min_tab_btn_w = 0;
                min_tab_btn_h = 0;
                break;
            }
            min_tab_btn_w = std::min(min_tab_btn_w, static_cast<int>(lv_obj_get_width(btn)));
            min_tab_btn_h = std::min(min_tab_btn_h, static_cast<int>(lv_obj_get_height(btn)));
        }
        const bool ok =
            container_w == kPrdUiW && container_h == kPrdUiH &&
            top_w == kPrdUiW && top_h == kTopBarH &&
            content_w == kPrdUiW && content_h == kContentH &&
            tab_w == kPrdUiW && tab_h == kBottomBarH;
        const bool touch_ok = min_tab_btn_w >= 44 && min_tab_btn_h >= 44;

        if (ok) {
            ESP_LOGI(TAG,
                     "PRD UI layout verified: container=%dx%d top=%dx%d content=%dx%d tab=%dx%d",
                     container_w, container_h, top_w, top_h,
                     content_w, content_h, tab_w, tab_h);
        } else {
            ESP_LOGE(TAG,
                     "PRD UI layout mismatch: container=%dx%d top=%dx%d content=%dx%d tab=%dx%d expected=%dx%d/%d/%d/%d",
                     container_w, container_h, top_w, top_h,
                     content_w, content_h, tab_w, tab_h,
                     kPrdUiW, kPrdUiH, kTopBarH, kContentH, kBottomBarH);
        }
        if (touch_ok) {
            ESP_LOGI(TAG,
                     "PRD touch targets verified: min_tab=%dx%d mic=44x44 wifi_rows=44 blufi=44",
                     min_tab_btn_w, min_tab_btn_h);
        } else {
            ESP_LOGE(TAG,
                     "PRD touch target mismatch: min_tab=%dx%d expected>=44x44",
                     min_tab_btn_w, min_tab_btn_h);
        }
    }

    virtual void SetupUI() override {
        if (setup_ui_called_) {
            ESP_LOGW(TAG, "SetupUI() called multiple times, skipping duplicate call");
            return;
        }

        Display::SetupUI();

        DisplayLockGuard lock(this);

        auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
        auto text_font = lvgl_theme->text_font()->font();

        auto screen = lv_screen_active();
        lv_obj_clean(screen);
        lv_obj_set_style_bg_color(screen, kBgBlack, 0);
        lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_image_src(screen, nullptr, 0);
        lv_obj_set_style_border_width(screen, 0, 0);
        lv_obj_set_style_pad_all(screen, 0, 0);
        lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

        CreateHiddenSystemLabels(screen);

        container_ = lv_obj_create(screen);
        lv_obj_set_size(container_, kPrdUiW, kPrdUiH);
        lv_obj_center(container_);
        lv_obj_set_style_layout(container_, LV_LAYOUT_NONE, 0);
        lv_obj_set_style_bg_color(container_, kBgBlack, 0);
        lv_obj_set_style_bg_opa(container_, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_image_src(container_, nullptr, 0);
        lv_obj_set_style_bg_image_opa(container_, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(container_, 0, 0);
        lv_obj_set_style_pad_all(container_, 0, 0);
        lv_obj_remove_flag(container_, LV_OBJ_FLAG_HIDDEN);

        HideBaseOverlayObjects();

        ESP_LOGI(TAG,
                 "PRD UI mounted: viewport=%dx%d top=%d content=%d bottom=%d physical=%dx%d",
                 kPrdUiW, kPrdUiH, kTopBarH, kContentH, kBottomBarH,
                 DISPLAY_WIDTH, DISPLAY_HEIGHT);

        top_status_bar_ = lv_obj_create(container_);
        lv_obj_set_pos(top_status_bar_, 0, 0);
        lv_obj_set_size(top_status_bar_, kPrdUiW, kTopBarH);
        lv_obj_set_style_bg_color(top_status_bar_, kBgBlack, 0);
        lv_obj_set_style_bg_opa(top_status_bar_, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(top_status_bar_, 0, 0);
        lv_obj_set_style_radius(top_status_bar_, 0, 0);
        lv_obj_set_style_pad_left(top_status_bar_, 14, 0);
        lv_obj_set_style_pad_right(top_status_bar_, 14, 0);
        lv_obj_set_style_pad_top(top_status_bar_, 0, 0);
        lv_obj_set_style_pad_bottom(top_status_bar_, 0, 0);
        lv_obj_set_flex_flow(top_status_bar_, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(top_status_bar_, LV_FLEX_ALIGN_SPACE_BETWEEN,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(top_status_bar_, LV_OBJ_FLAG_SCROLLABLE);

        time_label_ = lv_label_create(top_status_bar_);
        lv_label_set_text(time_label_, "--:--");
        lv_obj_set_style_text_font(time_label_, text_font, 0);
        lv_obj_set_style_text_color(time_label_, kTextMuted, 0);

        lv_obj_t* status_icons = lv_obj_create(top_status_bar_);
        lv_obj_set_size(status_icons, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(status_icons, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(status_icons, 0, 0);
        lv_obj_set_style_pad_all(status_icons, 0, 0);
        lv_obj_set_style_pad_column(status_icons, 8, 0);
        lv_obj_set_flex_flow(status_icons, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(status_icons, LV_FLEX_ALIGN_END,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(status_icons, LV_OBJ_FLAG_SCROLLABLE);

        custom_wifi_label_ = lv_label_create(status_icons);
        lv_label_set_text(custom_wifi_label_, "--");
        lv_obj_set_style_text_font(custom_wifi_label_, text_font, 0);
        lv_obj_set_style_text_color(custom_wifi_label_, kOnlineGreen, 0);

        custom_battery_label_ = lv_label_create(status_icons);
        lv_label_set_text(custom_battery_label_, "--%");
        lv_obj_set_style_text_font(custom_battery_label_, text_font, 0);
        lv_obj_set_style_text_color(custom_battery_label_, kTextMuted, 0);

        content_area_ = lv_obj_create(container_);
        lv_obj_set_pos(content_area_, 0, kTopBarH);
        lv_obj_set_size(content_area_, kPrdUiW, kContentH);
        lv_obj_set_style_bg_color(content_area_, kBgBlack, 0);
        lv_obj_set_style_bg_opa(content_area_, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(content_area_, 0, 0);
        lv_obj_set_style_radius(content_area_, 0, 0);
        lv_obj_set_style_pad_all(content_area_, 0, 0);
        lv_obj_set_style_layout(content_area_, LV_LAYOUT_NONE, 0);
        lv_obj_clear_flag(content_area_, LV_OBJ_FLAG_SCROLLABLE);

        page_status_ = CreatePageContainer(content_area_);
        page_wifi_ = CreatePageContainer(content_area_);
        page_voice_ = CreatePageContainer(content_area_);
        page_device_ = CreatePageContainer(content_area_);

        BuildStatusPage(page_status_, text_font, text_font);
        BuildWifiPage(page_wifi_, text_font);
        BuildVoicePage(page_voice_, text_font);
        BuildDevicePage(page_device_, text_font);

        tab_bar_ = lv_obj_create(container_);
        lv_obj_set_pos(tab_bar_, 0, kTopBarH + kContentH);
        lv_obj_set_size(tab_bar_, kPrdUiW, kBottomBarH);
        lv_obj_set_style_bg_color(tab_bar_, kTabBarBg, 0);
        lv_obj_set_style_bg_opa(tab_bar_, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(tab_bar_, LV_COLOR_MAKE(0x1A, 0x1A, 0x1A), 0);
        lv_obj_set_style_border_side(tab_bar_, LV_BORDER_SIDE_TOP, 0);
        lv_obj_set_style_border_width(tab_bar_, 1, 0);
        lv_obj_set_style_radius(tab_bar_, 0, 0);
        lv_obj_set_style_pad_all(tab_bar_, 2, 0);
        lv_obj_set_flex_flow(tab_bar_, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(tab_bar_, LV_FLEX_ALIGN_SPACE_EVENLY,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_user_data(tab_bar_, this);
        lv_obj_clear_flag(tab_bar_, LV_OBJ_FLAG_SCROLLABLE);

        CreateTabBtn(tab_bar_, "", "Status", 0, text_font, text_font);
        CreateTabBtn(tab_bar_, "", "WiFi", 1, text_font, text_font);
        CreateTabBtn(tab_bar_, "", "Conversation", 2, text_font, text_font);
        CreateTabBtn(tab_bar_, "", "Device", 3, text_font, text_font);
        int initial_tab = active_tab_;
        active_tab_ = -1;
        SwitchTab(initial_tab);
        SetChatState(kChatIdle);
        ForceCustomUiVisible();
        VerifyPrdLayout(screen);
        lv_obj_invalidate(screen);
        lv_refr_now(display_);

        ESP_LOGI(TAG, "Custom 4-tab UI mounted on container_");
    }

    virtual void SetEmotion(const char* emotion) override {
        (void)emotion;
        if (display_ == nullptr) {
            return;
        }
        DisplayLockGuard lock(this);
        ForceCustomUiVisible();
    }

    virtual void ClearChatMessages() override {
        if (display_ == nullptr) {
            return;
        }
        DisplayLockGuard lock(this);
        ForceCustomUiVisible();

        for (auto& bubble : chat_bubbles_) {
            if (bubble) lv_obj_add_flag(bubble, LV_OBJ_FLAG_HIDDEN);
        }
        chat_bubble_idx_ = 0;
        chat_bubble_count_ = 0;
        if (chat_guidance_) lv_obj_remove_flag(chat_guidance_, LV_OBJ_FLAG_HIDDEN);
        SetChatState(kChatIdle);
    }

    // ------------------------------------------------------------------
    // Override UpdateStatusBar to update our custom labels
    // ------------------------------------------------------------------
    virtual void UpdateStatusBar(bool update_all) override {
        if (display_ == nullptr) {
            return;
        }
        // Let the base class update its (hidden) labels so that
        // battery_icon_, network_icon_, muted_ state stays in sync.
        LvglDisplay::UpdateStatusBar(update_all);

        DisplayLockGuard lock(this);

        // --- Time ---
        time_t now = time(NULL);
        struct tm* tm_info = localtime(&now);
        if (tm_info->tm_year >= (2025 - 1900)) {
            char buf[16];
            strftime(buf, sizeof(buf), "%H:%M", tm_info);
            if (time_label_) lv_label_set_text(time_label_, buf);
        }

        // --- Mirror WiFi icon to our custom label ---
        if (custom_wifi_label_ && network_icon_) {
            lv_label_set_text(custom_wifi_label_, network_icon_);
        }

        // --- Battery percentage ---
        if (custom_battery_label_) {
            auto& board = Board::GetInstance();
            int level; bool chg, dischg;
            if (board.GetBatteryLevel(level, chg, dischg)) {
                char bbuf[12];
                snprintf(bbuf, sizeof(bbuf), "%d%%", level);
                lv_label_set_text(custom_battery_label_, bbuf);
            }
        }

        RefreshWifiInfo();
        RefreshDeviceInfo();
    }

    // ------------------------------------------------------------------
    // Override SetTheme — prevent base class from overwriting our custom UI
    // assets.cc calls display->SetTheme() after WiFi connect, which triggers
    // LcdDisplay::SetTheme() → sets container_ background image (lined paper),
    // covering our custom Tab UI. We skip the LcdDisplay layer entirely and
    // only save the theme via Display::SetTheme() (settings persistence).
    // ------------------------------------------------------------------
    virtual void SetTheme(Theme* theme) override {
        ESP_LOGI(TAG, "CustomLcdDisplay::SetTheme — skipping LcdDisplay theme refresh");
        // Only persist the theme setting; do NOT touch any LVGL objects
        Display::SetTheme(theme);
        if (display_ == nullptr) {
            return;
        }
        DisplayLockGuard lock(this);
        ForceCustomUiVisible();
    }

    // ------------------------------------------------------------------
    // Override SetStatus to update voice page ASR area
    // ------------------------------------------------------------------
    virtual void SetStatus(const char* status) override {
        if (display_ == nullptr) {
            return;
        }
        DisplayLockGuard lock(this);
        UpdateChatStateFromStatus(status);
        if (status_label_) {
            lv_label_set_text(status_label_, status ? status : "");
            lv_obj_add_flag(status_label_, LV_OBJ_FLAG_HIDDEN);
        }
        if (chat_hint_label_ && status && status[0] != '\0') {
            lv_label_set_text(chat_hint_label_, status);
        }
        RefreshDeviceInfo();
        last_status_update_time_ = std::chrono::system_clock::now();
        ForceCustomUiVisible();
    }

    // ------------------------------------------------------------------
    // Override SetChatMessage — route to chat page bubble system
    // ------------------------------------------------------------------
    virtual void SetChatMessage(const char* role, const char* content) override {
        if (display_ == nullptr) {
            return;
        }
        DisplayLockGuard lock(this);
        ForceCustomUiVisible();
        if (!content || content[0] == '\0') return;

        if (strcmp(role, "user") == 0) {
            // User message (ASR text) — add blue right-aligned bubble
            UpdateChatAgentFromText(content);
            AddChatBubble(content, true /* is_user */);
            SetChatState(kChatThinking);
        } else if (strcmp(role, "assistant") == 0) {
            // Agent reply — add dark left-aligned bubble
            AddChatBubble(content, false /* is_user */);
            SetChatState(kChatReply);
        }
    }

    virtual void SetAgentStatus(const char* json) override {
        if (display_ == nullptr || !json || json[0] == '\0') {
            return;
        }
        cJSON* root = cJSON_Parse(json);
        if (!root) {
            ESP_LOGW(TAG, "Invalid agent_status JSON");
            return;
        }

        DisplayLockGuard lock(this);
        ApplyAgentStatusPayload(root);
        cJSON_Delete(root);
        RefreshWifiInfo();
        RefreshDeviceInfo();
        ForceCustomUiVisible();
        RefreshNow();
    }

    lv_color_t GetCurrentAgentColor() const {
        if (current_agent_name_ == "Hermes") return kHermesPurp;
        if (current_agent_name_ == "贾维斯") return kJarvisBlue;
        return kDimGray;
    }

    static bool ContainsAsciiNoCase(const char* text, const char* needle) {
        if (!text || !needle) return false;
        std::string haystack(text);
        std::string target(needle);
        std::transform(haystack.begin(), haystack.end(), haystack.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        std::transform(target.begin(), target.end(), target.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return haystack.find(target) != std::string::npos;
    }

    void ApplyChatAgent(const char* name, lv_color_t color) {
        current_agent_name_ = name;
        if (chat_agent_name_) {
            lv_label_set_text(chat_agent_name_, name);
            lv_obj_set_style_text_color(chat_agent_name_, color, 0);
        }
        if (chat_route_label_) {
            std::string route = "→ ";
            route += name;
            lv_label_set_text(chat_route_label_, route.c_str());
            lv_obj_set_style_text_color(chat_route_label_, color, 0);
            lv_obj_set_style_border_color(chat_route_label_, color, 0);
        }
        if (chat_agent_icon_) {
            if (strcmp(name, "Hermes") == 0) {
                lv_label_set_text(chat_agent_icon_, FONT_AWESOME_CLOUD_BOLT);
            } else if (strcmp(name, "贾维斯") == 0) {
                lv_label_set_text(chat_agent_icon_, FONT_AWESOME_USER_ROBOT);
            } else {
                lv_label_set_text(chat_agent_icon_, FONT_AWESOME_VOLUME_HIGH);
            }
            lv_obj_set_style_text_color(chat_agent_icon_, color, 0);
        }
        if (chat_mic_btn_ && chat_state_ != kChatRecording) {
            lv_obj_set_style_bg_color(chat_mic_btn_, color, 0);
        }
    }

    void UpdateChatAgentFromText(const char* content) {
        if (!content) return;
        if (strstr(content, "Hermes") || ContainsAsciiNoCase(content, "hermes")) {
            ApplyChatAgent("Hermes", kHermesPurp);
        } else if (strstr(content, "贾维斯") || strstr(content, "Jarvis") ||
                   ContainsAsciiNoCase(content, "jarvis")) {
            ApplyChatAgent("贾维斯", kJarvisBlue);
        }
    }

    void UpdateChatStateFromStatus(const char* status) {
        if (!status || !chat_agent_status_) return;
        if (strstr(status, "聆听") || strstr(status, "Listening") ||
            ContainsAsciiNoCase(status, "listening")) {
            SetChatState(kChatRecording);
        } else if (strstr(status, "请稍候") || strstr(status, "处理中") ||
                   strstr(status, "Thinking") || ContainsAsciiNoCase(status, "thinking")) {
            SetChatState(kChatThinking);
        } else if (strstr(status, "说话") || strstr(status, "Speaking") ||
                   ContainsAsciiNoCase(status, "speaking")) {
            SetChatState(kChatReply);
        } else if (strstr(status, "错误") || strstr(status, "失败") ||
                   ContainsAsciiNoCase(status, "error") || ContainsAsciiNoCase(status, "failed")) {
            SetChatState(kChatError);
        } else if (strstr(status, "待命") || strstr(status, "Standby") ||
                   ContainsAsciiNoCase(status, "standby")) {
            SetChatState(kChatIdle);
        }
    }

    static const cJSON* FindAgentItem(const cJSON* root, const char* agent_name) {
        const cJSON* agents = cJSON_GetObjectItem(root, "agents");
        if (cJSON_IsArray(agents)) {
            cJSON* item = nullptr;
            cJSON_ArrayForEach(item, agents) {
                const cJSON* name = cJSON_GetObjectItem(item, "name");
                if (cJSON_IsString(name) && strcmp(name->valuestring, agent_name) == 0) {
                    return item;
                }
            }
        } else if (cJSON_IsObject(agents)) {
            return cJSON_GetObjectItem(agents, agent_name);
        }
        return nullptr;
    }

    void ApplyAgentStatusPayload(const cJSON* root) {
        if (!root) return;
        UpdateAgentCardFromJson("贾维斯", FindAgentItem(root, "贾维斯"));
        UpdateAgentCardFromJson("Jarvis", FindAgentItem(root, "Jarvis"));
        UpdateAgentCardFromJson("Hermes", FindAgentItem(root, "Hermes"));
        UpdateTaskSection(cJSON_GetObjectItem(root, "active_tasks"), active_tasks_header_,
                          "活跃任务", task_labels_, 6, active_tasks_more_label_, true);
        const cJSON* done_tasks = cJSON_GetObjectItem(root, "completed_tasks");
        if (!cJSON_IsArray(done_tasks)) {
            done_tasks = cJSON_GetObjectItem(root, "recent_done");
        }
        UpdateRecentDoneSummary(done_tasks);
        ESP_LOGI(TAG, "Applying agent_status: agents=%d active=%d done=%d",
                 cJSON_GetArraySize(cJSON_GetObjectItem(root, "agents")),
                 cJSON_GetArraySize(cJSON_GetObjectItem(root, "active_tasks")),
                 cJSON_GetArraySize(done_tasks));
        UpdateTaskSection(done_tasks, done_tasks_header_, "最近完成", done_labels_, 3, nullptr, false);
    }

    void UpdateAgentCardFromJson(const char* agent_name, const cJSON* item) {
        if (!cJSON_IsObject(item)) return;

        lv_obj_t* status_label = nullptr;
        lv_obj_t* action_label = nullptr;
        lv_obj_t* progress = nullptr;
        lv_color_t accent = kJarvisBlue;
        if (strcmp(agent_name, "Hermes") == 0) {
            status_label = hermes_status_label_;
            action_label = hermes_action_label_;
            progress = hermes_progress_;
            accent = kHermesPurp;
        } else {
            status_label = jarvis_status_label_;
            action_label = jarvis_action_label_;
            progress = jarvis_progress_;
        }

        const cJSON* status = cJSON_GetObjectItem(item, "status");
        if (status_label && cJSON_IsString(status)) {
            const char* display_status = status->valuestring;
            lv_color_t color = kOfflineGray;
            if (strcmp(display_status, "online") == 0 || strcmp(display_status, "在线") == 0) {
                display_status = "在线";
                color = kOnlineGreen;
            } else if (strcmp(display_status, "busy") == 0 || strcmp(display_status, "忙碌") == 0) {
                display_status = "忙碌";
                color = kBusyYellow;
            } else if (strcmp(display_status, "offline") == 0 || strcmp(display_status, "离线") == 0) {
                display_status = "离线";
                color = kOfflineGray;
            }
            lv_label_set_text(status_label, display_status);
            lv_obj_set_style_text_color(status_label, color, 0);
        }

        const cJSON* action = cJSON_GetObjectItem(item, "action");
        if (!cJSON_IsString(action)) action = cJSON_GetObjectItem(item, "current_task");
        if (action_label && cJSON_IsString(action)) {
            lv_label_set_text(action_label, action->valuestring);
        }

        const cJSON* context = cJSON_GetObjectItem(item, "context_pct");
        if (!cJSON_IsNumber(context)) context = cJSON_GetObjectItem(item, "context");
        if (progress && cJSON_IsNumber(context)) {
            int pct = std::max(0, std::min(100, context->valueint));
            int fill_w = ((kPrdUiW - kPagePadX * 2 - 20) * pct) / 100;
            lv_obj_set_width(progress, fill_w > 2 ? fill_w : 2);
            lv_color_t warning_color = LV_COLOR_MAKE(0xF8, 0x71, 0x71);
            lv_obj_set_style_bg_color(progress, pct >= 80 ? warning_color : accent, 0);
        }
    }

    void UpdateTaskSection(const cJSON* tasks, lv_obj_t* header, const char* title,
                           lv_obj_t* labels[], size_t label_count, lv_obj_t* more_label,
                           bool show_overflow) {
        const bool tasks_is_array = cJSON_IsArray(tasks);
        const int total = tasks_is_array ? cJSON_GetArraySize(tasks) : 0;
        if (header) {
            std::string header_text = title;
            header_text += " · ";
            header_text += std::to_string(total);
            lv_label_set_text(header, header_text.c_str());
        }
        if (total == 0 && label_count > 0 && labels[0]) {
            lv_label_set_text(labels[0], show_overflow ? "暂无活跃任务" : "暂无完成任务");
            for (size_t i = 1; i < label_count; ++i) {
                if (labels[i]) lv_label_set_text(labels[i], "");
            }
            if (more_label) lv_label_set_text(more_label, "");
            return;
        }
        for (size_t i = 0; i < label_count; ++i) {
            if (!labels[i]) continue;
            const cJSON* task = tasks_is_array ? cJSON_GetArrayItem(tasks, i) : nullptr;
            if (!cJSON_IsObject(task)) {
                lv_label_set_text(labels[i], "");
                continue;
            }
            const cJSON* name = cJSON_GetObjectItem(task, "name");
            if (!cJSON_IsString(name)) name = cJSON_GetObjectItem(task, "title");
            const cJSON* assignee = cJSON_GetObjectItem(task, "assignee");
            if (!cJSON_IsString(assignee)) assignee = cJSON_GetObjectItem(task, "agent");
            const cJSON* progress = cJSON_GetObjectItem(task, "progress");
            const cJSON* time = cJSON_GetObjectItem(task, "time");
            if (!cJSON_IsString(time)) time = cJSON_GetObjectItem(task, "state");
            std::string text = cJSON_IsString(name) ? name->valuestring : "--";
            if (cJSON_IsString(assignee)) {
                text += "  ";
                text += assignee->valuestring;
            }
            if (cJSON_IsNumber(progress)) {
                text += "  ";
                text += std::to_string(progress->valueint);
                text += "%";
            } else if (cJSON_IsString(time)) {
                text += "  ";
                text += time->valuestring;
            }
            lv_label_set_text(labels[i], text.c_str());
        }
        if (more_label) {
            if (show_overflow && total > static_cast<int>(label_count)) {
                std::string more = "还有 ";
                more += std::to_string(total - static_cast<int>(label_count));
                more += " 项";
                lv_label_set_text(more_label, more.c_str());
            } else {
                lv_label_set_text(more_label, "");
            }
        }
    }

    void UpdateRecentDoneSummary(const cJSON* done_tasks) {
        if (!recent_done_summary_label_) return;
        const bool tasks_is_array = cJSON_IsArray(done_tasks);
        const int total = tasks_is_array ? cJSON_GetArraySize(done_tasks) : 0;
        std::string text = "最近完成 · ";
        text += std::to_string(total);
        if (total == 0) {
            text += " · 暂无完成任务";
        } else {
            const cJSON* task = cJSON_GetArrayItem(done_tasks, 0);
            const cJSON* name = cJSON_IsObject(task) ? cJSON_GetObjectItem(task, "name") : nullptr;
            if (!cJSON_IsString(name) && cJSON_IsObject(task)) name = cJSON_GetObjectItem(task, "title");
            if (cJSON_IsString(name)) {
                text += " · ";
                text += name->valuestring;
            }
        }
        lv_label_set_text(recent_done_summary_label_, text.c_str());
    }

    // Add a message bubble to the chat area (ring buffer, max 5)
    void AddChatBubble(const char* text, bool is_user) {
        if (!chat_msg_area_) return;
        if (chat_guidance_) lv_obj_add_flag(chat_guidance_, LV_OBJ_FLAG_HIDDEN);

        // Hide oldest bubble if we have 5
        if (chat_bubble_count_ >= 5) {
            int oldest = chat_bubble_idx_;  // Current idx points to oldest
            if (chat_bubbles_[oldest]) {
                lv_obj_add_flag(chat_bubbles_[oldest], LV_OBJ_FLAG_HIDDEN);
            }
        }

        // Reuse or create bubble at current slot
        int slot = chat_bubble_idx_;
        if (!chat_bubbles_[slot]) {
            chat_bubbles_[slot] = CreateChatBubble(chat_msg_area_, text, is_user);
        } else {
            UpdateChatBubble(chat_bubbles_[slot], text, is_user);
            lv_obj_remove_flag(chat_bubbles_[slot], LV_OBJ_FLAG_HIDDEN);
        }

        // Advance ring buffer
        chat_bubble_idx_ = (chat_bubble_idx_ + 1) % 5;
        if (chat_bubble_count_ < 5) chat_bubble_count_++;

        // Scroll to bottom
        lv_obj_scroll_to_y(chat_msg_area_, LV_COORD_MAX, LV_ANIM_ON);
    }

    static lv_obj_t* CreateChatBubble(lv_obj_t* parent, const char* text, bool is_user) {
        lv_obj_t* bubble = lv_obj_create(parent);
        lv_obj_set_width(bubble, lv_pct(80));
        lv_obj_set_height(bubble, LV_SIZE_CONTENT);
        lv_obj_set_style_min_height(bubble, 32, 0);

        if (is_user) {
            // User: blue bg, right-aligned
            lv_obj_set_style_bg_color(bubble, kJarvisBlue, 0);
            lv_obj_set_style_bg_opa(bubble, LV_OPA_90, 0);
            lv_obj_set_style_border_width(bubble, 0, 0);
            lv_obj_set_style_radius(bubble, 12, 0);
            lv_obj_set_style_pad_all(bubble, 8, 0);
            lv_obj_align(bubble, LV_ALIGN_RIGHT_MID, 0, 0);
        } else {
            // Agent: dark bg, left-aligned
            lv_obj_set_style_bg_color(bubble, kCardBg, 0);
            lv_obj_set_style_bg_opa(bubble, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(bubble, kDimGray, 0);
            lv_obj_set_style_border_width(bubble, 1, 0);
            lv_obj_set_style_radius(bubble, 12, 0);
            lv_obj_set_style_pad_all(bubble, 8, 0);
            lv_obj_align(bubble, LV_ALIGN_LEFT_MID, 0, 0);
        }
        lv_obj_clear_flag(bubble, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* lbl = lv_label_create(bubble);
        lv_label_set_text(lbl, text);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(lbl, lv_pct(100));
        lv_obj_set_style_text_font(lbl, lv_obj_get_style_text_font(parent, LV_PART_MAIN), 0);
        lv_obj_set_style_text_color(lbl, kWhite, 0);

        return bubble;
    }

    static void UpdateChatBubble(lv_obj_t* bubble, const char* text, bool is_user) {
        // Find and update the label inside bubble
        uint32_t cnt = lv_obj_get_child_cnt(bubble);
        for (uint32_t i = 0; i < cnt; i++) {
            lv_obj_t* child = lv_obj_get_child(bubble, i);
            if (lv_obj_has_class(child, &lv_label_class)) {
                lv_label_set_text(child, text);
                break;
            }
        }
        // Update alignment
        lv_obj_align(bubble, is_user ? LV_ALIGN_RIGHT_MID : LV_ALIGN_LEFT_MID, 0, 0);
        if (is_user) {
            lv_obj_set_style_bg_color(bubble, kJarvisBlue, 0);
            lv_obj_set_style_bg_opa(bubble, LV_OPA_90, 0);
            lv_obj_set_style_border_width(bubble, 0, 0);
        } else {
            lv_obj_set_style_bg_color(bubble, kCardBg, 0);
            lv_obj_set_style_bg_opa(bubble, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(bubble, kDimGray, 0);
            lv_obj_set_style_border_width(bubble, 1, 0);
        }
    }

    // State machine transition
    void SetChatState(ChatState new_state) {
        chat_state_ = new_state;
        // Update agent bar status
        const char* status_text = "";
        lv_color_t status_color = kDimGray;

        switch (new_state) {
            case kChatIdle:
                status_text = "待命"; status_color = kOfflineGray; break;
            case kChatRecording:
                status_text = "聆听中"; status_color = LV_COLOR_MAKE(0xEF, 0x44, 0x44); break;
            case kChatThinking:
                status_text = "思考中"; status_color = kBusyYellow; break;
            case kChatReply:
                status_text = "在线"; status_color = kOnlineGreen; break;
            case kChatError:
                status_text = "错误"; status_color = LV_COLOR_MAKE(0xEF, 0x44, 0x44); break;
        }

        if (chat_agent_status_) {
            lv_label_set_text(chat_agent_status_, status_text);
            lv_obj_set_style_text_color(chat_agent_status_, status_color, 0);
        }

        // Update hint text
        const char* hints[] = {
            "你好小智唤醒，或点击麦克风",
            "正在聆听... (最长10秒)",
            "等待回复...",
            "点击继续对话",
            "点击重试"
        };
        if (chat_hint_label_)
            lv_label_set_text(chat_hint_label_, hints[new_state]);

        // Show/hide waveform
        if (chat_waveform_) {
            if (new_state == kChatRecording)
                lv_obj_remove_flag(chat_waveform_, LV_OBJ_FLAG_HIDDEN);
            else
                lv_obj_add_flag(chat_waveform_, LV_OBJ_FLAG_HIDDEN);
        }

        // Show/hide thinking animation
        if (chat_thinking_widget_) {
            if (new_state == kChatThinking)
                lv_obj_remove_flag(chat_thinking_widget_, LV_OBJ_FLAG_HIDDEN);
            else
                lv_obj_add_flag(chat_thinking_widget_, LV_OBJ_FLAG_HIDDEN);
        }

        // Mic button color
        if (chat_mic_btn_) {
            if (new_state == kChatRecording)
                lv_obj_set_style_bg_color(chat_mic_btn_, LV_COLOR_MAKE(0xEF, 0x44, 0x44), 0);
            else
                lv_obj_set_style_bg_color(chat_mic_btn_, GetCurrentAgentColor(), 0);
        }
    }

    static int WifiRssiToBars(int rssi) {
        if (rssi >= -55) return 4;
        if (rssi >= -67) return 3;
        if (rssi >= -75) return 2;
        if (rssi < 0) return 1;
        return 0;
    }

    static std::string FormatWifiSignal(int rssi) {
        int bars = WifiRssiToBars(rssi);
        std::string text = "信号：";
        text += std::to_string(bars);
        text += "/4";
        if (rssi < 0) {
            text += " (";
            text += std::to_string(rssi);
            text += "dBm)";
        }
        return text;
    }

    static std::string FormatUptime() {
        int64_t total_seconds = esp_timer_get_time() / 1000000;
        int hours = static_cast<int>(total_seconds / 3600);
        int minutes = static_cast<int>((total_seconds % 3600) / 60);
        char buf[24];
        snprintf(buf, sizeof(buf), "%dh %02dm", hours, minutes);
        return std::string(buf);
    }

    void CycleTab() {
        if (display_ == nullptr) {
            return;
        }
        DisplayLockGuard lock(this);
        SwitchTab((active_tab_ + 1) % 4);
        ForceCustomUiVisible();
        lv_obj_invalidate(lv_screen_active());
    }

    int ActiveTab() const {
        return active_tab_;
    }

    void ActivateMicFromFallback() {
        if (display_ == nullptr) {
            return;
        }
        DisplayLockGuard lock(this);
        if (active_tab_ != 2) {
            SwitchTab(2);
        }
        ToggleMic();
        ForceCustomUiVisible();
        lv_obj_invalidate(lv_screen_active());
    }

    void StartWifiConfigFromFallback() {
        if (display_ == nullptr) {
            return;
        }
        DisplayLockGuard lock(this);
        if (active_tab_ != 1) {
            SwitchTab(1);
        }
        StartWifiConfigFromUi();
        ForceCustomUiVisible();
        lv_obj_invalidate(lv_screen_active());
    }

    void SetWifiConfigHandler(std::function<void()> handler) {
        wifi_config_handler_ = std::move(handler);
    }

private:
    void CreateHiddenSystemLabels(lv_obj_t* screen) {
        network_label_ = lv_label_create(screen);
        status_label_ = lv_label_create(screen);
        notification_label_ = lv_label_create(screen);
        mute_label_ = lv_label_create(screen);
        battery_label_ = lv_label_create(screen);

        lv_obj_t* labels[] = {
            network_label_, status_label_, notification_label_, mute_label_, battery_label_
        };
        for (auto* label : labels) {
            lv_label_set_text(label, "");
            lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
        }

        low_battery_popup_ = lv_obj_create(screen);
        lv_obj_set_size(low_battery_popup_, 1, 1);
        lv_obj_set_style_bg_opa(low_battery_popup_, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(low_battery_popup_, 0, 0);
        lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);
    }

    void ForceCustomUiVisible() {
        auto screen = lv_screen_active();
        lv_obj_set_style_bg_color(screen, kBgBlack, 0);
        lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_image_src(screen, nullptr, 0);

        if (container_) {
            lv_obj_remove_flag(container_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_bg_color(container_, kBgBlack, 0);
            lv_obj_set_style_bg_opa(container_, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_image_src(container_, nullptr, 0);
            lv_obj_set_style_bg_image_opa(container_, LV_OPA_TRANSP, 0);
            lv_obj_move_foreground(container_);
        }

        HideBaseOverlayObjects();

        if (top_status_bar_) {
            lv_obj_remove_flag(top_status_bar_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(top_status_bar_);
        }
        if (content_area_) {
            lv_obj_remove_flag(content_area_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(content_area_);
        }
        if (tab_bar_) {
            lv_obj_remove_flag(tab_bar_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(tab_bar_);
        }
    }

    void RefreshNow() {
        if (display_ == nullptr) return;
        auto screen = lv_screen_active();
        lv_obj_update_layout(screen);
        lv_obj_invalidate(screen);
        lv_refr_now(display_);
    }

    void HideBaseOverlayObjects() {
        if (emoji_label_) lv_obj_add_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);
        if (emoji_image_) lv_obj_add_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);
        if (emoji_box_) lv_obj_add_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
        if (preview_image_) lv_obj_add_flag(preview_image_, LV_OBJ_FLAG_HIDDEN);
        if (low_battery_popup_) lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);
        if (status_bar_) lv_obj_add_flag(status_bar_, LV_OBJ_FLAG_HIDDEN);
    }

    // ---- custom lvgl objects ----
    // Top bar
    lv_obj_t* top_status_bar_       = nullptr;
    lv_obj_t* time_label_           = nullptr;
    lv_obj_t* custom_wifi_label_    = nullptr;
    lv_obj_t* custom_battery_label_ = nullptr;

    // Content area & pages
    lv_obj_t* content_area_ = nullptr;
    lv_obj_t* page_status_  = nullptr;
    lv_obj_t* page_wifi_    = nullptr;
    lv_obj_t* page_voice_   = nullptr;
    lv_obj_t* page_device_  = nullptr;

    // Tab bar
    lv_obj_t* tab_bar_ = nullptr;
    lv_obj_t* tab_btns_[4] = {};
    int       active_tab_ = 0;    // PRD default: Status tab

    // Status page widgets
    lv_obj_t* jarvis_progress_ = nullptr;
    lv_obj_t* hermes_progress_ = nullptr;
    lv_obj_t* jarvis_status_label_ = nullptr;
    lv_obj_t* hermes_status_label_ = nullptr;
    lv_obj_t* jarvis_action_label_ = nullptr;
    lv_obj_t* hermes_action_label_ = nullptr;
    lv_obj_t* active_tasks_header_ = nullptr;
    lv_obj_t* recent_done_summary_label_ = nullptr;
    lv_obj_t* done_tasks_header_ = nullptr;
    lv_obj_t* task_labels_[6] = {};
    lv_obj_t* active_tasks_more_label_ = nullptr;
    lv_obj_t* done_labels_[3] = {};

    // WiFi page widgets
    lv_obj_t* wifi_status_label_ = nullptr;
    lv_obj_t* wifi_ssid_label_   = nullptr;
    lv_obj_t* wifi_ip_label_     = nullptr;
    lv_obj_t* wifi_signal_label_ = nullptr;
    lv_obj_t* wifi_saved_header_ = nullptr;
    lv_obj_t* wifi_saved_empty_label_ = nullptr;
    lv_obj_t* wifi_saved_rows_[kMaxSavedWifiRows] = {};
    lv_obj_t* wifi_saved_name_labels_[kMaxSavedWifiRows] = {};
    lv_obj_t* wifi_saved_switch_labels_[kMaxSavedWifiRows] = {};
    int wifi_saved_indices_[kMaxSavedWifiRows] = {};
    int wifi_switching_index_ = -1;
    lv_obj_t* wifi_config_btn_   = nullptr;
    lv_obj_t* wifi_config_text_  = nullptr;
    bool wifi_config_active_ = false;
    std::function<void()> wifi_config_handler_;

    // Chat page widgets (v2.2 — Agent routing chat interface)
    lv_obj_t* chat_agent_bar_     = nullptr;   // Agent routing bar (top)
    lv_obj_t* chat_agent_icon_    = nullptr;
    lv_obj_t* chat_agent_name_    = nullptr;
    lv_obj_t* chat_agent_status_  = nullptr;
    lv_obj_t* chat_route_label_   = nullptr;
    lv_obj_t* chat_msg_area_      = nullptr;   // Scrollable message bubble area
    lv_obj_t* chat_guidance_      = nullptr;
    lv_obj_t* chat_bubbles_[5]    = {};        // Max 5 message bubbles
    int       chat_bubble_idx_    = 0;         // Ring buffer index
    int       chat_bubble_count_  = 0;         // Active bubble count
    lv_obj_t* chat_waveform_      = nullptr;   // Waveform area (recording)
    lv_obj_t* chat_wave_bars_[8]  = {};        // 8 waveform bars
    lv_obj_t* chat_bottom_bar_    = nullptr;   // Bottom input bar
    lv_obj_t* chat_mic_btn_       = nullptr;   // Small mic button
    lv_obj_t* chat_hint_label_    = nullptr;   // Hint text
    lv_obj_t* chat_thinking_widget_= nullptr;   // Thinking animation container
    lv_obj_t* device_wifi_label_  = nullptr;
    lv_obj_t* device_uptime_label_ = nullptr;
    lv_obj_t* device_battery_label_ = nullptr;

    // Chat state machine (defined early for forward reference)

    ChatState chat_state_ = kChatIdle;
    std::string current_agent_name_ = "小智";

    // ==================================================================
    // Helper: create a full-size page container inside parent
    // ==================================================================
    static lv_obj_t* CreatePageContainer(lv_obj_t* parent) {
        lv_obj_t* page = lv_obj_create(parent);
        lv_obj_set_pos(page, 0, 0);
        lv_obj_set_size(page, kPrdUiW, kContentH);
        lv_obj_set_style_bg_color(page, kBgBlack, 0);
        lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(page, 0, 0);
        lv_obj_set_style_radius(page, 0, 0);
        lv_obj_set_style_pad_left(page, kPagePadX, 0);
        lv_obj_set_style_pad_right(page, kPagePadX, 0);
        lv_obj_set_style_pad_top(page, 0, 0);
        lv_obj_set_style_pad_bottom(page, 0, 0);
        lv_obj_set_style_pad_row(page, 6, 0);
        lv_obj_set_flex_flow(page, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(page, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_scrollbar_mode(page, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_flag(page, LV_OBJ_FLAG_SCROLLABLE);
        return page;
    }

    // ==================================================================
    // Helper: create a single tab button
    // ==================================================================
    void CreateTabBtn(lv_obj_t* parent, const char* icon, const char* label,
                      int idx, const lv_font_t* icon_font, const lv_font_t* text_font) {
        lv_obj_t* btn = lv_obj_create(parent);
        lv_obj_set_size(btn, kPrdUiW / 4 - 2, kBottomBarH - 4);
        lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_radius(btn, 6, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

        // Highlight active tab
        if (idx == active_tab_) {
            lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
        }

        (void)icon;
        (void)icon_font;

        lv_obj_t* txt = lv_label_create(btn);
        lv_label_set_text(txt, label);
        lv_obj_set_style_text_font(txt, text_font, 0);
        lv_obj_set_style_text_color(txt, idx == active_tab_ ? kTabActive : kTextMuted, 0);
        lv_label_set_long_mode(txt, LV_LABEL_LONG_DOT);
        lv_obj_set_width(txt, kPrdUiW / 4 - 8);
        lv_obj_set_style_text_align(txt, LV_TEXT_ALIGN_CENTER, 0);

        // Callback
        lv_obj_add_event_cb(btn, TabBtnEventHandler, LV_EVENT_CLICKED,
                            (void*)(intptr_t)idx);

        tab_btns_[idx] = btn;
    }

    // ==================================================================
    // Tab click handler — switch pages
    // ==================================================================
    static void TabBtnEventHandler(lv_event_t* e) {
        int idx = (int)(intptr_t)lv_event_get_user_data(e);
        // Walk up to find the tab_bar_ which has our 'this' in user_data
        lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
        lv_obj_t* bar = lv_obj_get_parent(btn);
        auto* self = static_cast<CustomLcdDisplay*>(lv_obj_get_user_data(bar));
        if (!self) return;
        self->SwitchTab(idx);
    }

    static const char* TabName(int idx) {
        switch (idx) {
            case 0: return "Status";
            case 1: return "WiFi";
            case 2: return "Conversation";
            case 3: return "Device";
            default: return "Unknown";
        }
    }

    void SwitchTab(int idx) {
        if (idx < 0 || idx > 3 || idx == active_tab_) return;
        active_tab_ = idx;
        ESP_LOGI(TAG, "UI tab selected: %s (%d)", TabName(idx), idx);

        lv_obj_t* pages[] = { page_status_, page_wifi_, page_voice_, page_device_ };
        for (int i = 0; i < 4; i++) {
            if (i == idx) lv_obj_remove_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
            else          lv_obj_add_flag(pages[i], LV_OBJ_FLAG_HIDDEN);

            // Update tab button highlight
            lv_obj_t* btn = tab_btns_[i];
            if (!btn) continue;
            uint32_t child_cnt = lv_obj_get_child_cnt(btn);
            for (uint32_t c = 0; c < child_cnt; c++) {
                lv_obj_t* child = lv_obj_get_child(btn, c);
                lv_obj_set_style_text_color(child,
                    i == idx ? kTabActive : kTextMuted, 0);
            }
            if (i == idx) {
                lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
            } else {
                lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
            }
        }
        if (idx == 1) RefreshWifiInfo();
        if (idx == 3) RefreshDeviceInfo();
        ForceCustomUiVisible();
        RefreshNow();
    }

    // ==================================================================
    // Page 1 — Status / Agent Overview
    // ==================================================================
    void BuildStatusPage(lv_obj_t* page, const lv_font_t* font, const lv_font_t* icon_font) {
        lv_obj_t* page_label = lv_label_create(page);
        lv_label_set_text(page_label, "Status 状态总览");
        lv_obj_set_style_text_font(page_label, font, 0);
        lv_obj_set_style_text_color(page_label, kWhite, 0);
        lv_obj_set_width(page_label, kPrdUiW - kPagePadX * 2);
        lv_label_set_long_mode(page_label, LV_LABEL_LONG_DOT);
        lv_obj_set_style_margin_top(page_label, 6, 0);
        lv_obj_set_style_margin_bottom(page_label, 4, 0);

        // --- Agent cards ---
        BuildAgentCard(page, "贾维斯", kJarvisBlue, kOnlineGreen,
                       "在线", "等待指令 - 无活跃任务", 32, font, icon_font);
        BuildAgentCard(page, "Hermes", kHermesPurp, kBusyYellow,
                       "忙碌", "编码中 - Pulse App 3D", 67, font, icon_font);

        // --- Active tasks header ---
        active_tasks_header_ = lv_label_create(page);
        lv_label_set_text(active_tasks_header_, "活跃任务 · 3");
        lv_obj_set_style_text_font(active_tasks_header_, font, 0);
        lv_obj_set_style_text_color(active_tasks_header_, kTextMuted, 0);
        lv_obj_set_style_margin_top(active_tasks_header_, 4, 0);

        recent_done_summary_label_ = lv_label_create(page);
        lv_label_set_text(recent_done_summary_label_, "最近完成 · 0 · 暂无完成任务");
        lv_obj_set_style_text_font(recent_done_summary_label_, font, 0);
        lv_obj_set_style_text_color(recent_done_summary_label_, kOfflineGray, 0);
        lv_obj_set_style_pad_left(recent_done_summary_label_, 8, 0);

        const char* tasks[] = {
            "Agent 唤醒词路由        Hermes   0%",
            "状态 API 开发           贾维斯  30%",
            "WiFi BluFi 配网         Hermes  60%",
            "",
            "",
            "",
        };
        for (size_t i = 0; i < 6; ++i) {
            lv_obj_t* lbl = lv_label_create(page);
            lv_label_set_text(lbl, tasks[i]);
            lv_obj_set_style_text_font(lbl, font, 0);
            lv_obj_set_style_text_color(lbl, kTextSoft, 0);
            lv_obj_set_style_pad_left(lbl, 8, 0);
            lv_obj_set_width(lbl, kPrdUiW - kPagePadX * 2 - 8);
            lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
            task_labels_[i] = lbl;
        }
        active_tasks_more_label_ = lv_label_create(page);
        lv_label_set_text(active_tasks_more_label_, "");
        lv_obj_set_style_text_font(active_tasks_more_label_, font, 0);
        lv_obj_set_style_text_color(active_tasks_more_label_, kTextMuted, 0);
        lv_obj_set_style_pad_left(active_tasks_more_label_, 8, 0);

        // --- Recently completed ---
        done_tasks_header_ = lv_label_create(page);
        lv_label_set_text(done_tasks_header_, "最近完成 · 2");
        lv_obj_set_style_text_font(done_tasks_header_, font, 0);
        lv_obj_set_style_text_color(done_tasks_header_, kTextMuted, 0);
        lv_obj_set_style_margin_top(done_tasks_header_, 4, 0);

        const char* done[] = {
            "语音对话全链路          Hermes  2h前",
            "ASR 部署                Hermes  1d前",
            "",
        };
        for (size_t i = 0; i < 3; ++i) {
            lv_obj_t* lbl = lv_label_create(page);
            lv_label_set_text(lbl, done[i]);
            lv_obj_set_style_text_font(lbl, font, 0);
            lv_obj_set_style_text_color(lbl, kOfflineGray, 0);
            lv_obj_set_style_pad_left(lbl, 8, 0);
            lv_obj_set_width(lbl, kPrdUiW - kPagePadX * 2 - 8);
            lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
            done_labels_[i] = lbl;
        }
    }

    void BuildAgentCard(lv_obj_t* parent, const char* name,
                        lv_color_t accent, lv_color_t status_color,
                        const char* status_text, const char* action_text,
                        int progress_pct,
                        const lv_font_t* font, const lv_font_t* icon_font) {
        // Card container
        lv_obj_t* card = lv_obj_create(parent);
        lv_obj_set_width(card, kPrdUiW - kPagePadX * 2);
        lv_obj_set_height(card, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(card, LV_COLOR_MAKE(0x0D, 0x11, 0x17), 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(card, LV_COLOR_MAKE(0x21, 0x26, 0x2D), 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_radius(card, 8, 0);
        lv_obj_set_style_pad_all(card, 10, 0);
        lv_obj_set_style_pad_row(card, 5, 0);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);

        // Row 1: name + status badge
        lv_obj_t* row1 = lv_obj_create(card);
        lv_obj_set_size(row1, kPrdUiW - kPagePadX * 2 - 20, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(row1, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row1, 0, 0);
        lv_obj_set_style_pad_all(row1, 0, 0);
        lv_obj_set_flex_flow(row1, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row1, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        // Agent icon
        lv_obj_t* agent_ico = lv_label_create(row1);
        lv_label_set_text(agent_ico, strcmp(name, "Hermes") == 0 ? FONT_AWESOME_CLOUD_BOLT : FONT_AWESOME_USER_ROBOT);
        lv_obj_set_style_text_font(agent_ico, icon_font, 0);
        lv_obj_set_style_text_color(agent_ico, accent, 0);
        lv_obj_set_style_margin_right(agent_ico, 4, 0);

        lv_obj_t* name_lbl = lv_label_create(row1);
        lv_label_set_text(name_lbl, name);
        lv_obj_set_style_text_font(name_lbl, font, 0);
        lv_obj_set_style_text_color(name_lbl, kWhite, 0);

        // spacer
        lv_obj_t* sp = lv_obj_create(row1);
        lv_obj_set_size(sp, 1, 1);
        lv_obj_set_style_bg_opa(sp, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(sp, 0, 0);
        lv_obj_set_flex_grow(sp, 1);

        // Status badge
        lv_obj_t* badge = lv_label_create(row1);
        lv_label_set_text(badge, status_text);
        lv_obj_set_style_text_font(badge, font, 0);
        lv_obj_set_style_text_color(badge, status_color, 0);
        if (strcmp(name, "贾维斯") == 0) jarvis_status_label_ = badge;
        else                              hermes_status_label_ = badge;

        // Row 2: current action
        lv_obj_t* action_lbl = lv_label_create(card);
        lv_label_set_text(action_lbl, action_text);
        lv_obj_set_style_text_font(action_lbl, font, 0);
        lv_obj_set_style_text_color(action_lbl, kTextMuted, 0);
        lv_obj_set_width(action_lbl, kPrdUiW - kPagePadX * 2 - 20);
        lv_label_set_long_mode(action_lbl, LV_LABEL_LONG_DOT);
        if (strcmp(name, "贾维斯") == 0) jarvis_action_label_ = action_lbl;
        else                              hermes_action_label_ = action_lbl;

        // Row 3: progress bar (using lv_obj, not lv_bar)
        lv_obj_t* bar_bg = lv_obj_create(card);
        lv_obj_set_size(bar_bg, kPrdUiW - kPagePadX * 2 - 20, 4);
        lv_obj_set_style_bg_color(bar_bg, kProgressBg, 0);
        lv_obj_set_style_bg_opa(bar_bg, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(bar_bg, 0, 0);
        lv_obj_set_style_radius(bar_bg, 3, 0);
        lv_obj_set_style_pad_all(bar_bg, 0, 0);
        lv_obj_clear_flag(bar_bg, LV_OBJ_FLAG_SCROLLABLE);

        // Progress fill
        lv_obj_t* bar_fill = lv_obj_create(bar_bg);
        int fill_w = ((kPrdUiW - kPagePadX * 2 - 20) * progress_pct) / 100;
        lv_obj_set_size(bar_fill, fill_w > 2 ? fill_w : 2, 4);
        lv_obj_set_pos(bar_fill, 0, 0);
        lv_obj_set_style_bg_color(bar_fill, accent, 0);
        lv_obj_set_style_bg_opa(bar_fill, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(bar_fill, 0, 0);
        lv_obj_set_style_radius(bar_fill, 3, 0);
        lv_obj_set_style_pad_all(bar_fill, 0, 0);
        lv_obj_clear_flag(bar_fill, LV_OBJ_FLAG_SCROLLABLE);

        // Store progress fill objects for future updates
        if (strcmp(name, "贾维斯") == 0) jarvis_progress_ = bar_fill;
        else                              hermes_progress_ = bar_fill;
    }

    // ==================================================================
    // Page 2 — WiFi
    // ==================================================================
    void BuildWifiPage(lv_obj_t* page, const lv_font_t* font) {
        // Section title
        lv_obj_t* title = lv_label_create(page);
        lv_label_set_text(title, "WiFi 网络设置");
        lv_obj_set_style_text_font(title, font, 0);
        lv_obj_set_style_text_color(title, kWhite, 0);

        // Status card
        lv_obj_t* card = lv_obj_create(page);
        lv_obj_set_width(card, kPrdUiW - 8);
        lv_obj_set_height(card, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(card, kCardBg, 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(card, 0, 0);
        lv_obj_set_style_radius(card, 8, 0);
        lv_obj_set_style_pad_all(card, 8, 0);
        lv_obj_set_style_pad_row(card, 6, 0);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);

        wifi_status_label_ = lv_label_create(card);
        lv_label_set_text(wifi_status_label_, "状态：连接中...");
        lv_obj_set_style_text_font(wifi_status_label_, font, 0);
        lv_obj_set_style_text_color(wifi_status_label_, kWhite, 0);

        wifi_ssid_label_ = lv_label_create(card);
        lv_label_set_text(wifi_ssid_label_, "WiFi：--");
        lv_obj_set_style_text_font(wifi_ssid_label_, font, 0);
        lv_obj_set_style_text_color(wifi_ssid_label_, kDimGray, 0);

        wifi_ip_label_ = lv_label_create(card);
        lv_label_set_text(wifi_ip_label_, "IP：--");
        lv_obj_set_style_text_font(wifi_ip_label_, font, 0);
        lv_obj_set_style_text_color(wifi_ip_label_, kDimGray, 0);

        wifi_signal_label_ = lv_label_create(card);
        lv_label_set_text(wifi_signal_label_, "信号：--");
        lv_obj_set_style_text_font(wifi_signal_label_, font, 0);
        lv_obj_set_style_text_color(wifi_signal_label_, kTabActive, 0);

        wifi_saved_header_ = lv_label_create(page);
        lv_label_set_text(wifi_saved_header_, "已保存 · 0");
        lv_obj_set_style_text_font(wifi_saved_header_, font, 0);
        lv_obj_set_style_text_color(wifi_saved_header_, kTextMuted, 0);

        wifi_saved_empty_label_ = lv_label_create(page);
        lv_label_set_text(wifi_saved_empty_label_, "暂无已保存网络");
        lv_obj_set_style_text_font(wifi_saved_empty_label_, font, 0);
        lv_obj_set_style_text_color(wifi_saved_empty_label_, kOfflineGray, 0);
        lv_obj_set_style_pad_left(wifi_saved_empty_label_, 8, 0);

        for (int i = 0; i < kMaxSavedWifiRows; ++i) {
            wifi_saved_indices_[i] = -1;
            lv_obj_t* row = lv_obj_create(page);
            lv_obj_set_width(row, kPrdUiW - 8);
            lv_obj_set_height(row, 44);
            lv_obj_set_style_bg_color(row, kCardBg, 0);
            lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(row, 0, 0);
            lv_obj_set_style_radius(row, 6, 0);
            lv_obj_set_style_pad_left(row, 8, 0);
            lv_obj_set_style_pad_right(row, 8, 0);
            lv_obj_set_style_pad_top(row, 0, 0);
            lv_obj_set_style_pad_bottom(row, 0, 0);
            lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_user_data(row, this);
            lv_obj_add_event_cb(row, SavedWifiRowEventHandler, LV_EVENT_CLICKED,
                                reinterpret_cast<void*>(static_cast<intptr_t>(i)));

            wifi_saved_name_labels_[i] = lv_label_create(row);
            lv_label_set_text(wifi_saved_name_labels_[i], "--");
            lv_obj_set_style_text_font(wifi_saved_name_labels_[i], font, 0);
            lv_obj_set_style_text_color(wifi_saved_name_labels_[i], kTextSoft, 0);
            lv_obj_set_width(wifi_saved_name_labels_[i], kPrdUiW - 120);
            lv_label_set_long_mode(wifi_saved_name_labels_[i], LV_LABEL_LONG_DOT);

            lv_obj_t* spacer = lv_obj_create(row);
            lv_obj_set_size(spacer, 1, 1);
            lv_obj_set_style_bg_opa(spacer, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(spacer, 0, 0);
            lv_obj_set_flex_grow(spacer, 1);

            wifi_saved_switch_labels_[i] = lv_label_create(row);
            lv_label_set_text(wifi_saved_switch_labels_[i], "切换");
            lv_obj_set_style_text_font(wifi_saved_switch_labels_[i], font, 0);
            lv_obj_set_style_text_color(wifi_saved_switch_labels_[i], kTabActive, 0);
            wifi_saved_rows_[i] = row;
            lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
        }

        lv_obj_t* hint = lv_label_create(page);
        lv_label_set_text(hint, "手机蓝牙发送 WiFi 密码给板子");
        lv_obj_set_style_text_font(hint, font, 0);
        lv_obj_set_style_text_color(hint, kDimGray, 0);
        lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(hint, kPrdUiW - 8);

        wifi_config_btn_ = lv_obj_create(page);
        lv_obj_set_size(wifi_config_btn_, kPrdUiW - 8, 44);
        lv_obj_set_style_bg_color(wifi_config_btn_, kJarvisBlue, 0);
        lv_obj_set_style_bg_opa(wifi_config_btn_, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(wifi_config_btn_, 0, 0);
        lv_obj_set_style_radius(wifi_config_btn_, 8, 0);
        lv_obj_set_style_pad_all(wifi_config_btn_, 0, 0);
        lv_obj_add_flag(wifi_config_btn_, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(wifi_config_btn_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(wifi_config_btn_, WifiConfigBtnEventHandler, LV_EVENT_CLICKED, this);

        wifi_config_text_ = lv_label_create(wifi_config_btn_);
        lv_label_set_text(wifi_config_text_, "开启 BluFi 蓝牙配网");
        lv_obj_set_style_text_font(wifi_config_text_, font, 0);
        lv_obj_set_style_text_color(wifi_config_text_, kWhite, 0);
        lv_obj_center(wifi_config_text_);

        RefreshWifiInfo();
    }

    static void WifiConfigBtnEventHandler(lv_event_t* e) {
        auto* self = static_cast<CustomLcdDisplay*>(lv_event_get_user_data(e));
        if (!self) return;
        self->StartWifiConfigFromUi();
    }

    void StartWifiConfigFromUi() {
        ESP_LOGI(TAG, "UI action: start BluFi provisioning from WiFi page");
        wifi_config_active_ = true;
        RefreshWifiInfo();
        if (wifi_config_handler_) {
            wifi_config_handler_();
        }
    }

    static void SavedWifiRowEventHandler(lv_event_t* e) {
        auto* row = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
        auto* self = static_cast<CustomLcdDisplay*>(lv_obj_get_user_data(row));
        if (!self) return;
        int row_index = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
        self->SwitchSavedWifi(row_index);
    }

    void SwitchSavedWifi(int row_index) {
        if (row_index < 0 || row_index >= kMaxSavedWifiRows) return;
        int ssid_index = wifi_saved_indices_[row_index];
        const auto& ssids = SsidManager::GetInstance().GetSsidList();
        if (ssid_index < 0 || ssid_index >= static_cast<int>(ssids.size())) return;

        ESP_LOGI(TAG, "UI action: switch saved WiFi to %s (index=%d)",
                 ssids[ssid_index].ssid.c_str(), ssid_index);
        wifi_switching_index_ = ssid_index;
        if (wifi_status_label_) {
            lv_label_set_text(wifi_status_label_, "状态：连接中...");
            lv_obj_set_style_text_color(wifi_status_label_, kBusyYellow, 0);
        }
        SsidManager::GetInstance().SetDefaultSsid(ssid_index);
        auto& wifi = WifiManager::GetInstance();
        wifi.StopStation();
        wifi.StartStation();
        RefreshWifiInfo();
    }

    void RefreshWifiInfo() {
        if (!wifi_status_label_ && !wifi_ssid_label_ && !wifi_ip_label_ && !wifi_signal_label_ &&
            !wifi_saved_header_ && !wifi_config_text_) {
            return;
        }

        auto& wifi = WifiManager::GetInstance();
        const bool config_mode = wifi.IsConfigMode() || wifi_config_active_;
        const bool connected = wifi.IsConnected();

        if (connected) {
            wifi_config_active_ = false;
            wifi_switching_index_ = -1;
        }

        if (wifi_status_label_) {
            const char* status = config_mode ? "状态：配网中" :
                                 connected ? "状态：已连接" :
                                             "状态：未连接";
            lv_label_set_text(wifi_status_label_, status);
            lv_obj_set_style_text_color(wifi_status_label_,
                                        connected ? kOnlineGreen : (config_mode ? kBusyYellow : kWhite), 0);
        }

        if (wifi_ssid_label_) {
            std::string ssid = connected ? wifi.GetSsid() : (config_mode ? "Google-BluFi" : "--");
            std::string text = config_mode && !connected ? "设备名：" + ssid : "WiFi：" + ssid;
            lv_label_set_text(wifi_ssid_label_, text.c_str());
        }

        if (wifi_ip_label_) {
            std::string ip = connected ? wifi.GetIpAddress() :
                             (config_mode ? "等待手机连接..." : "--");
            std::string text = connected ? "IP：" + ip : ip;
            lv_label_set_text(wifi_ip_label_, text.c_str());
        }

        if (wifi_signal_label_) {
            std::string signal = connected ? FormatWifiSignal(wifi.GetRssi()) : "信号：--";
            lv_label_set_text(wifi_signal_label_, signal.c_str());
            lv_obj_set_style_text_color(wifi_signal_label_, connected ? kTabActive : kDimGray, 0);
        }

        if (wifi_saved_header_) {
            const auto& ssids = SsidManager::GetInstance().GetSsidList();
            const std::string current = connected ? wifi.GetSsid() : "";
            int total = 0;
            int visible = 0;
            for (int i = 0; i < static_cast<int>(ssids.size()); ++i) {
                if (!current.empty() && ssids[i].ssid == current) continue;
                ++total;
                if (visible >= kMaxSavedWifiRows) continue;
                wifi_saved_indices_[visible] = i;
                if (wifi_saved_rows_[visible]) lv_obj_remove_flag(wifi_saved_rows_[visible], LV_OBJ_FLAG_HIDDEN);
                if (wifi_saved_name_labels_[visible]) {
                    lv_label_set_text(wifi_saved_name_labels_[visible], ssids[i].ssid.c_str());
                }
                if (wifi_saved_switch_labels_[visible]) {
                    const bool switching = wifi_switching_index_ == i && !connected;
                    lv_label_set_text(wifi_saved_switch_labels_[visible], switching ? "连接中..." : "切换");
                    lv_obj_set_style_text_color(wifi_saved_switch_labels_[visible],
                                                switching ? kBusyYellow : kTabActive, 0);
                }
                ++visible;
            }
            std::string header = "已保存 · ";
            header += std::to_string(total);
            if (total > kMaxSavedWifiRows) header += "（显示10）";
            lv_label_set_text(wifi_saved_header_, header.c_str());
            if (wifi_saved_empty_label_) {
                if (total == 0) lv_obj_remove_flag(wifi_saved_empty_label_, LV_OBJ_FLAG_HIDDEN);
                else lv_obj_add_flag(wifi_saved_empty_label_, LV_OBJ_FLAG_HIDDEN);
            }
            for (int i = visible; i < kMaxSavedWifiRows; ++i) {
                wifi_saved_indices_[i] = -1;
                if (wifi_saved_rows_[i]) lv_obj_add_flag(wifi_saved_rows_[i], LV_OBJ_FLAG_HIDDEN);
            }
        }

        if (wifi_config_btn_) {
            lv_obj_set_style_bg_color(wifi_config_btn_, config_mode ? kOnlineGreen : kJarvisBlue, 0);
        }
        if (wifi_config_text_) {
            lv_label_set_text(wifi_config_text_,
                              config_mode ? "配网中... 连接 Google-BluFi" :
                                            "开启 BluFi 蓝牙配网");
            lv_obj_center(wifi_config_text_);
        }
    }

    // ==================================================================
    // Page 3 — Chat (Agent Routing Voice Conversation) v2.2
    // ==================================================================
    void BuildVoicePage(lv_obj_t* page, const lv_font_t* font) {
        lv_obj_t* title = lv_label_create(page);
        lv_label_set_text(title, "Conversation 对话");
        lv_obj_set_style_text_font(title, font, 0);
        lv_obj_set_style_text_color(title, kWhite, 0);
        lv_obj_set_style_margin_bottom(title, 2, 0);

        // --- Region 1: Agent Routing Bar (~28px) ---
        chat_agent_bar_ = lv_obj_create(page);
        lv_obj_set_size(chat_agent_bar_, kPrdUiW - 8, 28);
        lv_obj_set_style_bg_opa(chat_agent_bar_, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(chat_agent_bar_, 0, 0);
        lv_obj_set_style_pad_all(chat_agent_bar_, 0, 0);
        lv_obj_clear_flag(chat_agent_bar_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(chat_agent_bar_, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(chat_agent_bar_,
                              LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        // Agent icon
        chat_agent_icon_ = lv_label_create(chat_agent_bar_);
        lv_label_set_text(chat_agent_icon_, FONT_AWESOME_VOLUME_HIGH);
        lv_obj_set_style_text_color(chat_agent_icon_, kDimGray, 0);

        // Agent name
        chat_agent_name_ = lv_label_create(chat_agent_bar_);
        lv_label_set_text(chat_agent_name_, "小智");
        lv_obj_set_style_text_font(chat_agent_name_, font, 0);
        lv_obj_set_style_text_color(chat_agent_name_, kWhite, 0);
        lv_obj_set_style_margin_left(chat_agent_name_, 4, 0);

        // Spacer
        lv_obj_t* asp = lv_obj_create(chat_agent_bar_);
        lv_obj_set_size(asp, 1, 1);
        lv_obj_set_style_bg_opa(asp, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(asp, 0, 0);
        lv_obj_set_flex_grow(asp, 1);

        // Status badge
        chat_agent_status_ = lv_label_create(chat_agent_bar_);
        lv_label_set_text(chat_agent_status_, "待命");
        lv_obj_set_style_text_font(chat_agent_status_, font, 0);
        lv_obj_set_style_text_color(chat_agent_status_, kOfflineGray, 0);

        chat_route_label_ = lv_label_create(chat_agent_bar_);
        lv_label_set_text(chat_route_label_, "→ 小智");
        lv_obj_set_style_text_font(chat_route_label_, font, 0);
        lv_obj_set_style_text_color(chat_route_label_, kDimGray, 0);
        lv_obj_set_style_border_width(chat_route_label_, 1, 0);
        lv_obj_set_style_border_color(chat_route_label_, kDimGray, 0);
        lv_obj_set_style_radius(chat_route_label_, 6, 0);
        lv_obj_set_style_pad_left(chat_route_label_, 4, 0);
        lv_obj_set_style_pad_right(chat_route_label_, 4, 0);
        lv_obj_set_style_margin_left(chat_route_label_, 6, 0);

        // --- Region 2: Message Bubble Area (fills remaining space) ---
        chat_msg_area_ = lv_obj_create(page);
        lv_obj_set_width(chat_msg_area_, kPrdUiW - 8);
        lv_obj_set_flex_grow(chat_msg_area_, 1);
        lv_obj_set_style_bg_opa(chat_msg_area_, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(chat_msg_area_, 0, 0);
        lv_obj_set_style_pad_all(chat_msg_area_, 4, 0);
        lv_obj_set_style_pad_row(chat_msg_area_, 6, 0);
        lv_obj_set_flex_flow(chat_msg_area_, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(chat_msg_area_,
                              LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_START);
        lv_obj_set_scrollbar_mode(chat_msg_area_, LV_SCROLLBAR_MODE_OFF);

        // Idle guidance text (shown when no messages)
        chat_guidance_ = lv_obj_create(chat_msg_area_);
        lv_obj_set_width(chat_guidance_, lv_pct(100));
        lv_obj_set_height(chat_guidance_, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(chat_guidance_, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(chat_guidance_, 0, 0);
        lv_obj_set_flex_flow(chat_guidance_, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(chat_guidance_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t* gtxt = lv_label_create(chat_guidance_);
        lv_label_set_text(gtxt, "先说 \"你好小智\" 唤醒\n再说 \"贾维斯\" 或 \"Hermes\" 路由\n也可点击麦克风");
        lv_label_set_long_mode(gtxt, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(gtxt, font, 0);
        lv_obj_set_style_text_color(gtxt, kDimGray, 0);
        lv_obj_set_style_text_align(gtxt, LV_TEXT_ALIGN_CENTER, 0);

        // --- Region 3: Waveform Area (hidden by default) ---
        chat_waveform_ = lv_obj_create(page);
        lv_obj_set_size(chat_waveform_, kPrdUiW - 8, 20);
        lv_obj_set_style_bg_opa(chat_waveform_, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(chat_waveform_, 0, 0);
        lv_obj_set_style_pad_all(chat_waveform_, 0, 0);
        lv_obj_add_flag(chat_waveform_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_flex_flow(chat_waveform_, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(chat_waveform_,
                              LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(chat_waveform_, LV_OBJ_FLAG_SCROLLABLE);

        // 8 waveform bars
        for (int i = 0; i < 8; i++) {
            chat_wave_bars_[i] = lv_obj_create(chat_waveform_);
            lv_obj_set_size(chat_wave_bars_[i], 12, 4);
            lv_obj_set_style_bg_color(chat_wave_bars_[i], LV_COLOR_MAKE(0xEF, 0x44, 0x44), 0);
            lv_obj_set_style_bg_opa(chat_wave_bars_[i], LV_OPA_COVER, 0);
            lv_obj_set_style_radius(chat_wave_bars_[i], 2, 0);
            lv_obj_set_style_border_width(chat_wave_bars_[i], 0, 0);
            lv_obj_clear_flag(chat_wave_bars_[i], LV_OBJ_FLAG_SCROLLABLE);
        }

        // --- Region 4: Thinking Animation (hidden by default) ---
        chat_thinking_widget_ = lv_obj_create(page);
        lv_obj_set_size(chat_thinking_widget_, kPrdUiW - 8, 32);
        lv_obj_set_style_bg_opa(chat_thinking_widget_, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(chat_thinking_widget_, 0, 0);
        lv_obj_add_flag(chat_thinking_widget_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_flex_flow(chat_thinking_widget_, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(chat_thinking_widget_,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(chat_thinking_widget_, LV_OBJ_FLAG_SCROLLABLE);

        // Thinking label + bouncing dots
        lv_obj_t* think_lbl = lv_label_create(chat_thinking_widget_);
        lv_label_set_text(think_lbl, "思考中");
        lv_obj_set_style_text_font(think_lbl, font, 0);
        lv_obj_set_style_text_color(think_lbl, kDimGray, 0);

        // Three dots
        for (int d = 0; d < 3; d++) {
            lv_obj_t* dot = lv_obj_create(chat_thinking_widget_);
            lv_obj_set_size(dot, 6, 6);
            lv_obj_set_style_bg_color(dot, kBusyYellow, 0);
            lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(dot, 3, 0);
            lv_obj_set_style_border_width(dot, 0, 0);
            lv_obj_set_style_margin_left(dot, 3, 0);
            lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
        }

        // --- Region 5: Bottom Input Bar (~44px) ---
        chat_bottom_bar_ = lv_obj_create(page);
        lv_obj_set_size(chat_bottom_bar_, kPrdUiW - 8, 44);
        lv_obj_set_style_bg_color(chat_bottom_bar_, kCardBg, 0);
        lv_obj_set_style_bg_opa(chat_bottom_bar_, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(chat_bottom_bar_, 0, 0);
        lv_obj_set_style_radius(chat_bottom_bar_, 10, 0);
        lv_obj_set_style_pad_all(chat_bottom_bar_, 0, 0);
        lv_obj_set_flex_flow(chat_bottom_bar_, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(chat_bottom_bar_,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(chat_bottom_bar_, LV_OBJ_FLAG_SCROLLABLE);

        // PRD touch target: at least 44px.
        chat_mic_btn_ = lv_obj_create(chat_bottom_bar_);
        int mic_sz = 44;
        lv_obj_set_size(chat_mic_btn_, mic_sz, mic_sz);
        lv_obj_set_style_bg_color(chat_mic_btn_, kJarvisBlue, 0);
        lv_obj_set_style_bg_opa(chat_mic_btn_, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(chat_mic_btn_, 0, 0);
        lv_obj_set_style_radius(chat_mic_btn_, mic_sz / 2, 0);
        lv_obj_add_flag(chat_mic_btn_, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(chat_mic_btn_, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* mic_ico = lv_label_create(chat_mic_btn_);
        lv_label_set_text(mic_ico, FONT_AWESOME_MICROPHONE);
        lv_obj_center(mic_ico);
        lv_obj_set_style_text_color(mic_ico, kWhite, 0);

        lv_obj_add_event_cb(chat_mic_btn_, MicBtnEventHandler, LV_EVENT_CLICKED, this);

        // Hint text
        chat_hint_label_ = lv_label_create(chat_bottom_bar_);
        lv_label_set_text(chat_hint_label_, "你好小智唤醒，或点击麦克风");
        lv_obj_set_style_text_font(chat_hint_label_, font, 0);
        lv_obj_set_style_text_color(chat_hint_label_, kDimGray, 0);
        lv_obj_set_style_margin_left(chat_hint_label_, 10, 0);
    }

    static void MicBtnEventHandler(lv_event_t* e) {
        auto* self = static_cast<CustomLcdDisplay*>(lv_event_get_user_data(e));
        if (!self) return;
        self->ToggleMic();
    }

    void ToggleMic() {
        if (!chat_mic_btn_) return;
        ESP_LOGI(TAG, "UI action: mic button tapped, chat_state=%d device_state=%d",
                 static_cast<int>(chat_state_),
                 static_cast<int>(Application::GetInstance().GetDeviceState()));
        // Toggle chat state: idle↔recording
        if (chat_state_ == kChatIdle || chat_state_ == kChatReply || chat_state_ == kChatError) {
            SetChatState(kChatRecording);
            // Trigger chat via application
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateIdle) {
                app.ToggleChatState();
            }
        } else {
            SetChatState(kChatIdle);
        }
    }

    // ==================================================================
    // Page 4 — Device Info
    // ==================================================================
    void BuildDevicePage(lv_obj_t* page, const lv_font_t* font) {
        lv_obj_t* title = lv_label_create(page);
        lv_label_set_text(title, "Device 设备信息");
        lv_obj_set_style_text_font(title, font, 0);
        lv_obj_set_style_text_color(title, kWhite, 0);

        lv_obj_t* card = lv_obj_create(page);
        lv_obj_set_width(card, kPrdUiW - 8);
        lv_obj_set_height(card, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(card, kCardBg, 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(card, 0, 0);
        lv_obj_set_style_radius(card, 8, 0);
        lv_obj_set_style_pad_all(card, 8, 0);
        lv_obj_set_style_pad_row(card, 5, 0);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);

        auto add_info = [&](const char* key, const char* val) -> lv_obj_t* {
            lv_obj_t* row = lv_obj_create(card);
            lv_obj_set_size(row, kPrdUiW - 24, LV_SIZE_CONTENT);
            lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(row, 0, 0);
            lv_obj_set_style_pad_all(row, 0, 0);
            lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
            lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

            lv_obj_t* k = lv_label_create(row);
            lv_label_set_text(k, key);
            lv_obj_set_style_text_font(k, font, 0);
            lv_obj_set_style_text_color(k, kDimGray, 0);
            lv_obj_set_width(k, 100);

            lv_obj_t* v = lv_label_create(row);
            lv_label_set_text(v, val);
            lv_obj_set_style_text_font(v, font, 0);
            lv_obj_set_style_text_color(v, kWhite, 0);
            lv_label_set_long_mode(v, LV_LABEL_LONG_WRAP);
            lv_obj_set_width(v, kPrdUiW - 128);
            return v;
        };

        add_info("设备名称",   "Google");
        add_info("运行模式",   "Local PRD WebSocket");
        add_info("服务器",     "jarvis.local:8001");
        add_info("Endpoint",  CONFIG_GOOGLE_BOARD_LOCAL_WEBSOCKET_URL);
        add_info("Fallback",  CONFIG_GOOGLE_BOARD_LOCAL_WEBSOCKET_FALLBACK_URL);
        device_uptime_label_ = add_info("运行时间", "--");
        device_battery_label_ = add_info("电池", "--");
        device_wifi_label_ = add_info("WiFi", "--");
        add_info("固件",       "xiaozhi-esp32 v1.0");
        add_info("芯片",       "ESP32-C6 160MHz");
        add_info("内存",       "512KB / 16MB");
        add_info("屏幕",       "2.06\" AMOLED 368×448");
        add_info("PRD视口",    "368×448");
        add_info("Codec",      "ES8311");
        add_info("PMIC",       "AXP2101");
        add_info("Touch",      "FT3168");
        RefreshDeviceInfo();
    }

    void RefreshDeviceInfo() {
        if (!device_wifi_label_ && !device_uptime_label_ && !device_battery_label_) return;
        auto& wifi = WifiManager::GetInstance();
        std::string value;
        if (wifi.IsConnected()) {
            value = wifi.GetSsid() + " (" + wifi.GetIpAddress() + ")";
        } else if (wifi.IsConfigMode() || wifi_config_active_) {
            value = "Google-BluFi 配网中";
        } else {
            value = "未连接";
        }
        if (device_wifi_label_) lv_label_set_text(device_wifi_label_, value.c_str());
        if (device_uptime_label_) {
            std::string uptime = FormatUptime();
            lv_label_set_text(device_uptime_label_, uptime.c_str());
        }
        if (device_battery_label_) {
            int level = 0;
            bool charging = false;
            bool discharging = false;
            if (Board::GetInstance().GetBatteryLevel(level, charging, discharging)) {
                char buf[20];
                snprintf(buf, sizeof(buf), "%d%%%s", level, charging ? " 充电中" : "");
                lv_label_set_text(device_battery_label_, buf);
            } else {
                lv_label_set_text(device_battery_label_, "--");
            }
        }
    }
};
#endif

class PanelDirectDisplay : public Display {
public:
    PanelDirectDisplay(esp_lcd_panel_handle_t panel, int width, int height)
        : panel_(panel), direct_theme_("direct") {
        width_ = width;
        height_ = height;
        current_theme_ = &direct_theme_;
        mutex_ = xSemaphoreCreateRecursiveMutex();

        ESP_LOGI(TAG, "Initialize panel direct display");
        esp_err_t ret = esp_lcd_panel_disp_on_off(panel_, true);
        if (ret != ESP_ERR_NOT_SUPPORTED) {
            ESP_ERROR_CHECK(ret);
        }
        DrawDashboard();
    }

    ~PanelDirectDisplay() override {
        if (mutex_ != nullptr) {
            vSemaphoreDelete(mutex_);
        }
    }

    void SetupUI() override {
        setup_ui_called_ = true;
        ESP_LOGI(TAG, "Panel direct dashboard ready");
    }

    void SetStatus(const char* status) override {
        ESP_LOGI(TAG, "SetStatus: %s", status ? status : "");
        status_ = NormalizeStatus(status);
        if (status_ == "LISTENING" || status_ == "SPEAKING" || status_ == "THINKING") {
            active_page_ = kPageChat;
        }
        DrawDashboard();
    }

    void ShowNotification(const char* notification, int duration_ms = 3000) override {
        (void)duration_ms;
        ESP_LOGI(TAG, "Notification: %s", notification ? notification : "");
        notice_ = NormalizeStatus(notification);
        DrawDashboard();
    }

    void ShowNotification(const std::string& notification, int duration_ms = 3000) override {
        ShowNotification(notification.c_str(), duration_ms);
    }

    void SetEmotion(const char* emotion) override {
        ESP_LOGI(TAG, "SetEmotion: %s", emotion ? emotion : "");
        emotion_ = NormalizeEmotion(emotion);
        DrawDashboard();
    }

    void SetChatMessage(const char* role, const char* content) override {
        ESP_LOGI(TAG, "Chat[%s]: %s", role ? role : "", content ? content : "");
        chat_role_ = role ? role : "";
        chat_message_ = NormalizeStatus(content);
        if (chat_role_ != "user" && chat_role_ != "assistant") {
            DrawDashboard();
            return;
        }
        if (!chat_message_.empty()) {
            std::string prefix = (chat_role_ == "user") ? "YOU " : "AI ";
            chat_history_.push_back(prefix + chat_message_);
            if (chat_history_.size() > 5) {
                chat_history_.erase(chat_history_.begin());
            }
        }
        active_page_ = kPageChat;
        DrawDashboard();
    }

    void ClearChatMessages() override {
        ESP_LOGI(TAG, "ClearChatMessages");
        chat_role_.clear();
        chat_message_.clear();
        chat_history_.clear();
        active_page_ = kPageStatus;
        DrawDashboard();
    }

    void SetTheme(Theme* theme) override {
        if (theme != nullptr) {
            current_theme_ = theme;
        }
        ESP_LOGI(TAG, "SetTheme ignored by direct display");
    }

    void UpdateStatusBar(bool update_all = false) override {
        (void)update_all;
    }

    void NextPage() {
        active_page_ = static_cast<Page>((static_cast<int>(active_page_) + 1) % kPageCount);
        DrawDashboard();
    }

    void SetPowerSaveMode(bool on) override {
        if (panel_ == nullptr) {
            return;
        }
        esp_err_t ret = esp_lcd_panel_disp_on_off(panel_, !on);
        if (ret != ESP_ERR_NOT_SUPPORTED) {
            ESP_ERROR_CHECK(ret);
        }
        if (!on) {
            DrawDashboard();
        }
    }

private:
    static constexpr int kVisibleHeight = 410;
    static constexpr int kTopBarHeight = 30;
    static constexpr int kTabBarHeight = 48;
    static constexpr int kTabTop = kVisibleHeight - kTabBarHeight;
    static constexpr int kContentBottom = kTabTop - 8;

    enum Page {
        kPageStatus = 0,
        kPageWifi = 1,
        kPageChat = 2,
        kPageDevice = 3,
        kPageCount = 4,
    };

    bool Lock(int timeout_ms = 0) override {
        if (mutex_ == nullptr) {
            return true;
        }
        TickType_t timeout = timeout_ms < 0 ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
        return xSemaphoreTakeRecursive(mutex_, timeout) == pdTRUE;
    }

    void Unlock() override {
        if (mutex_ != nullptr) {
            xSemaphoreGiveRecursive(mutex_);
        }
    }

    static uint16_t Rgb565(uint8_t r, uint8_t g, uint8_t b) {
        return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    }

    void FillRect(int x, int y, int w, int h, uint16_t color) {
        if (panel_ == nullptr || w <= 0 || h <= 0) {
            return;
        }
        x = std::max(0, x);
        y = std::max(0, y);
        w = std::min(w, width_ - x);
        h = std::min(h, height_ - y);
        if (w <= 0 || h <= 0) {
            return;
        }

        std::vector<uint16_t> line(w, color);
        for (int row = y; row < y + h; ++row) {
            ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_, x, row, x + w, row + 1, line.data()));
            if ((row & 0x1f) == 0) {
                vTaskDelay(1);
            }
        }
    }

    static const uint8_t* Glyph(char c) {
        static const uint8_t space[7] = {0, 0, 0, 0, 0, 0, 0};
        static const uint8_t unknown[7] = {0x0e, 0x11, 0x01, 0x06, 0x04, 0x00, 0x04};
        static const uint8_t digits[10][7] = {
            {0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e},
            {0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e},
            {0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f},
            {0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e},
            {0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02},
            {0x1f, 0x10, 0x1e, 0x01, 0x01, 0x11, 0x0e},
            {0x06, 0x08, 0x10, 0x1e, 0x11, 0x11, 0x0e},
            {0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
            {0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e},
            {0x0e, 0x11, 0x11, 0x0f, 0x01, 0x02, 0x0c},
        };
        static const uint8_t letters[26][7] = {
            {0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11},
            {0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e},
            {0x0e, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0e},
            {0x1e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1e},
            {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f},
            {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10},
            {0x0e, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0f},
            {0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11},
            {0x0e, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e},
            {0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0c},
            {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11},
            {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f},
            {0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11},
            {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11},
            {0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e},
            {0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10},
            {0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d},
            {0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11},
            {0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e},
            {0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},
            {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e},
            {0x11, 0x11, 0x11, 0x11, 0x11, 0x0a, 0x04},
            {0x11, 0x11, 0x11, 0x15, 0x15, 0x1b, 0x11},
            {0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11},
            {0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04},
            {0x1f, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1f},
        };
        if (c >= '0' && c <= '9') return digits[c - '0'];
        if (c >= 'A' && c <= 'Z') return letters[c - 'A'];
        if (c == ' ') return space;
        return unknown;
    }

    void DrawText(int x, int y, const std::string& text, uint16_t color, int scale = 2) {
        int cursor = x;
        for (char raw : text) {
            char c = static_cast<char>(std::toupper(static_cast<unsigned char>(raw)));
            const uint8_t* glyph = Glyph(c);
            for (int row = 0; row < 7; ++row) {
                for (int col = 0; col < 5; ++col) {
                    if (glyph[row] & (1 << (4 - col))) {
                        FillRect(cursor + col * scale, y + row * scale, scale, scale, color);
                    }
                }
            }
            cursor += 6 * scale;
            if (cursor >= width_ - 8) {
                break;
            }
        }
    }

    std::string ToAsciiUpper(const char* text, size_t max_len = 22) {
        std::string out;
        if (text == nullptr) {
            return out;
        }
        for (const char* p = text; *p != '\0' && out.size() < max_len; ++p) {
            unsigned char ch = static_cast<unsigned char>(*p);
            if (ch < 0x80) {
                if (std::isalnum(ch)) {
                    out.push_back(static_cast<char>(std::toupper(ch)));
                } else if (ch == ' ' || ch == '-' || ch == '_' || ch == '.') {
                    if (!out.empty() && out.back() != ' ') {
                        out.push_back(' ');
                    }
                }
            }
        }
        return out;
    }

    std::string NormalizeStatus(const char* text) {
        if (text == nullptr || text[0] == '\0') return "";
        if (std::strstr(text, "扫描") != nullptr) return "SCAN WIFI";
        if (std::strstr(text, "连接") != nullptr && std::strstr(text, "已连接") == nullptr) return "WIFI CONNECT";
        if (std::strstr(text, "已连接") != nullptr) return "WIFI OK";
        if (std::strstr(text, "聆听") != nullptr) return "LISTENING";
        if (std::strstr(text, "说话") != nullptr) return "SPEAKING";
        if (std::strstr(text, "请稍候") != nullptr || std::strstr(text, "思考") != nullptr) return "THINKING";
        if (std::strstr(text, "检查") != nullptr) return "CHECK OTA";
        if (std::strstr(text, "登录") != nullptr) return "LOGIN";
        if (std::strstr(text, "待命") != nullptr) return "READY";
        if (std::strstr(text, "版本") != nullptr) return "VERSION 2 2 6";
        std::string ascii = ToAsciiUpper(text);
        if (ascii == "LISTENING") return "LISTENING";
        if (ascii == "SPEAKING") return "SPEAKING";
        if (ascii == "PLEASE WAIT") return "THINKING";
        if (ascii == "STANDBY") return "READY";
        return ascii.empty() ? "UPDATE" : ascii;
    }

    std::string NormalizeEmotion(const char* emotion) {
        std::string ascii = ToAsciiUpper(emotion, 14);
        if (ascii.empty()) return "NEUTRAL";
        if (ascii == "MICROCHIP AI") return "AI";
        return ascii;
    }

    void DrawStatusArea() {
        DisplayLockGuard lock(this);
        const uint16_t card = Rgb565(10, 10, 10);
        const uint16_t white = Rgb565(255, 255, 255);
        const uint16_t blue = Rgb565(37, 99, 235);
        FillRect(30, 66, 250, 54, card);
        DrawText(30, 70, "STATUS", blue, 2);
        DrawText(30, 96, status_.empty() ? "STARTING" : status_, white, 2);
    }

    void DrawNotificationArea() {
        DisplayLockGuard lock(this);
        const uint16_t black = Rgb565(0, 0, 0);
        const uint16_t yellow = Rgb565(251, 191, 36);
        FillRect(34, 344, 320, 42, black);
        DrawText(34, 346, notice_.empty() ? "NO NOTICE" : notice_, yellow, 2);
    }

    void DrawEmotionArea() {
        DisplayLockGuard lock(this);
        const uint16_t card = Rgb565(10, 10, 10);
        const uint16_t green = Rgb565(52, 211, 153);
        FillRect(230, 198, 138, 46, card);
        DrawText(230, 202, "MOOD", green, 2);
        DrawText(230, 226, emotion_.empty() ? "NEUTRAL" : emotion_, green, 2);
    }

    void DrawChatArea() {
        DisplayLockGuard lock(this);
        const uint16_t card = Rgb565(10, 10, 10);
        const uint16_t purple = Rgb565(139, 92, 246);
        const uint16_t white = Rgb565(255, 255, 255);
        FillRect(34, 198, 138, 46, card);
        DrawText(34, 202, chat_role_.empty() ? "CHAT" : chat_role_, purple, 2);
        DrawText(34, 226, chat_message_.empty() ? "WAIT" : chat_message_, white, 2);
    }

    void DrawAlignmentOverlay() {
        const uint16_t red = Rgb565(239, 68, 68);
        const uint16_t green = Rgb565(52, 211, 153);
        const uint16_t blue = Rgb565(37, 99, 235);
        const uint16_t yellow = Rgb565(251, 191, 36);
        const uint16_t white = Rgb565(255, 255, 255);

        FillRect(0, 0, 5, height_, red);
        FillRect(0, 0, width_, 5, green);
        FillRect(width_ - 5, 0, 5, height_, blue);
        FillRect(0, height_ - 5, width_, 5, yellow);
        FillRect(width_ / 2 - 1, 0, 3, height_, white);
        FillRect(0, height_ / 2 - 1, width_, 3, white);

        DrawText(8, 42, "L", red, 2);
        DrawText(width_ - 22, 42, "R", blue, 2);
        DrawText(width_ / 2 - 30, height_ / 2 + 8, "CENTER", white, 2);
    }

    void DrawProgressBar(int x, int y, int w, int h, int percent, uint16_t fill_color) {
        const uint16_t track = Rgb565(34, 34, 34);
        FillRect(x, y, w, h, track);
        const int fill_w = std::max(2, (w * percent) / 100);
        FillRect(x, y, fill_w, h, fill_color);
    }

    void DrawAgentCard(int x, int y, const char* name, const char* state,
                       const char* action, int context_percent,
                       uint16_t accent, uint16_t state_color) {
        const uint16_t card = Rgb565(10, 10, 10);
        const uint16_t border = Rgb565(26, 26, 46);
        const uint16_t white = Rgb565(255, 255, 255);
        const uint16_t gray = Rgb565(120, 120, 120);

        FillRect(x, y, width_ - 24, 82, border);
        FillRect(x + 1, y + 1, width_ - 26, 80, card);
        FillRect(x + 10, y + 14, 34, 34, accent);
        DrawText(x + 14, y + 23, name[0] == 'J' ? "J" : "H", Rgb565(0, 0, 0), 2);
        DrawText(x + 54, y + 10, name, white, 2);
        DrawText(width_ - 90, y + 10, state, state_color, 2);
        DrawText(x + 54, y + 34, action, gray, 1);
        DrawText(x + 10, y + 62, "CTX", gray, 1);
        DrawProgressBar(x + 42, y + 64, width_ - 110, 5, context_percent, accent);
        char pct[8];
        snprintf(pct, sizeof(pct), "%d", context_percent);
        DrawText(width_ - 48, y + 58, pct, gray, 1);
    }

    void DrawTaskRow(int x, int y, const char* task, const char* who,
                     int progress, uint16_t dot_color, uint16_t who_color) {
        const uint16_t row = Rgb565(8, 8, 8);
        const uint16_t white = Rgb565(220, 220, 220);
        const uint16_t gray = Rgb565(90, 90, 90);

        FillRect(x, y, width_ - 24, 26, row);
        FillRect(x + 8, y + 9, 8, 8, dot_color);
        DrawText(x + 24, y + 7, task, white, 1);
        DrawText(width_ - 122, y + 7, who, who_color, 1);
        char pct[8];
        snprintf(pct, sizeof(pct), "%d", progress);
        DrawText(width_ - 42, y + 7, progress >= 0 ? pct : "DONE", gray, 1);
    }

    void DrawTopBar(uint16_t blue, uint16_t green, uint16_t white, uint16_t dim) {
        DrawText(12, 8, "22 25", white, 2);
        FillRect(width_ - 84, 11, 4, 10, blue);
        FillRect(width_ - 76, 7, 4, 14, blue);
        FillRect(width_ - 68, 3, 4, 18, blue);
        FillRect(width_ - 54, 7, 34, 14, dim);
        FillRect(width_ - 52, 9, 24, 10, green);
        DrawText(width_ - 142, 9, "86", white, 1);
    }

    void DrawBottomTabs(uint16_t blue, uint16_t dark, uint16_t card, uint16_t gray) {
        FillRect(0, kTabTop, width_, kTabBarHeight, dark);
        const int tab_w = width_ / 4;
        const char* labels[] = {"STATUS", "WIFI", "CHAT", "DEVICE"};
        for (int i = 0; i < 4; ++i) {
            const int x = i * tab_w;
            if (i == static_cast<int>(active_page_)) {
                FillRect(x + 6, kTabTop + 4, tab_w - 12, kTabBarHeight - 8, card);
            }
            DrawText(x + 18, kTabTop + 17, labels[i],
                     i == static_cast<int>(active_page_) ? blue : gray, 1);
        }
    }

    void DrawStatusPage(uint16_t blue, uint16_t purple, uint16_t green,
                        uint16_t yellow, uint16_t white, uint16_t gray) {
        DrawText(12, 38, "STATUS OVERVIEW", white, 1);
        DrawText(238, 38, status_.empty() ? "STARTING" : status_, status_ == "READY" ? green : yellow, 1);

        DrawAgentCard(12, 58, "JARVIS", "ONLINE", "WAITING FOR COMMAND",
                      32, blue, green);
        DrawAgentCard(12, 144, "HERMES", "BUSY", "CODING PULSE APP",
                      67, purple, yellow);

        DrawText(12, 238, "ACTIVE TASKS 3", gray, 1);
        DrawTaskRow(12, 256, "AGENT ROUTING", "HERMES", 0, yellow, purple);
        DrawTaskRow(12, 286, "STATUS API", "JARVIS", 30, yellow, blue);
        DrawTaskRow(12, 316, "WIFI BLUFI", "HERMES", 60, yellow, purple);

        DrawText(12, 344, notice_.empty() ? "NO NOTICE" : notice_, gray, 1);
    }

    void DrawWifiRow(int y, const char* ssid, const char* state, int bars,
                     uint16_t blue, uint16_t green, uint16_t card,
                     uint16_t white, uint16_t gray) {
        FillRect(12, y, width_ - 24, 36, card);
        for (int i = 0; i < 4; ++i) {
            const int h = 5 + i * 4;
            FillRect(24 + i * 8, y + 22 - h, 5, h, i < bars ? blue : gray);
        }
        DrawText(68, y + 10, ssid, white, 1);
        DrawText(width_ - 96, y + 10, state, state[0] == 'O' ? green : blue, 1);
    }

    void DrawWifiPage(uint16_t blue, uint16_t green, uint16_t white,
                      uint16_t gray, uint16_t card, uint16_t purple) {
        DrawText(12, 38, "WIFI SETTINGS", white, 1);
        DrawText(12, 68, "CURRENT CONNECTION", gray, 1);
        DrawWifiRow(86, "ZIROOM501 2", notice_ == "WIFI OK" ? "ONLINE" : "LINKING", 4, blue, green, card, white, gray);

        DrawText(12, 140, "SAVED NETWORKS 2", gray, 1);
        DrawWifiRow(158, "RICARDO", "SWITCH", 3, blue, green, card, white, gray);
        DrawWifiRow(200, "IPHONE HOTSPOT", "SWITCH", 2, blue, green, card, white, gray);

        FillRect(12, 254, width_ - 24, 86, Rgb565(13, 13, 24));
        DrawText(44, 272, "ADD NEW NETWORK", white, 1);
        DrawText(44, 298, "PHONE SENDS WIFI BY BLUFI", gray, 1);
        FillRect(64, 322, width_ - 128, 24, Rgb565(26, 42, 76));
        DrawText(88, 329, "OPEN GOOGLE BLUFI", blue, 1);
    }

    void DrawChatPage(uint16_t blue, uint16_t purple, uint16_t green,
                      uint16_t yellow, uint16_t white, uint16_t gray,
                      uint16_t card) {
        const bool thinking = status_ == "THINKING" || notice_ == "THINKING";
        const bool listening = status_ == "LISTENING";
        const uint16_t agent_color = chat_role_ == "assistant" ? purple : blue;

        FillRect(12, 34, width_ - 24, 30, Rgb565(0, 0, 0));
        FillRect(20, 40, 20, 20, agent_color);
        DrawText(54, 44, chat_role_ == "assistant" ? "HERMES" : "XIAOZHI", white, 1);
        DrawText(width_ - 116, 44,
                 thinking ? "THINKING" : (listening ? "LISTENING" : "STANDBY"),
                 thinking ? yellow : (listening ? Rgb565(239, 68, 68) : gray), 1);

        FillRect(12, 70, width_ - 24, 220, Rgb565(0, 0, 0));
        if (chat_history_.empty()) {
            DrawText(74, 132, "SAY JARVIS TO WAKE ME", gray, 1);
            DrawText(74, 160, "SAY HERMES TO WAKE HIM", gray, 1);
            DrawText(92, 198, "OR TAP MIC TO START", Rgb565(34, 34, 34), 1);
        } else {
            int y = 80;
            for (const auto& msg : chat_history_) {
                const bool user = msg.rfind("YOU ", 0) == 0;
                const int bubble_w = width_ - 78;
                const int x = user ? 54 : 16;
                FillRect(x, y, bubble_w, 40, user ? blue : card);
                DrawText(x + 10, y + 10, msg, white, 1);
                y += 48;
            }
        }

        if (listening) {
            for (int i = 0; i < 10; ++i) {
                int h = 4 + ((i * 7) % 15);
                FillRect(70 + i * 20, 300 - h / 2, 8, h, Rgb565(248, 113, 113));
            }
        }

        FillRect(12, 312, width_ - 24, 40, card);
        FillRect(28, 318, 28, 28, listening ? Rgb565(239, 68, 68) : blue);
        DrawText(78, 326,
                 listening ? "LISTENING MAX 10S" :
                 thinking ? "WAITING FOR REPLY" : "WAKE WORD OR TAP MIC",
                 gray, 1);
    }

    void DrawDevicePage(uint16_t blue, uint16_t white, uint16_t gray, uint16_t card) {
        DrawText(12, 38, "DEVICE INFORMATION", white, 1);
        const char* rows[][2] = {
            {"DEVICE", "GOOGLE"},
            {"FIRMWARE", "XIAOZHI ESP32"},
            {"CHIP", "ESP32 C6"},
            {"DISPLAY", "AMOLED 368X448"},
            {"AUDIO", "ES8311"},
            {"PMIC", "AXP2101"},
            {"SERVER", "XIAOZHI WS MQTT"},
            {"STATE", status_.empty() ? "STARTING" : status_.c_str()},
            {"NOTICE", notice_.empty() ? "NONE" : notice_.c_str()},
        };
        for (int i = 0; i < 9; ++i) {
            int y = 64 + i * 31;
            FillRect(12, y, width_ - 24, 25, card);
            DrawText(24, y + 7, rows[i][0], gray, 1);
            DrawText(146, y + 7, rows[i][1], i == 0 ? blue : white, 1);
        }
    }

    void DrawDashboard() {
        DisplayLockGuard lock(this);

        const uint16_t black = Rgb565(0, 0, 0);
        const uint16_t blue = Rgb565(37, 99, 235);
        const uint16_t purple = Rgb565(139, 92, 246);
        const uint16_t green = Rgb565(52, 211, 153);
        const uint16_t yellow = Rgb565(251, 191, 36);
        const uint16_t dark = Rgb565(18, 18, 18);
        const uint16_t card = Rgb565(10, 10, 10);
        const uint16_t white = Rgb565(255, 255, 255);
        const uint16_t gray = Rgb565(80, 80, 80);
        const uint16_t dim = Rgb565(48, 48, 48);

        FillRect(0, 0, width_, height_, black);
        DrawTopBar(blue, green, white, dim);
        switch (active_page_) {
            case kPageStatus:
                DrawStatusPage(blue, purple, green, yellow, white, gray);
                break;
            case kPageWifi:
                DrawWifiPage(blue, green, white, gray, card, purple);
                break;
            case kPageChat:
                DrawChatPage(blue, purple, green, yellow, white, gray, card);
                break;
            case kPageDevice:
                DrawDevicePage(blue, white, gray, card);
                break;
            default:
                active_page_ = kPageStatus;
                DrawStatusPage(blue, purple, green, yellow, white, gray);
                break;
        }
        DrawBottomTabs(blue, dark, card, gray);

        ESP_LOGI(TAG, "Panel PRD-style dashboard drawn, page=%d", static_cast<int>(active_page_));
    }

    esp_lcd_panel_handle_t panel_ = nullptr;
    SemaphoreHandle_t mutex_ = nullptr;
    Theme direct_theme_;
    Page active_page_ = kPageStatus;
    std::string status_ = "STARTING";
    std::string notice_ = "BOOTING";
    std::string emotion_ = "NEUTRAL";
    std::string chat_role_;
    std::string chat_message_;
    std::vector<std::string> chat_history_;
};

class CustomBacklight : public Backlight {
public:
    CustomBacklight(esp_lcd_panel_io_handle_t panel_io) : Backlight(), panel_io_(panel_io) {}

protected:
    esp_lcd_panel_io_handle_t panel_io_;

    virtual void SetBrightnessImpl(uint8_t brightness) override {
        auto display = Board::GetInstance().GetDisplay();
        DisplayLockGuard lock(display);
        uint8_t data[1] = {((uint8_t)((255 * brightness) / 100))};
        int lcd_cmd = 0x51;
        lcd_cmd &= 0xff;
        lcd_cmd <<= 8;
        lcd_cmd |= LCD_OPCODE_WRITE_CMD << 24;
        esp_lcd_panel_io_tx_param(panel_io_, lcd_cmd, &data, sizeof(data));
    }
};

class WaveshareEsp32c6TouchAMOLED2inch06 : public WifiBoard {
private:
    struct SafeTouchContext {
        esp_lcd_touch_handle_t handle = nullptr;
        i2c_master_dev_handle_t ft3168_dev = nullptr;
    };

    i2c_master_bus_handle_t i2c_bus_;
    Pmic* pmic_ = nullptr;
    esp_io_expander_handle_t io_expander_ = nullptr;
    Button boot_button_;
    CustomLcdDisplay* display_ = nullptr;
    CustomBacklight* backlight_;
    PowerSaveTimer* power_save_timer_;

    void InitializePowerSaveTimer() {
        // Keep the AMOLED bright during UI validation; auto-dimming made the
        // screen look like it was flashing even when the firmware was stable.
        power_save_timer_ = new PowerSaveTimer(-1, -1, -1);
        power_save_timer_->OnEnterSleepMode([this]() {
            GetDisplay()->SetPowerSaveMode(true);
            GetBacklight()->SetBrightness(20);
        });
        power_save_timer_->OnExitSleepMode([this]() {
            GetDisplay()->SetPowerSaveMode(false);
            GetBacklight()->RestoreBrightness();
        });
        power_save_timer_->OnShutdownRequest([this]() {
            pmic_->PowerOff();
        });
        power_save_timer_->SetEnabled(false);
    }

    void InitializeCodecI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
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



    void InitializeAxp2101() {
        ESP_LOGI(TAG, "Init AXP2101");
        pmic_ = new Pmic(i2c_bus_, 0x34);
    }

    void InitializeTca9554() {
        if (!I2cDevicePresent(ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000)) {
            ESP_LOGW(TAG, "TCA9554 IO expander not found at 0x20");
            return;
        }

        esp_err_t ret = esp_io_expander_new_i2c_tca9554(
            i2c_bus_, ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000, &io_expander_);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Create TCA9554 IO expander failed: %s", esp_err_to_name(ret));
            return;
        }

        const uint32_t reset_pins = IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1 | IO_EXPANDER_PIN_NUM_2;
        ESP_ERROR_CHECK(esp_io_expander_set_dir(io_expander_, reset_pins, IO_EXPANDER_OUTPUT));
        ESP_ERROR_CHECK(esp_io_expander_set_level(io_expander_, reset_pins, 1));
        vTaskDelay(pdMS_TO_TICKS(100));
        ESP_ERROR_CHECK(esp_io_expander_set_level(io_expander_, reset_pins, 0));
        vTaskDelay(pdMS_TO_TICKS(300));
        ESP_ERROR_CHECK(esp_io_expander_set_level(io_expander_, reset_pins, 1));
        vTaskDelay(pdMS_TO_TICKS(100));
        ESP_LOGI(TAG, "TCA9554 reset lines initialized");
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.sclk_io_num = EXAMPLE_PIN_NUM_LCD_PCLK;
        buscfg.data0_io_num = EXAMPLE_PIN_NUM_LCD_DATA0;
        buscfg.data1_io_num = EXAMPLE_PIN_NUM_LCD_DATA1;
        buscfg.data2_io_num = EXAMPLE_PIN_NUM_LCD_DATA2;
        buscfg.data3_io_num = EXAMPLE_PIN_NUM_LCD_DATA3;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        buscfg.flags = SPICOMMON_BUSFLAG_QUAD;
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            if (display_ != nullptr && display_->ActiveTab() == 2) {
                ESP_LOGI(TAG, "BOOT click: fallback microphone action");
                display_->ActivateMicFromFallback();
                return;
            }
            app.ToggleChatState();
        });

        boot_button_.OnDoubleClick([this]() {
            // Physical fallback for page navigation if touch calibration is off.
            ESP_LOGI(TAG, "BOOT double click: fallback page switch");
            if (display_ != nullptr) {
                display_->CycleTab();
            }
        });

        boot_button_.OnLongPress([this]() {
            ESP_LOGI(TAG, "BOOT long press: fallback BluFi provisioning");
            if (display_ != nullptr) {
                display_->StartWifiConfigFromFallback();
            } else {
                EnterWifiConfigMode();
            }
        });

        boot_button_.OnMultipleClick([this]() {
            ESP_LOGI(TAG, "BOOT quadruple click: direct LCD transport diagnostic");
            if (display_ != nullptr) {
                display_->DrawTransportDiagnosticFromFallback();
            }
        }, 4);

#if CONFIG_USE_DEVICE_AEC
        boot_button_.OnMultipleClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateIdle) {
                app.SetAecMode(app.GetAecMode() == kAecOff ? kAecOnDeviceSide : kAecOff);
            }
        }, 3);
#endif
    }

    void DrawPanelColorBars(esp_lcd_panel_handle_t panel) {
        ESP_LOGI(TAG, "Drawing direct LCD color bars before LVGL");
        const uint16_t colors[] = {
            0xF800, // red
            0x07E0, // green
            0x001F, // blue
            0xFFFF, // white
            0x0000, // black
        };
        const int bar_h = DISPLAY_HEIGHT / static_cast<int>(sizeof(colors) / sizeof(colors[0]));
        std::vector<uint16_t> line(DISPLAY_WIDTH);

        for (int i = 0; i < static_cast<int>(sizeof(colors) / sizeof(colors[0])); ++i) {
            std::fill(line.begin(), line.end(), colors[i]);
            const int y1 = i * bar_h;
            const int y2 = (i == 4) ? DISPLAY_HEIGHT : (i + 1) * bar_h;
            for (int y = y1; y < y2; ++y) {
                ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, 0, y, DISPLAY_WIDTH, y + 1, line.data()));
            }
        }

        ESP_LOGI(TAG, "Direct LCD color bars drawn; holding for visual check");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    void InitializeSH8601Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = SH8601_PANEL_IO_QSPI_CONFIG(
            EXAMPLE_PIN_NUM_LCD_CS,
            nullptr,
            nullptr
        );
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片
        ESP_LOGD(TAG, "Install LCD driver");
        const sh8601_vendor_config_t vendor_config = {
            .init_cmds = &vendor_specific_init[0],
            .init_cmds_size = sizeof(vendor_specific_init) / sizeof(sh8601_lcd_init_cmd_t),
            .flags = {
                .use_qspi_interface = 1,
            }
        };

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        panel_config.vendor_config = (void *)&vendor_config;
        ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(panel_io, &panel_config, &panel));

        ESP_LOGI(TAG,
                 "Display config: driver=SH8601 qspi=1 bpp=%d panel=%dx%d gap=(0,0) mirror=(%d,%d) swap_xy=%d offset=(%d,%d) lvgl_rounder=2px prd_viewport=%dx%d",
                 panel_config.bits_per_pixel, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                 DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                 DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, kPrdUiW, kPrdUiH);
        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, false);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        esp_lcd_panel_disp_on_off(panel, true);
        display_ = new CustomLcdDisplay(panel_io, panel,
                                        DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                        DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
                                        DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y,
                                        DISPLAY_SWAP_XY);
        display_->SetWifiConfigHandler([this]() {
            EnterWifiConfigMode();
        });
        backlight_ = new CustomBacklight(panel_io);
        backlight_->RestoreBrightness();
    }

    bool InitializeTouchWithCst9217(const esp_lcd_touch_config_t& tp_cfg, esp_lcd_touch_handle_t* out_tp) {
        esp_lcd_panel_io_handle_t tp_io_handle = nullptr;
        esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_CST9217_CONFIG();
        tp_io_config.scl_speed_hz = 400 * 1000;
        esp_err_t ret = esp_lcd_new_panel_io_i2c(i2c_bus_, &tp_io_config, &tp_io_handle);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Create CST9217 touch IO failed: %s", esp_err_to_name(ret));
            return false;
        }
        ret = esp_lcd_touch_new_i2c_cst9217(tp_io_handle, &tp_cfg, out_tp);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "CST9217 touch probe failed: %s", esp_err_to_name(ret));
            esp_lcd_panel_io_del(tp_io_handle);
            return false;
        }
        ESP_LOGI(TAG, "CST9217 touch controller initialized");
        return true;
    }

    bool InitializeTouchWithCst816s(const esp_lcd_touch_config_t& tp_cfg, esp_lcd_touch_handle_t* out_tp) {
        esp_lcd_panel_io_handle_t tp_io_handle = nullptr;
        esp_lcd_panel_io_i2c_config_t tp_io_config = {};
        tp_io_config.dev_addr = ESP_LCD_TOUCH_IO_I2C_CST816S_ADDRESS;
        tp_io_config.control_phase_bytes = 1;
        tp_io_config.dc_bit_offset = 0;
        tp_io_config.lcd_cmd_bits = 8;
        tp_io_config.flags.disable_control_phase = 1;
        tp_io_config.scl_speed_hz = 400 * 1000;
        esp_err_t ret = esp_lcd_new_panel_io_i2c(i2c_bus_, &tp_io_config, &tp_io_handle);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Create CST816S touch IO failed: %s", esp_err_to_name(ret));
            return false;
        }
        ret = esp_lcd_touch_new_i2c_cst816s(tp_io_handle, &tp_cfg, out_tp);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "CST816S touch probe failed: %s", esp_err_to_name(ret));
            esp_lcd_panel_io_del(tp_io_handle);
            return false;
        }
        ESP_LOGI(TAG, "CST816S touch controller initialized");
        return true;
    }

    bool InitializeTouchWithFt5x06(const esp_lcd_touch_config_t& tp_cfg, esp_lcd_touch_handle_t* out_tp) {
        esp_lcd_panel_io_handle_t tp_io_handle = nullptr;
        esp_lcd_panel_io_i2c_config_t tp_io_config = {};
        tp_io_config.dev_addr = ESP_LCD_TOUCH_IO_I2C_FT5x06_ADDRESS;
        tp_io_config.control_phase_bytes = 1;
        tp_io_config.dc_bit_offset = 0;
        tp_io_config.lcd_cmd_bits = 8;
        tp_io_config.flags.disable_control_phase = 1;
        tp_io_config.scl_speed_hz = 400 * 1000;
        esp_err_t ret = esp_lcd_new_panel_io_i2c(i2c_bus_, &tp_io_config, &tp_io_handle);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Create FT5x06 touch IO failed: %s", esp_err_to_name(ret));
            return false;
        }
        ret = esp_lcd_touch_new_i2c_ft5x06(tp_io_handle, &tp_cfg, out_tp);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "FT5x06 touch probe failed: %s", esp_err_to_name(ret));
            esp_lcd_panel_io_del(tp_io_handle);
            return false;
        }
        ESP_LOGI(TAG, "FT5x06 touch controller initialized");
        return true;
    }

    bool InitializeFt3168RawTouch(i2c_master_dev_handle_t* out_dev) {
        i2c_device_config_t touch_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = ESP_LCD_TOUCH_IO_I2C_FT5x06_ADDRESS,
            .scl_speed_hz = 400 * 1000,
            .scl_wait_us = 0,
            .flags = {
                .disable_ack_check = 0,
            },
        };
        esp_err_t ret = i2c_master_bus_add_device(i2c_bus_, &touch_cfg, out_dev);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Create FT3168 raw I2C device failed: %s", esp_err_to_name(ret));
            return false;
        }
        ESP_LOGI(TAG, "FT3168 raw touch reader initialized");
        return true;
    }

    static void SafeTouchRead(lv_indev_t* indev, lv_indev_data_t* data) {
        auto* ctx = static_cast<SafeTouchContext*>(lv_indev_get_driver_data(indev));
        if (ctx == nullptr) {
            data->state = LV_INDEV_STATE_RELEASED;
            return;
        }

        if (ctx->ft3168_dev != nullptr) {
            uint8_t reg = 0x02;
            uint8_t touch_data[5] = {};
            esp_err_t ret = i2c_master_transmit_receive(
                ctx->ft3168_dev, &reg, 1, touch_data, sizeof(touch_data), 10);
            if (ret != ESP_OK || (touch_data[0] & 0x0F) == 0) {
                data->state = LV_INDEV_STATE_RELEASED;
                return;
            }

            data->point.x = ((touch_data[1] & 0x0F) << 8) | touch_data[2];
            data->point.y = ((touch_data[3] & 0x0F) << 8) | touch_data[4];
            data->state = LV_INDEV_STATE_PRESSED;
            return;
        }

        if (ctx->handle == nullptr) {
            data->state = LV_INDEV_STATE_RELEASED;
            return;
        }

        uint8_t touch_cnt = 0;
        esp_lcd_touch_point_data_t touch_data[CONFIG_ESP_LCD_TOUCH_MAX_POINTS] = {};
        esp_err_t ret = esp_lcd_touch_read_data(ctx->handle);
        if (ret != ESP_OK) {
            data->state = LV_INDEV_STATE_RELEASED;
            return;
        }

        ret = esp_lcd_touch_get_data(ctx->handle, touch_data, &touch_cnt, CONFIG_ESP_LCD_TOUCH_MAX_POINTS);
        if (ret != ESP_OK || touch_cnt == 0) {
            data->state = LV_INDEV_STATE_RELEASED;
            return;
        }

        data->point.x = touch_data[0].x;
        data->point.y = touch_data[0].y;
        data->state = LV_INDEV_STATE_PRESSED;
    }

    void AddSafeLvglTouch(esp_lcd_touch_handle_t tp, i2c_master_dev_handle_t ft3168_dev = nullptr) {
        auto* ctx = static_cast<SafeTouchContext*>(heap_caps_calloc(1, sizeof(SafeTouchContext), MALLOC_CAP_DEFAULT));
        if (ctx == nullptr) {
            ESP_LOGW(TAG, "Not enough memory for touch input context");
            return;
        }
        ctx->handle = tp;
        ctx->ft3168_dev = ft3168_dev;

        lvgl_port_lock(0);
        lv_indev_t* indev = lv_indev_create();
        lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(indev, SafeTouchRead);
        lv_indev_set_disp(indev, lv_display_get_default());
        lv_indev_set_driver_data(indev, ctx);
        lvgl_port_unlock();
    }

    bool I2cDevicePresent(uint8_t addr) {
        return i2c_master_probe(i2c_bus_, addr, 50) == ESP_OK;
    }

    void LogI2cDevices() {
        std::string found;
        char buf[8];
        for (uint8_t addr = 0x08; addr < 0x78; ++addr) {
            if (I2cDevicePresent(addr)) {
                snprintf(buf, sizeof(buf), "0x%02X ", addr);
                found += buf;
            }
        }
        ESP_LOGI(TAG, "I2C devices: %s", found.empty() ? "none" : found.c_str());
    }

    void InitializeTouch() {
        esp_lcd_touch_handle_t tp = nullptr;
        i2c_master_dev_handle_t ft3168_dev = nullptr;
        esp_lcd_touch_config_t tp_cfg = {
            .x_max = DISPLAY_WIDTH - 1,
            .y_max = DISPLAY_HEIGHT - 1,
            .rst_gpio_num = GPIO_NUM_NC,
            .int_gpio_num = GPIO_NUM_NC,
            .levels = {
                .reset = 0,
                .interrupt = 0,
            },
            .flags = {
                .swap_xy = 0,
                .mirror_x = 0,
                .mirror_y = 0,
            },
        };

        bool initialized = false;

        for (int attempt = 0; attempt < 6 && !initialized; ++attempt) {
            LogI2cDevices();
            if (I2cDevicePresent(ESP_LCD_TOUCH_IO_I2C_CST9217_ADDRESS)) {
                initialized = InitializeTouchWithCst9217(tp_cfg, &tp);
            }
            if (!initialized && I2cDevicePresent(ESP_LCD_TOUCH_IO_I2C_CST816S_ADDRESS)) {
                initialized = InitializeTouchWithCst816s(tp_cfg, &tp);
            }
            if (!initialized && I2cDevicePresent(ESP_LCD_TOUCH_IO_I2C_FT5x06_ADDRESS)) {
                initialized = InitializeFt3168RawTouch(&ft3168_dev);
            }
            if (!initialized) {
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        }

        if (!initialized) {
            ESP_LOGW(TAG, "Touch controller did not ACK during boot; registering FT3168 raw reader for late-ready touch");
            initialized = InitializeFt3168RawTouch(&ft3168_dev);
        }
        if (!initialized) {
            ESP_LOGW(TAG, "No touch input registered; BOOT double-click can still switch tabs");
            return;
        }

        AddSafeLvglTouch(tp, ft3168_dev);
        ESP_LOGI(TAG, "Touch input registered with safe LVGL reader");
    }

    // 初始化工具
    void InitializeTools() {
        auto &mcp_server = McpServer::GetInstance();
        mcp_server.AddTool("self.system.reconfigure_wifi",
            "End this conversation and enter WiFi configuration mode.\n"
            "**CAUTION** You must ask the user to confirm this action.",
            PropertyList(), [this](const PropertyList& properties) {
                EnterWifiConfigMode();
                return true;
            });
    }

public:
    WaveshareEsp32c6TouchAMOLED2inch06() :
        boot_button_(BOOT_BUTTON_GPIO) {
        InitializePowerSaveTimer();
        InitializeCodecI2c();
        InitializeAxp2101();
        InitializeTca9554();
        InitializeSpi();
        InitializeSH8601Display();
        InitializeTouch();
        InitializeButtons();
        InitializeTools();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        return backlight_;
    }

    virtual bool GetBatteryLevel(int &level, bool& charging, bool& discharging) override {
        static bool last_discharging = false;
        charging = pmic_->IsCharging();
        discharging = pmic_->IsDischarging();
        if (discharging != last_discharging) {
            power_save_timer_->SetEnabled(discharging);
            last_discharging = discharging;
        }

        level = pmic_->GetBatteryLevel();
        return true;
    }

    virtual void SetPowerSaveLevel(PowerSaveLevel level) override {
        if (level != PowerSaveLevel::LOW_POWER) {
            power_save_timer_->WakeUp();
        }
        WifiBoard::SetPowerSaveLevel(level);
    }
};

DECLARE_BOARD(WaveshareEsp32c6TouchAMOLED2inch06);
