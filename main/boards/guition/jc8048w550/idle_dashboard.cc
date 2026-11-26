#include "idle_dashboard.h"

#include "lunar_calendar.h"
#include "vn_events.h"
#include "weather_icon.h"

#include <material_symbols.h>

#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>

using dashboard::Repo;
using dashboard::Snapshot;
using dashboard::Source;
using dashboard::Weather;

namespace {

const lv_color_t kCard = lv_color_hex(0x24242c);
const lv_color_t kDivider = lv_color_hex(0x34343e);
const lv_color_t kText = lv_color_hex(0xe8e8ec);
const lv_color_t kMuted = lv_color_hex(0x8f8fa0);
const lv_color_t kStar = lv_color_hex(0xf5c542);
const lv_color_t kWarning = lv_color_hex(0xff9f43);

constexpr int32_t kCardPadding = 8;
constexpr int32_t kRowHeight = 34;
constexpr int32_t kStatsWidth = 72;  // star icon + star count, right-aligned
constexpr int32_t kRowPadding = 8;   // left/right padding and gap inside a repo row
constexpr int32_t kBadgeSize = 22;
constexpr int64_t kWeatherMaxAgeSeconds = 2 * 30 * 60;
constexpr int64_t kReposMaxAgeSeconds = 2 * 60 * 60;

bool TimeSynced() {
    time_t now = time(nullptr);
    struct tm local;
    localtime_r(&now, &local);
    return local.tm_year >= 2025 - 1900;
}

// A source is stale when its last attempt failed or its data is too old.
bool IsStale(bool last_failed, int64_t fetched_at_s, int64_t max_age_s) {
    if (last_failed) {
        return true;
    }
    return TimeSynced() && time(nullptr) - fetched_at_s > max_age_s;
}

void SetText(lv_obj_t* label, const char* text) {
    if (strcmp(lv_label_get_text(label), text) != 0) {
        lv_label_set_text(label, text);
    }
}

void SetHidden(lv_obj_t* obj, bool hidden) {
    if (hidden) {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

// A transparent, borderless box with no padding that neither scrolls nor takes taps.
lv_obj_t* Box(lv_obj_t* parent) {
    lv_obj_t* box = lv_obj_create(parent);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_style_pad_all(box, 0, 0);
    lv_obj_set_style_radius(box, 0, 0);
    return box;
}

lv_obj_t* Label(lv_obj_t* parent, const lv_font_t* font, lv_color_t color, const char* text = "") {
    lv_obj_t* label = lv_label_create(parent);
    if (font != nullptr) {
        lv_obj_set_style_text_font(label, font, 0);
    }
    lv_obj_set_style_text_color(label, color, 0);
    lv_label_set_text(label, text);
    return label;
}

void Column(lv_obj_t* box, lv_flex_align_t cross) {
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_START, cross, cross);
}

void Row(lv_obj_t* box, lv_flex_align_t main, lv_flex_align_t cross) {
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(box, main, cross, cross);
}

lv_color_t RankColor(int index) {
    switch (index) {
        case 0:
            return lv_color_hex(0xf5c542);
        case 1:
            return lv_color_hex(0xc0c0c8);
        case 2:
            return lv_color_hex(0xcd7f32);
        default:
            return lv_color_hex(0x3b82f6);
    }
}

}  // namespace

IdleDashboard::IdleDashboard(lv_obj_t* content_parent, lv_obj_t* overlay_parent,
                             int32_t content_width, const DashboardFonts& fonts)
    : fonts_(fonts) {
    lv_obj_set_flex_flow(content_parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content_parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(content_parent, 4, 0);
    lv_obj_set_style_pad_column(content_parent, 0, 0);

    BuildSummaryCard(content_parent, content_width);
    BuildRepoList(content_parent, content_width);
    BuildDetail(overlay_parent);

    RefreshClock();
    clock_timer_ = lv_timer_create(OnClockTimer, 10000, this);
}

IdleDashboard::~IdleDashboard() {
    if (clock_timer_ != nullptr) {
        lv_timer_delete(clock_timer_);
    }
}

void IdleDashboard::BuildSummaryCard(lv_obj_t* parent, int32_t width) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(card, width, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_bg_color(card, kCard, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, kCardPadding, 0);
    lv_obj_set_style_pad_row(card, 4, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);

    // Top row: the solar and lunar date on the left, today's weather on the right.
    lv_obj_t* top = Box(card);
    lv_obj_set_size(top, lv_pct(100), LV_SIZE_CONTENT);
    Row(top, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* left = Box(top);
    lv_obj_set_size(left, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    Column(left, LV_FLEX_ALIGN_START);
    solar_ = Label(left, fonts_.solar, kText, "--/--/----");
    lunar_line1_ = Label(left, fonts_.lunar, kText);
    lunar_line2_ = Label(left, fonts_.lunar, kMuted);

    lv_obj_t* right = Box(top);
    lv_obj_set_size(right, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    Column(right, LV_FLEX_ALIGN_END);
    weather_box_ = Box(right);
    lv_obj_set_size(weather_box_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    Row(weather_box_, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(weather_box_, 10, 0);
    weather_icon_ = CreateWeatherIcon(weather_box_, 56);
    lv_obj_t* temps = Box(weather_box_);
    lv_obj_set_size(temps, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    Column(temps, LV_FLEX_ALIGN_START);
    temp_ = Label(temps, fonts_.temp, kText);
    condition_ = Label(temps, fonts_.small, kText);
    range_ = Label(temps, fonts_.small, kMuted);
    weather_status_ = Label(right, fonts_.small, kWarning, "loading...");

    // Meta row: solar term and city on the left, today's holiday on the right.
    lv_obj_t* meta = Box(card);
    lv_obj_set_size(meta, lv_pct(100), LV_SIZE_CONTENT);
    Row(meta, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(meta, 8, 0);
    term_ = Label(meta, fonts_.lunar, kMuted);
    city_icon_ = Label(meta, fonts_.icon, kMuted, MATERIAL_SYMBOLS_LOCATION_ON);
    city_ = Label(meta, fonts_.text, kMuted, "--");
    event_ = Label(meta, fonts_.lunar, kWarning);
    lv_obj_set_flex_grow(event_, 1);
    lv_obj_set_style_text_align(event_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(event_, LV_LABEL_LONG_MODE_CLIP);
    SetHidden(city_icon_, true);
    SetHidden(city_, true);
    SetHidden(event_, true);

    // Bottom row: the seven days after today.
    days_ = Box(card);
    lv_obj_set_size(days_, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_border_side(days_, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(days_, 1, 0);
    lv_obj_set_style_border_color(days_, kDivider, 0);
    lv_obj_set_style_pad_top(days_, 4, 0);
    Row(days_, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START);
    const int32_t column_width = (width - 2 * kCardPadding) / 7;
    for (DayColumn& day : day_columns_) {
        lv_obj_t* column = Box(days_);
        lv_obj_set_size(column, column_width, LV_SIZE_CONTENT);
        Column(column, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(column, 2, 0);
        day.weekday = Label(column, fonts_.small, kMuted);
        day.icon = CreateWeatherIcon(column, 28);
        day.temps = Label(column, fonts_.small, kText);
    }
    SetHidden(days_, true);
    SetHidden(weather_box_, true);
}

void IdleDashboard::BuildRepoList(lv_obj_t* parent, int32_t width) {
    lv_obj_t* header = Box(parent);
    lv_obj_set_size(header, width, LV_SIZE_CONTENT);
    Row(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(header, 2, 0);
    Label(header, fonts_.small, kMuted, "GitHub \xE2\x80\xA2 AI trending");
    repo_tag_ = Label(header, fonts_.small, kWarning, "loading...");

    for (int i = 0; i < kRepoRows; ++i) {
        RepoRow& row = rows_[i];
        row.owner = this;
        row.index = i;
        row.row = lv_obj_create(parent);
        lv_obj_remove_flag(row.row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(row.row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(row.row, width, kRowHeight);
        lv_obj_set_style_radius(row.row, 8, 0);
        lv_obj_set_style_bg_color(row.row, kCard, 0);
        lv_obj_set_style_bg_opa(row.row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(row.row, 0, 0);
        lv_obj_set_style_pad_hor(row.row, kRowPadding, 0);
        lv_obj_set_style_pad_ver(row.row, 0, 0);
        lv_obj_set_style_pad_column(row.row, kRowPadding, 0);
        Row(row.row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
        lv_obj_add_event_cb(row.row, OnRowClicked, LV_EVENT_CLICKED, &row);

        row.badge = Box(row.row);
        lv_obj_set_size(row.badge, kBadgeSize, kBadgeSize);
        lv_obj_set_style_radius(row.badge, kBadgeSize / 2, 0);
        lv_obj_set_style_bg_color(row.badge, RankColor(i), 0);
        lv_obj_set_style_bg_opa(row.badge, LV_OPA_COVER, 0);
        char rank_text[4];
        snprintf(rank_text, sizeof(rank_text), "%d", i + 1);
        // Gold and silver badges take dark digits, the rest white.
        row.rank = Label(row.badge, fonts_.small, i < 2 ? lv_color_hex(0x222222) : lv_color_white(),
                         rank_text);
        lv_obj_center(row.rank);

        // The name takes all the width left by the badge, the stats and the gaps
        // between them; the description is only shown in the detail card.
        row.name = Label(row.row, fonts_.text, kText);
        lv_obj_set_width(row.name,
                         width - 2 * kRowPadding - kBadgeSize - kStatsWidth - 2 * kRowPadding);
        lv_label_set_long_mode(row.name, LV_LABEL_LONG_MODE_CLIP);
        lv_obj_t* stats = Box(row.row);
        lv_obj_set_size(stats, kStatsWidth, LV_SIZE_CONTENT);
        Row(stats, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(stats, 2, 0);
        row.star = Label(stats, fonts_.icon, kStar, MATERIAL_SYMBOLS_STAR);
        row.stars = Label(stats, fonts_.small, kStar);
        SetHidden(row.row, true);
    }
}

void IdleDashboard::BuildDetail(lv_obj_t* parent) {
    // The overlay covers the whole parent, padding included, and swallows taps.
    scrim_ = lv_obj_create(parent);
    lv_obj_add_flag(scrim_, LV_OBJ_FLAG_FLOATING);
    lv_obj_add_flag(scrim_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(scrim_, LV_OBJ_FLAG_SCROLLABLE);
    // A new object has no coordinates until a layout pass runs: without this the
    // overlay would be created 0x0.
    lv_obj_update_layout(parent);
    lv_obj_set_size(scrim_, lv_obj_get_width(parent), lv_obj_get_height(parent));
    lv_obj_set_pos(scrim_, -lv_obj_get_style_pad_left(parent, LV_PART_MAIN),
                   -lv_obj_get_style_pad_top(parent, LV_PART_MAIN));
    lv_obj_set_style_radius(scrim_, 0, 0);
    lv_obj_set_style_bg_color(scrim_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scrim_, LV_OPA_60, 0);
    lv_obj_set_style_border_width(scrim_, 0, 0);
    lv_obj_set_style_pad_all(scrim_, 16, 0);
    lv_obj_set_flex_flow(scrim_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scrim_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_event_cb(scrim_, OnDetailClosed, LV_EVENT_CLICKED, this);

    // Taps on the card itself do nothing: events do not bubble to the scrim.
    lv_obj_t* card = lv_obj_create(scrim_);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(card, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_radius(card, 14, 0);
    lv_obj_set_style_bg_color(card, kCard, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 14, 0);
    lv_obj_set_style_pad_row(card, 8, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* title_row = Box(card);
    lv_obj_set_size(title_row, lv_pct(100), LV_SIZE_CONTENT);
    Row(title_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START);
    detail_name_ = Label(title_row, fonts_.text, kText);
    lv_obj_set_width(detail_name_, lv_pct(85));
    lv_label_set_long_mode(detail_name_, LV_LABEL_LONG_MODE_WRAP);

    lv_obj_t* close = lv_obj_create(title_row);
    lv_obj_add_flag(close, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(close, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(close, 32, 32);
    lv_obj_set_style_radius(close, 16, 0);
    lv_obj_set_style_bg_color(close, kDivider, 0);
    lv_obj_set_style_bg_opa(close, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(close, 0, 0);
    lv_obj_set_style_pad_all(close, 0, 0);
    lv_obj_t* close_label = Label(close, fonts_.small, kText, LV_SYMBOL_CLOSE);
    lv_obj_center(close_label);
    lv_obj_add_event_cb(close, OnDetailClosed, LV_EVENT_CLICKED, this);

    detail_description_ = Label(card, fonts_.text, kText);
    lv_obj_set_width(detail_description_, lv_pct(100));
    lv_label_set_long_mode(detail_description_, LV_LABEL_LONG_MODE_WRAP);
    detail_meta_ = Label(card, fonts_.text, kStar);
    lv_obj_set_width(detail_meta_, lv_pct(100));
    lv_label_set_long_mode(detail_meta_, LV_LABEL_LONG_MODE_WRAP);
    detail_topics_ = Label(card, fonts_.text, kMuted);
    lv_obj_set_width(detail_topics_, lv_pct(100));
    lv_label_set_long_mode(detail_topics_, LV_LABEL_LONG_MODE_WRAP);

    SetHidden(scrim_, true);
}

void IdleDashboard::Update(const Snapshot& snapshot) {
    UpdateWeather(snapshot.weather);
    UpdateRepos(snapshot.repos);
}

void IdleDashboard::UpdateWeather(const Source<Weather>& source) {
    if (!source.valid) {
        SetHidden(weather_box_, true);
        SetHidden(days_, true);
        SetHidden(city_icon_, true);
        SetHidden(city_, true);
        SetText(weather_status_, source.last_failed ? "no data" : "loading...");
        SetHidden(weather_status_, false);
        return;
    }
    const Weather& weather = source.data;
    char text[48];
    snprintf(text, sizeof(text),
             "%d\xC2\xB0"
             "C",
             weather.temp_c);
    SetText(temp_, text);
    SetText(condition_, weather.condition.c_str());
    snprintf(text, sizeof(text), "H%d\xC2\xB0 L%d\xC2\xB0  %d%%", weather.high_c, weather.low_c,
             weather.humidity);
    SetText(range_, text);
    SetWeatherIcon(weather_icon_, weather.icon);
    SetText(city_, weather.city.empty() ? "--" : weather.city.c_str());
    for (size_t i = 0; i < day_columns_.size(); ++i) {
        const dashboard::ForecastDay& day = weather.days[i];
        SetText(day_columns_[i].weekday, day.weekday.c_str());
        SetWeatherIcon(day_columns_[i].icon, day.icon);
        snprintf(text, sizeof(text), "%d\xC2\xB0/%d\xC2\xB0", day.high_c, day.low_c);
        SetText(day_columns_[i].temps, text);
    }

    bool stale = IsStale(source.last_failed, source.fetched_at_s, kWeatherMaxAgeSeconds);
    lv_opa_t opa = stale ? LV_OPA_50 : LV_OPA_COVER;
    lv_obj_set_style_opa(weather_box_, opa, 0);
    lv_obj_set_style_opa(days_, opa, 0);
    SetText(weather_status_, "offline");
    SetHidden(weather_status_, !stale);
    SetHidden(weather_box_, false);
    SetHidden(days_, false);
    SetHidden(city_icon_, false);
    SetHidden(city_, false);
}

void IdleDashboard::UpdateRepos(const Source<std::vector<Repo>>& source) {
    repos_ = source.valid ? source.data : std::vector<Repo>();
    bool stale = IsStale(source.last_failed, source.fetched_at_s, kReposMaxAgeSeconds);
    if (!source.valid) {
        SetText(repo_tag_, source.last_failed ? "no data" : "loading...");
        SetHidden(repo_tag_, false);
    } else {
        SetText(repo_tag_, "offline");
        SetHidden(repo_tag_, !stale);
    }
    for (int i = 0; i < kRepoRows; ++i) {
        RepoRow& row = rows_[i];
        if (i >= static_cast<int>(repos_.size())) {
            SetHidden(row.row, true);
            continue;
        }
        const Repo& repo = repos_[i];
        SetText(row.name, repo.full_name.c_str());
        SetText(row.stars, dashboard::FormatStars(repo.stars).c_str());
        lv_obj_set_style_opa(row.row, stale ? LV_OPA_50 : LV_OPA_COVER, 0);
        SetHidden(row.row, false);
    }
    // A repo list that shrank must not leave a detail card for a repo that is gone.
    HideDetail();
}

void IdleDashboard::RefreshClock() {
    time_t now = time(nullptr);
    struct tm local;
    localtime_r(&now, &local);
    if (local.tm_year < 2025 - 1900) {
        SetText(solar_, "--/--/----");
        SetText(lunar_line1_, "");
        SetText(lunar_line2_, "");
        SetText(term_, "");
        SetHidden(event_, true);
        return;
    }
    static const char* const kWeekdays[7] = {"Chủ Nhật", "Thứ Hai", "Thứ Ba", "Thứ Tư",
                                             "Thứ Năm",  "Thứ Sáu", "Thứ Bảy"};
    const int day = local.tm_mday;
    const int month = local.tm_mon + 1;
    const int year = local.tm_year + 1900;
    const lunar::LunarDate lunar_date = lunar::SolarToLunar(day, month, year);

    char text[128];
    snprintf(text, sizeof(text), "%s, %02d/%02d/%04d", kWeekdays[local.tm_wday], day, month, year);
    SetText(solar_, text);
    snprintf(text, sizeof(text), "Âm lịch: %d/%d%s năm %s", lunar_date.day, lunar_date.month,
             lunar_date.leap ? " nhuận" : "", lunar::YearCanChi(lunar_date.year).c_str());
    SetText(lunar_line1_, text);
    snprintf(text, sizeof(text), "Ngày %s · Tháng %s",
             lunar::DayCanChi(lunar::JulianDay(day, month, year)).c_str(),
             lunar::MonthCanChi(lunar_date).c_str());
    SetText(lunar_line2_, text);
    snprintf(text, sizeof(text), "Tiết %s", lunar::SolarTerm(day, month, year));
    SetText(term_, text);

    std::string events;
    for (const std::string& event : dashboard::EventsFor(day, month, year)) {
        events += (events.empty() ? "" : " · ") + event;
    }
    SetText(event_, events.c_str());
    SetHidden(event_, events.empty());
}

void IdleDashboard::ShowDetail(int index) {
    if (index < 0 || index >= static_cast<int>(repos_.size())) {
        return;
    }
    const Repo& repo = repos_[index];
    SetText(detail_name_, repo.full_name.c_str());
    SetText(detail_description_,
            repo.description.empty() ? "(no description)" : repo.description.c_str());
    std::string meta = "Language: " + (repo.language.empty() ? std::string("-") : repo.language) +
                       "    Stars: " + dashboard::FormatStars(repo.stars) +
                       "    Forks: " + dashboard::FormatStars(repo.forks) +
                       "    Updated: " + (repo.updated.empty() ? std::string("-") : repo.updated);
    SetText(detail_meta_, meta.c_str());
    std::string topics;
    for (const std::string& topic : repo.topics) {
        topics += (topics.empty() ? "" : ", ") + topic;
    }
    std::string topics_line = topics.empty() ? std::string() : "Topics: " + topics;
    SetText(detail_topics_, topics_line.c_str());
    SetHidden(detail_topics_, topics.empty());
    SetHidden(scrim_, false);
    lv_obj_move_foreground(scrim_);
}

void IdleDashboard::HideDetail() {
    if (scrim_ != nullptr) {
        SetHidden(scrim_, true);
    }
}

bool IdleDashboard::IsDetailOpen() const {
    return scrim_ != nullptr && !lv_obj_has_flag(scrim_, LV_OBJ_FLAG_HIDDEN);
}

void IdleDashboard::OnRowClicked(lv_event_t* event) {
    auto* row = static_cast<RepoRow*>(lv_event_get_user_data(event));
    row->owner->ShowDetail(row->index);
}

void IdleDashboard::OnDetailClosed(lv_event_t* event) {
    static_cast<IdleDashboard*>(lv_event_get_user_data(event))->HideDetail();
}

void IdleDashboard::OnClockTimer(lv_timer_t* timer) {
    static_cast<IdleDashboard*>(lv_timer_get_user_data(timer))->RefreshClock();
}
