#include "r2d2_nav.h"

#include "audio/audio_codec.h"
#include "backlight.h"
#include "board.h"
#include "r2d2_core_client.h"
#include "r2d2_face.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <material_symbols.h>
#include <wifi_manager.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

LV_FONT_DECLARE(font_noto_sans_basic_16_4);
LV_FONT_DECLARE(font_noto_sans_basic_20_4);
LV_FONT_DECLARE(font_material_symbols_16_4);

#define TAG "R2d2Nav"

namespace r2d2 {

namespace {

constexpr int kRows = 3;                      // list items per page on the round screen
constexpr int kRowTop[kRows] = {60, 102, 144};
constexpr int kRowH = 36, kRowW = 184;
constexpr uint32_t kHairline = 0x262E3F;
constexpr uint32_t kRowBg = 0x171C27;
constexpr int64_t kReloadAfterActionUs = 900 * 1000;  // let a switch report its new state
constexpr int64_t kDeviceRefreshUs = 2 * 1000 * 1000;
constexpr int64_t kLiveRefreshUs = 5 * 1000 * 1000;

lv_obj_t* Plain(lv_obj_t* parent) {
    lv_obj_t* obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    return obj;
}

lv_obj_t* Label(lv_obj_t* parent, const lv_font_t* font, uint32_t color) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, Rgb(color), 0);
    lv_label_set_text(label, "");
    return label;
}

uint32_t StateColor(const std::string& state) {
    if (state == "on") return 0x5FB88A;
    if (state == "warn") return 0xE0A83E;
    if (state == "crit") return kVoiceError;
    return kTextFaint;  // off
}

std::string Str(const cJSON* obj, const char* key) {
    const cJSON* v = cJSON_GetObjectItem(obj, key);
    return cJSON_IsString(v) ? v->valuestring : "";
}

void Show(lv_obj_t* obj, bool visible) {
    if (visible) lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

}  // namespace

NavView::NavView(lv_obj_t* parent, CoreClient* client) : client_(client) {
    root_ = Plain(parent);
    lv_obj_set_size(root_, LV_PCT(100), LV_PCT(100));

    area_dots_ = Plain(root_);
    lv_obj_set_size(area_dots_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(area_dots_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(area_dots_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(area_dots_, 5, 0);
    lv_obj_align(area_dots_, LV_ALIGN_TOP_MID, 0, 12);

    depth_dots_ = Plain(root_);
    lv_obj_set_size(depth_dots_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(depth_dots_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(depth_dots_, 5, 0);
    lv_obj_align(depth_dots_, LV_ALIGN_RIGHT_MID, -8, 0);

    title_ = Label(root_, &font_noto_sans_basic_16_4, kTextDim);
    lv_obj_align(title_, LV_ALIGN_TOP_MID, 0, 30);

    note_ = Label(root_, &font_noto_sans_basic_16_4, kTextDim);
    lv_obj_set_width(note_, 170);
    lv_obj_set_style_text_align(note_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(note_, LV_LABEL_LONG_WRAP);
    lv_obj_align(note_, LV_ALIGN_CENTER, 0, 0);

    for (int i = 0; i < kRows; i++) {
        lv_obj_t* row = Plain(root_);
        lv_obj_set_size(row, kRowW, kRowH);
        lv_obj_set_pos(row, (240 - kRowW) / 2, kRowTop[i]);
        lv_obj_set_style_radius(row, kRowH / 2, 0);
        lv_obj_set_style_bg_color(row, Rgb(kRowBg), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(row, Rgb(kHairline), 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_t* label = Label(row, &font_noto_sans_basic_16_4, kText);
        lv_obj_set_width(label, kRowW - 64);
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 14, 0);
        lv_obj_t* value = Label(row, &font_noto_sans_basic_16_4, kTextDim);
        lv_obj_align(value, LV_ALIGN_RIGHT_MID, -14, 0);
        lv_obj_t* dot = Plain(row);
        lv_obj_set_size(dot, 10, 10);
        lv_obj_set_style_radius(dot, 5, 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_align(dot, LV_ALIGN_RIGHT_MID, -16, 0);
        rows_.push_back(row);
        row_labels_.push_back(label);
        row_values_.push_back(value);
        row_dots_.push_back(dot);
    }

    metrics_ = Plain(root_);
    lv_obj_set_size(metrics_, 200, 124);
    lv_obj_align(metrics_, LV_ALIGN_CENTER, 0, 8);
    for (int i = 0; i < 4; i++) {
        lv_obj_t* value = Label(metrics_, &font_noto_sans_basic_20_4, kText);
        lv_obj_t* label = Label(metrics_, &font_noto_sans_basic_16_4, kTextDim);
        int x = i % 2 == 0 ? -50 : 50, y = i < 2 ? -32 : 30;
        lv_obj_align(value, LV_ALIGN_CENTER, x, y - 10);
        lv_obj_align(label, LV_ALIGN_CENTER, x, y + 14);
        metric_values_.push_back(value);
        metric_labels_.push_back(label);
    }

    chart_box_ = Plain(root_);
    lv_obj_set_size(chart_box_, LV_PCT(100), LV_PCT(100));
    chart_value_ = Label(chart_box_, &font_noto_sans_basic_20_4, kText);
    lv_obj_align(chart_value_, LV_ALIGN_TOP_MID, 0, 56);
    chart_ = lv_chart_create(chart_box_);
    lv_obj_set_size(chart_, 180, 62);
    lv_obj_align(chart_, LV_ALIGN_CENTER, 0, 12);
    lv_chart_set_type(chart_, LV_CHART_TYPE_LINE);
    lv_chart_set_div_line_count(chart_, 3, 0);
    lv_obj_set_style_bg_opa(chart_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(chart_, 0, 0);
    lv_obj_set_style_pad_all(chart_, 0, 0);
    lv_obj_set_style_line_color(chart_, Rgb(kHairline), LV_PART_MAIN);
    lv_obj_set_style_line_width(chart_, 3, LV_PART_ITEMS);
    lv_obj_set_style_width(chart_, 0, LV_PART_INDICATOR);
    lv_obj_set_style_height(chart_, 0, LV_PART_INDICATOR);
    chart_series_ = lv_chart_add_series(chart_, Rgb(kRainTint), LV_CHART_AXIS_PRIMARY_Y);
    chart_range_ = Label(chart_box_, &font_noto_sans_basic_16_4, kTextFaint);
    lv_obj_align(chart_range_, LV_ALIGN_CENTER, 0, 58);

    image_ = lv_image_create(root_);
    lv_obj_align(image_, LV_ALIGN_CENTER, 0, -2);
    caption_ = Label(root_, &font_noto_sans_basic_16_4, kTextDim);
    lv_obj_align(caption_, LV_ALIGN_CENTER, 0, 70);

    footer_arrow_ = Label(root_, &font_material_symbols_16_4, kTextFaint);
    lv_label_set_text(footer_arrow_, MATERIAL_SYMBOLS_KEYBOARD_ARROW_UP);
    lv_obj_align(footer_arrow_, LV_ALIGN_BOTTOM_MID, 0, -40);
    footer_ = Label(root_, &font_noto_sans_basic_16_4, kTextFaint);
    lv_obj_align(footer_, LV_ALIGN_BOTTOM_MID, 0, -22);

    Render();
}

// ── data from the Core ───────────────────────────────────────────────────────

void NavView::SetNav(const cJSON* nav) {
    areas_.clear();
    const cJSON* areas = cJSON_GetObjectItem(nav, "areas");
    const cJSON* area = nullptr;
    cJSON_ArrayForEach(area, areas) {
        areas_.push_back({Str(area, "name"), Str(area, "screen")});
    }
    const cJSON* home = cJSON_GetObjectItem(nav, "home");
    const cJSON* timeout = cJSON_GetObjectItem(nav, "home_timeout_s");
    home_ = cJSON_IsNumber(home) ? std::clamp(home->valueint, 0, (int)areas_.size() - 1) : 0;
    if (cJSON_IsNumber(timeout)) home_timeout_us_ = (int64_t)timeout->valueint * 1000 * 1000;
    ESP_LOGI(TAG, "nav: %d areas, home %d", (int)areas_.size(), home_);
    if (!areas_.empty()) GoArea(home_);
}

void NavView::OnScreen(uint32_t seq, const std::string& id, cJSON* json) {
    if (seq != seq_) {  // the user moved on
        cJSON_Delete(json);
        return;
    }
    Screen s;
    s.id = id;
    s.loading = false;
    if (json == nullptr) {
        s.kind = "note";
        s.text = "Sem conexão com o r2d2";
    } else {
        s.kind = Str(json, "kind");
        s.title = Str(json, "title");
        s.next = Str(json, "next");
        s.next_name = Str(json, "next_name");
        s.text = Str(json, "text");
        s.image = Str(json, "image");
        s.caption = Str(json, "caption");
        const cJSON* entry = nullptr;
        cJSON_ArrayForEach(entry, cJSON_GetObjectItem(json, "items")) {
            Item item{Str(entry, "label"), Str(entry, "value"), Str(entry, "state"), Str(entry, "open"),
                      Str(entry, "area"), Str(entry, "local"), ""};
            const cJSON* act = cJSON_GetObjectItem(entry, "act");
            if (cJSON_IsObject(act)) {
                char* text = cJSON_PrintUnformatted(act);
                item.act = text ? text : "";
                cJSON_free(text);
            }
            s.items.push_back(std::move(item));
        }
        cJSON_ArrayForEach(entry, cJSON_GetObjectItem(json, "metrics")) {
            s.metrics.emplace_back(Str(entry, "value"), Str(entry, "label"));
        }
        const cJSON* points = cJSON_GetObjectItem(json, "points");
        if (cJSON_IsArray(points)) {
            const cJSON* decimals = cJSON_GetObjectItem(json, "decimals");
            const cJSON* last = cJSON_GetObjectItem(json, "last");
            int dec = cJSON_IsNumber(decimals) ? std::clamp(decimals->valueint, 0, 3) : 1;
            float lo = INFINITY, hi = -INFINITY;
            cJSON_ArrayForEach(entry, points) {
                float v = cJSON_IsNumber(entry) ? (float)entry->valuedouble : NAN;
                s.chart.push_back(v);
                if (!std::isnan(v)) {
                    lo = std::min(lo, v);
                    hi = std::max(hi, v);
                }
            }
            char text[48];
            std::string unit = Str(json, "unit");
            if (cJSON_IsNumber(last)) {
                snprintf(text, sizeof(text), "%.*f %s", dec, last->valuedouble, unit.c_str());
                s.chart_value = text;
            }
            if (std::isfinite(lo)) {
                snprintf(text, sizeof(text), "mín %.*f · máx %.*f", dec, lo, dec, hi);
                s.chart_range = text;
            }
        }
        cJSON_Delete(json);
    }
    screen_ = std::move(s);
    page_ = std::min(restore_page_, Pages() - 1);
    restore_page_ = 0;
    image_data_.reset();
    lv_image_set_src(image_, nullptr);
    if (!screen_.image.empty()) client_->FetchImage(screen_.image, seq_);
    next_refresh_us_ = esp_timer_get_time() + (screen_.id == "camera_live" ? kLiveRefreshUs : kDeviceRefreshUs);
    Render();
}

void NavView::OnImage(uint32_t seq, std::unique_ptr<LvglAllocatedImage> image) {
    if (seq != seq_ || !image || !image->IsValid()) return;
    image_data_ = std::move(image);
    lv_image_set_src(image_, image_data_->image_dsc());
}

void NavView::OnAction(uint32_t seq, bool ok) {
    if (seq != seq_) return;
    if (!ok) ESP_LOGW(TAG, "action failed on %s", screen_.id.c_str());
    // Reload either way: on success to show the real new state, on failure to undo the optimistic one.
    next_refresh_us_ = esp_timer_get_time() + kReloadAfterActionUs;
    screen_.loading = false;
    reload_pending_ = true;
}

// ── navigation ───────────────────────────────────────────────────────────────

bool NavView::AtHome() const { return areas_.empty() || (area_ == home_ && stack_.size() <= 1); }

void NavView::GoArea(int area, const std::string& open) {
    if (area < 0 || area >= (int)areas_.size()) return;
    area_ = area;
    stack_ = {areas_[area].screen};
    if (!open.empty()) stack_.push_back(open);
    Load(stack_.back());
}

void NavView::Push(const std::string& id) {
    stack_.push_back(id);
    Load(id);
}

void NavView::Pop() {
    if (stack_.size() <= 1) return;
    stack_.pop_back();
    Load(stack_.back());
}

void NavView::Load(const std::string& id, bool quiet) {
    seq_++;
    reload_pending_ = false;
    client_->FetchScreen(id, seq_);
    if (quiet) {
        restore_page_ = page_;
        return;
    }
    page_ = 0;
    restore_page_ = 0;
    screen_ = Screen{};
    screen_.id = id;
    image_data_.reset();
    lv_image_set_src(image_, nullptr);
    Render();
}

int NavView::Pages() const {
    if (screen_.kind != "list") return 1;
    return std::max<int>(1, (screen_.items.size() + kRows - 1) / kRows);
}

void NavView::OnGesture(Gesture gesture, int x, int y) {
    last_touch_us_ = esp_timer_get_time();
    if (areas_.empty()) {
        client_->FetchNav();
        return;
    }
    switch (gesture) {
        case Gesture::kLeft:  // the content follows the finger: the next area comes in from the right
            GoArea(area_ + 1);
            break;
        case Gesture::kRight:
            GoArea(area_ - 1);
            break;
        case Gesture::kUp:
            if (page_ + 1 < Pages()) {
                page_++;
                Render();
            } else if (!screen_.next.empty()) {
                Push(screen_.next);
            }
            break;
        case Gesture::kDown:
            if (page_ > 0) {
                page_--;
                Render();
            } else {
                Pop();
            }
            break;
        case Gesture::kTap:
            Tap(x, y);
            break;
    }
}

void NavView::Tap(int x, int y) {
    if (screen_.id == "camera_live") {  // tap: a fresh frame now
        next_refresh_us_ = 0;
        return;
    }
    if (screen_.kind != "list") return;
    for (int i = 0; i < kRows; i++) {
        if (y < kRowTop[i] || y > kRowTop[i] + kRowH) continue;
        size_t index = page_ * kRows + i;
        if (index >= screen_.items.size()) return;
        Item& item = screen_.items[index];
        ESP_LOGI(TAG, "tap %s", item.label.c_str());
        if (!item.area.empty()) {
            for (size_t a = 0; a < areas_.size(); a++) {
                if (areas_[a].screen == item.area) {
                    GoArea(a, item.open);
                    return;
                }
            }
        } else if (!item.open.empty()) {
            Push(item.open);
        } else if (!item.act.empty()) {
            if (item.state == "on" || item.state == "off") item.state = item.state == "on" ? "off" : "on";
            client_->Action(item.act, seq_);
            Render();
        } else if (!item.local.empty()) {
            RunLocal(item);
        }
        return;
    }
}

void NavView::RunLocal(Item& item) {
    auto& board = Board::GetInstance();
    if (item.local == "volume+" || item.local == "volume-") {
        auto codec = board.GetAudioCodec();
        int v = std::clamp(codec->output_volume() + (item.local == "volume+" ? 10 : -10), 10, 100);
        codec->SetOutputVolume(v);
    } else if (item.local == "bright+" || item.local == "bright-") {
        auto backlight = board.GetBacklight();
        if (backlight) {
            int b = std::clamp(backlight->brightness() + (item.local == "bright+" ? 15 : -15), 10, 100);
            backlight->SetBrightness(b, true);
        }
    }
    Render();
}

void NavView::Tick(int64_t now_us) {
    if (areas_.empty()) return;
    if (!AtHome() && last_touch_us_ && now_us - last_touch_us_ > home_timeout_us_) {
        GoArea(home_);
        return;
    }
    if (now_us < next_refresh_us_ || screen_.loading) return;
    if (reload_pending_) {
        Load(screen_.id, true);
    } else if (screen_.kind == "device") {
        next_refresh_us_ = now_us + kDeviceRefreshUs;
        Render();
    } else if (screen_.id == "camera_live" && !screen_.image.empty()) {
        next_refresh_us_ = now_us + kLiveRefreshUs;
        client_->FetchImage(screen_.image, seq_);
    }
}

// ── drawing ──────────────────────────────────────────────────────────────────

void NavView::RenderDots() {
    lv_obj_clean(area_dots_);
    for (size_t i = 0; i < areas_.size(); i++) {
        lv_obj_t* dot = Plain(area_dots_);
        bool on = (int)i == area_;
        lv_obj_set_size(dot, on ? 14 : 6, 6);
        lv_obj_set_style_radius(dot, 3, 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(dot, Rgb(on ? ((int)i == home_ ? kAccent : kText) : kTextFaint), 0);
    }
    lv_obj_clean(depth_dots_);
    int depth = stack_.size() + Pages() - 1;
    if (depth > 1) {
        int current = stack_.size() - 1 + page_;
        for (int i = 0; i < depth && i < 6; i++) {
            lv_obj_t* dot = Plain(depth_dots_);
            lv_obj_set_size(dot, 6, i == current ? 14 : 6);
            lv_obj_set_style_radius(dot, 3, 0);
            lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(dot, Rgb(i == current ? kText : kTextFaint), 0);
        }
    }
}

void NavView::Render() {
    RenderDots();
    const std::string& kind = screen_.kind;
    Show(note_, screen_.loading || kind == "note");
    Show(metrics_, kind == "metrics" || kind == "device");
    Show(chart_box_, kind == "chart");
    Show(image_, kind == "photo");
    Show(caption_, kind == "photo");
    for (auto row : rows_) Show(row, false);

    std::string title = screen_.title;
    if (Pages() > 1) {
        char text[32];
        snprintf(text, sizeof(text), " · %d/%d", page_ + 1, Pages());
        title += text;
    }
    lv_label_set_text(title_, title.c_str());

    if (screen_.loading) {
        lv_label_set_text(note_, "…");
        lv_label_set_text(footer_, "");
        Show(footer_arrow_, false);
        return;
    }
    if (kind == "note") lv_label_set_text(note_, screen_.text.c_str());
    if (kind == "list") RenderList();
    if (kind == "metrics") RenderMetrics();
    if (kind == "device") RenderDevice();
    if (kind == "chart") RenderChart();
    if (kind == "photo") lv_label_set_text(caption_, screen_.caption.c_str());

    std::string footer = page_ + 1 < Pages() ? "mais" : screen_.next_name;
    lv_label_set_text(footer_, footer.c_str());
    Show(footer_arrow_, !footer.empty());
}

void NavView::RenderList() {
    auto& board = Board::GetInstance();
    for (int i = 0; i < kRows; i++) {
        size_t index = page_ * kRows + i;
        if (index >= screen_.items.size()) continue;
        const Item& item = screen_.items[index];
        Show(rows_[i], true);
        lv_label_set_text(row_labels_[i], item.label.c_str());
        std::string value = item.value;
        if (item.local == "volume+" || item.local == "volume-") {
            value = std::to_string(board.GetAudioCodec()->output_volume());
        } else if ((item.local == "bright+" || item.local == "bright-") && board.GetBacklight()) {
            value = std::to_string(board.GetBacklight()->brightness());
        } else if (!item.open.empty() || !item.area.empty()) {
            value = ">";
        }
        bool dot = !item.state.empty();
        lv_label_set_text(row_values_[i], dot ? "" : value.c_str());
        Show(row_dots_[i], dot);
        if (dot) lv_obj_set_style_bg_color(row_dots_[i], Rgb(StateColor(item.state)), 0);
    }
}

void NavView::RenderMetrics() {
    for (size_t i = 0; i < metric_values_.size(); i++) {
        bool has = i < screen_.metrics.size();
        lv_label_set_text(metric_values_[i], has ? screen_.metrics[i].first.c_str() : "");
        lv_label_set_text(metric_labels_[i], has ? screen_.metrics[i].second.c_str() : "");
    }
}

void NavView::RenderDevice() {
    auto& board = Board::GetInstance();
    auto& wifi = WifiManager::GetInstance();
    char rssi[16], battery[16], uptime[16];
    snprintf(rssi, sizeof(rssi), "%d", wifi.IsConnected() ? wifi.GetRssi() : 0);
    int level = 0;
    bool charging = false, discharging = false;
    if (board.GetBatteryLevel(level, charging, discharging)) {
        snprintf(battery, sizeof(battery), charging ? "%d%%+" : "%d%%", level);
    } else {
        snprintf(battery, sizeof(battery), "--");
    }
    int64_t minutes = esp_timer_get_time() / 60000000;
    if (minutes < 60) snprintf(uptime, sizeof(uptime), "%d min", (int)minutes);
    else snprintf(uptime, sizeof(uptime), "%d h", (int)(minutes / 60));
    screen_.metrics = {{rssi, "Wi-Fi dBm"}, {battery, "bateria"},
                       {client_->online() ? "online" : "offline", "r2d2"}, {uptime, "ligado há"}};
    RenderMetrics();
}

void NavView::RenderChart() {
    int n = screen_.chart.size();
    if (n < 2) return;
    float lo = INFINITY, hi = -INFINITY;
    for (float v : screen_.chart) {
        if (std::isnan(v)) continue;
        lo = std::min(lo, v);
        hi = std::max(hi, v);
    }
    if (!std::isfinite(lo)) return;
    const float factor = 10.0f;  // one decimal is plenty at 180 px
    float pad = std::max((hi - lo) * 0.1f, 0.1f);
    lv_chart_set_point_count(chart_, n);
    lv_chart_set_axis_range(chart_, LV_CHART_AXIS_PRIMARY_Y, (int32_t)std::floor((lo - pad) * factor),
                            (int32_t)std::ceil((hi + pad) * factor));
    for (int i = 0; i < n; i++) {
        float v = screen_.chart[i];
        lv_chart_set_series_value_by_id(chart_, chart_series_, i,
                                        std::isnan(v) ? LV_CHART_POINT_NONE : (int32_t)std::lround(v * factor));
    }
    lv_chart_refresh(chart_);
    lv_label_set_text(chart_value_, screen_.chart_value.c_str());
    lv_label_set_text(chart_range_, screen_.chart_range.c_str());
}

}  // namespace r2d2
