#include "rgb_matrix_display.h"
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_lvgl_port.h>
#include <font_awesome.h>
#include <stdio.h>
#include <time.h>
#include <cstring>
#include <string>
#include "application.h"
#include "assets/lang_config.h"
#include "board.h"
#include "config.h"
#include "emoji_collection.h"
#include "hub75.h"

// 声明中文字体
LV_FONT_DECLARE(font_puhui_14_1);
LV_FONT_DECLARE(BUILTIN_ICON_FONT);
LV_FONT_DECLARE(font_awesome_30_4);

// 声明32x32 Emoji集合
class Twemoji32;

struct Hub75Context {
    Hub75Driver driver;
};

namespace {

const char* LogTag = "Hub75Display";
constexpr size_t Len = 16;

bool StartsWith(const char* text, const char* prefix) {
    return strncmp(text, prefix, strlen(prefix)) == 0;
}

bool IsInlineSpace(char ch) { return (ch == ' ') || (ch == '\r') || (ch == '\n') || (ch == '\t'); }

bool UseBuiltInEmotionIcon(const char* emotion) {
    if (emotion == nullptr) {
        return false;
    }
    if (strcmp(emotion, "microchip_ai") == 0) {
        return true;
    }
    if (strcmp(emotion, "link") == 0) {
        return true;
    }
    return false;
}

const char* MakeVersionText(const char* text) {
    const char* p = text + strlen(Lang::Strings::VERSION);
    while (*p == ' ') {
        p++;
    }
    if (*p == '\0') {
        return "v";
    }

    thread_local char buf[3 + Len]{0};
    buf[0] = 'v';
    buf[1] = ':';

    size_t n = 0;
    while (n < Len) {
        const char c = *p++;
        if (c == '\0') {
            break;
        }
        buf[2 + n] = c;
        n++;
    }
    buf[2 + n] = '\0';
    return buf;
}

std::string MakeSingleLineText(const char* text) {
    if (text == nullptr) {
        return "";
    }
    std::string out;
    out.reserve(strlen(text));
    bool prev_space = true;
    while (*text != '\0') {
        const char ch = *text++;
        if (IsInlineSpace(ch)) {
            if (!prev_space) {
                out.push_back(' ');
            }
            prev_space = true;
            continue;
        }

        out.push_back(ch);
        prev_space = false;
    }

    if (!out.empty() && out.back() == ' ') {
        out.pop_back();
    }
    return out;
}

std::string TransformMessageText(const char* role, const char* content) {
    if (content == nullptr) {
        return "";
    }
    if ((role != nullptr) && (strcmp(role, "system") == 0)) {
        return MakeSingleLineText(content);
    }
    return content;
}

// 重新映射状态文本，将原始文本更换为更简短的文本
const char* RemapStatusText(const char* text) {
    if (text == nullptr) {
        return nullptr;
    }

    struct MapItem {
        const char* from;
        const char* to;
    };

    static const MapItem Map[] = {
        {Lang::Strings::SCANNING_WIFI, "扫描中"},
        {Lang::Strings::CONNECTING, "连接中"},
        {Lang::Strings::WIFI_CONFIG_MODE, "配网中"},
        {Lang::Strings::CHECKING_NEW_VERSION, "检查中"},
        {Lang::Strings::LOADING_PROTOCOL, "登录中"},
        {Lang::Strings::REGISTERING_NETWORK, "配网中"},
        {Lang::Strings::DETECTING_MODULE, "检测中"},
        {Lang::Strings::ACTIVATION, "激活中"},
        {Lang::Strings::PLEASE_WAIT, "等待中"},
        {Lang::Strings::LISTENING, "聆听"},
        {Lang::Strings::SPEAKING, "说话"},
        {Lang::Strings::STANDBY, "待命"},
    };

    size_t i = 0;
    while (true) {
        if (i >= (sizeof(Map) / sizeof(Map[0]))) {
            break;
        }
        if (strcmp(text, Map[i].from) == 0) {
            return Map[i].to;
        }
        i++;
    }

    if (StartsWith(text, Lang::Strings::CONNECT_TO)) {
        return "连接中";
    }

    if (StartsWith(text, Lang::Strings::CONNECTED_TO)) {
        return "已连接";
    }

    if (StartsWith(text, Lang::Strings::VERSION)) {
        return MakeVersionText(text);
    }

    return text;
}

const char* TransformStatusText(const char* text) { return RemapStatusText(text); }

void SetObjectVisible(lv_obj_t* obj, bool visible) {
    if (obj == nullptr) {
        return;
    }
    if (visible) {
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

}  // namespace

CustomMatrixDisplay::CustomMatrixDisplay(int width, int height) : LvglDisplay() {
    width_ = width;
    height_ = height;

    Hub75Config hub75_config{};
    hub75_config.min_refresh_rate = HUB75_MIN_REFRESH_RATE;
    hub75_config.panel_width = HUB75_PANEL_WIDTH;
    hub75_config.panel_height = HUB75_PANEL_HEIGHT;
    hub75_config.scan_wiring = HUB75_SCAN_WIRING;
    hub75_config.shift_driver = HUB75_SHIFT_DRIVER;
    hub75_config.layout_cols = HUB75_CHAIN_COLUMNS;
    hub75_config.layout_rows = HUB75_CHAIN_ROWS;

    hub75_config.pins.r1 = static_cast<int>(HUB75_R1);
    hub75_config.pins.g1 = static_cast<int>(HUB75_G1);
    hub75_config.pins.b1 = static_cast<int>(HUB75_B1);
    hub75_config.pins.r2 = static_cast<int>(HUB75_R2);
    hub75_config.pins.g2 = static_cast<int>(HUB75_G2);
    hub75_config.pins.b2 = static_cast<int>(HUB75_B2);

    hub75_config.pins.a = static_cast<int>(HUB75_A);
    hub75_config.pins.b = static_cast<int>(HUB75_B);
    hub75_config.pins.c = static_cast<int>(HUB75_C);
    hub75_config.pins.d = static_cast<int>(HUB75_D);
    hub75_config.pins.e = static_cast<int>(HUB75_E);

    hub75_config.pins.lat = static_cast<int>(HUB75_LAT);
    hub75_config.pins.oe = static_cast<int>(HUB75_OE);
    hub75_config.pins.clk = static_cast<int>(HUB75_CLK);

    hub75_context_ = new Hub75Context{Hub75Driver(hub75_config)};
    const bool hub75_begin_ok = hub75_context_->driver.begin();
    if (!hub75_begin_ok) {
        ESP_LOGE(LogTag, "Hub75 begin failed");
        delete hub75_context_;
        hub75_context_ = nullptr;
        return;
    }

    hub75_context_->driver.clear();

    lv_init();

    lvgl_port_cfg_t lvgl_port_config = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_port_config.task_priority = 2;
    lvgl_port_config.timer_period_ms = 16;
    lvgl_port_init(&lvgl_port_config);

    // Lock before creating display
    lvgl_port_lock(0);

    const int ui_width_px = width_;
    const int ui_height_px = height_;
    const size_t render_buffer_pixels = static_cast<size_t>(ui_width_px) * 40;
    const size_t render_buffer_bytes =
        render_buffer_pixels * LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565);
    void* render_buffer_1 =
        heap_caps_malloc(render_buffer_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (render_buffer_1 == nullptr) {
        ESP_LOGE(LogTag, "LVGL buffer alloc failed");
        lvgl_port_unlock();
        return;
    }

    display_ = lv_display_create(ui_width_px, ui_height_px);
    if (display_ == nullptr) {
        ESP_LOGE(LogTag, "LVGL display create failed");
        free(render_buffer_1);
        lvgl_port_unlock();
        return;
    }

    lv_display_set_flush_cb(display_, LvglFlushCallback);
    lv_display_set_user_data(display_, this);
    lv_display_set_buffers(display_, render_buffer_1, nullptr, render_buffer_bytes,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    lvgl_port_unlock();
}

bool CustomMatrixDisplay::Lock(int timeout_ms) { return lvgl_port_lock(timeout_ms); }

void CustomMatrixDisplay::Unlock() { lvgl_port_unlock(); }

void CustomMatrixDisplay::SetupUI() {
    if (setup_ui_called_) {
        return;
    }
    Display::SetupUI();

    // 初始化 Emoji 资源（用于 SetEmotion）
    emoji_collection_ = std::make_shared<Twemoji32>();

    const int ui_width_px = width_;
    const int ui_height_px = height_;

    // 屏幕根对象：全黑背景
    auto* screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);

    // 主容器：承载所有控件
    main_container_ = lv_obj_create(screen);
    lv_obj_set_size(main_container_, ui_width_px, ui_height_px);
    lv_obj_set_style_bg_color(main_container_, lv_color_black(), 0);
    lv_obj_set_style_border_width(main_container_, 0, 0);
    lv_obj_set_style_pad_all(main_container_, 0, 0);
    lv_obj_center(main_container_);

    // 左上角：WiFi 图标（UpdateStatusBar 更新内容）
    network_label_ = lv_label_create(main_container_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, &BUILTIN_ICON_FONT, 0);
    lv_obj_set_style_text_color(network_label_, lv_color_white(), 0);
    lv_obj_align(network_label_, LV_ALIGN_TOP_LEFT, 0, 0);

    // 右上角：状态文本 + 时间
    status_label_ = lv_label_create(main_container_);
    lv_obj_set_size(status_label_, ui_width_px - 16, 16);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_color(status_label_, lv_color_white(), 0);
    lv_obj_set_style_text_font(status_label_, &font_puhui_14_1, 0);
    lv_obj_align(status_label_, LV_ALIGN_TOP_RIGHT, 0, -1);
    status_text_ = "初始化";
    RefreshStatusLabelLocked();

    // 底部滚动文本：SetChatMessage 使用
    message_label_ = lv_label_create(main_container_);
    lv_obj_set_width(message_label_, ui_width_px);
    lv_label_set_long_mode(message_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(message_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(message_label_, lv_color_white(), 0);
    lv_obj_set_style_text_font(message_label_, &font_puhui_14_1, 0);
    lv_obj_align(message_label_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_label_set_text(message_label_, "hi 小智");

    // Emoji 图片：SetEmotion 使用
    emoji_image_ = lv_image_create(main_container_);
    lv_obj_align(emoji_image_, LV_ALIGN_CENTER, 0, -1);
    lv_obj_add_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);

    emotion_icon_label_ = lv_label_create(main_container_);
    lv_label_set_text(emotion_icon_label_, FONT_AWESOME_MICROCHIP_AI);
    lv_obj_set_style_text_font(emotion_icon_label_, &font_awesome_30_4, 0);
    lv_obj_set_style_text_color(emotion_icon_label_, lv_color_white(), 0);
    lv_obj_align(emotion_icon_label_, LV_ALIGN_CENTER, 0, -1);
    lv_obj_remove_flag(emotion_icon_label_, LV_OBJ_FLAG_HIDDEN);
}

void CustomMatrixDisplay::SetEmotion(const char* emotion) {
    DisplayLockGuard lock(this);

    if (emotion == nullptr) {
        SetObjectVisible(emoji_image_, false);
        SetObjectVisible(emotion_icon_label_, false);
        return;
    }

    // 尝试获取表情图片
    const LvglImage* emoji_lvgl_image = nullptr;
    if (emoji_collection_ && !UseBuiltInEmotionIcon(emotion)) {
        emoji_lvgl_image = emoji_collection_->GetEmojiImage(emotion);
    }

    if (emoji_lvgl_image != nullptr) {
        if (emoji_image_ == nullptr) {
            return;
        }
        lv_image_set_src(emoji_image_, emoji_lvgl_image->image_dsc());
        SetObjectVisible(emoji_image_, true);
        SetObjectVisible(emotion_icon_label_, false);
        return;
    }

    SetObjectVisible(emoji_image_, false);
    if (emotion_icon_label_ == nullptr) {
        return;
    }
    lv_label_set_text(emotion_icon_label_, FONT_AWESOME_MICROCHIP_AI);
    SetObjectVisible(emotion_icon_label_, true);
}

void CustomMatrixDisplay::SetChatMessage(const char* role, const char* content) {
    DisplayLockGuard lock(this);
    if (message_label_ == nullptr) {
        return;
    }

    const std::string text = TransformMessageText(role, content);

    lv_label_set_text(message_label_, text.c_str());
    SetObjectVisible(message_label_, !text.empty());
}

void CustomMatrixDisplay::SetStatus(const char* status) {
    DisplayLockGuard lock(this);
    status_text_ = TransformStatusText(status);
    RefreshStatusLabelLocked();
    last_status_update_time_ = std::chrono::system_clock::now();
}

void CustomMatrixDisplay::ShowNotification(const char* notification, int duration_ms) {
    static_cast<void>(duration_ms);
    SetStatus(notification);
}

void CustomMatrixDisplay::UpdateStatusBar(bool update_all) {
    static_cast<void>(update_all);

    auto& app = Application::GetInstance();
    auto& board = Board::GetInstance();

    const char* network_icon = board.GetNetworkStateIcon();
    {
        DisplayLockGuard lock(this);
        if (network_label_ != nullptr && network_icon != nullptr) {
            lv_label_set_text(network_label_, network_icon);
        }
    }

    time_t now = time(nullptr);
    tm timeinfo{};
    localtime_r(&now, &timeinfo);
    if (timeinfo.tm_year < 2025 - 1900) {
        return;
    }

    char buf[6]{0};
    snprintf(buf, sizeof(buf), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

    const bool idle = app.GetDeviceState() == kDeviceStateIdle;
    if (idle) {
        const auto now_tp = std::chrono::system_clock::now();
        if (last_status_update_time_ + std::chrono::seconds(10) < now_tp) {
            status_text_.clear();
            time_text_ = buf;
            last_status_update_time_ = now_tp;
        }
    }

    if (!idle) {
        time_text_.clear();
    }

    DisplayLockGuard lock(this);
    RefreshStatusLabelLocked();
}

CustomMatrixDisplay::~CustomMatrixDisplay() {
    if (hub75_context_ == nullptr) {
        return;
    }
    hub75_context_->driver.end();
    delete hub75_context_;
    hub75_context_ = nullptr;
}

void CustomMatrixDisplay::SetBrightness(uint8_t brightness_0_100) {
    if (hub75_context_ == nullptr) {
        return;
    }
    if (brightness_0_100 > 100) {
        brightness_0_100 = 100;
    }
    uint32_t value = static_cast<uint32_t>(brightness_0_100) * 255u;
    uint8_t basis = static_cast<uint8_t>(value / 100u);
    hub75_context_->driver.set_brightness(basis);
}

void CustomMatrixDisplay::RefreshStatusLabelLocked() {
    if (status_label_ == nullptr) {
        return;
    }

    std::string text = status_text_;
    if (!time_text_.empty()) {
        if (!text.empty()) {
            text += " ";
        }
        text += time_text_;
    }
    lv_label_set_text(status_label_, text.c_str());
    SetObjectVisible(status_label_, true);
}

void CustomMatrixDisplay::LvglFlushCallback(lv_display_t* disp, const lv_area_t* area,
                                            uint8_t* color_map) {
    if (disp == nullptr) {
        return;
    }
    auto* display = static_cast<CustomMatrixDisplay*>(lv_display_get_user_data(disp));
    if (display == nullptr) {
        lv_disp_flush_ready(disp);
        return;
    }
    if (display->hub75_context_ == nullptr) {
        lv_disp_flush_ready(disp);
        return;
    }

    const auto* pixel_buffer = reinterpret_cast<const uint16_t*>(color_map);
    const uint16_t start_x = static_cast<uint16_t>(area->x1);
    const uint16_t start_y = static_cast<uint16_t>(area->y1);
    const uint16_t width_px = static_cast<uint16_t>(area->x2 - area->x1 + 1);
    const uint16_t height_px = static_cast<uint16_t>(area->y2 - area->y1 + 1);

    display->hub75_context_->driver.draw_pixels(
        start_x, start_y, width_px, height_px, reinterpret_cast<const uint8_t*>(pixel_buffer),
        Hub75PixelFormat::RGB565, Hub75ColorOrder::RGB, false);
    lv_disp_flush_ready(disp);
}
