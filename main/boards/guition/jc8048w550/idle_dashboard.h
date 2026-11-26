#ifndef JC8048W550_IDLE_DASHBOARD_H
#define JC8048W550_IDLE_DASHBOARD_H

#include "dashboard_model.h"

#include <lvgl.h>

#include <array>
#include <vector>

struct DashboardFonts {
    const lv_font_t* small;   // list rows, secondary text
    const lv_font_t* normal;  // detail title
    const lv_font_t* temp;    // current temperature
    const lv_font_t* day;     // big solar day number (bold digits only)
    const lv_font_t* solar;   // weekday name (Vietnamese letters, 20 px)
    const lv_font_t* lunar;   // lunar lines, solar term, event (Vietnamese letters, 16 px)
    const lv_font_t* icon;    // Material Symbols (star, location)
    // Text that comes from a web service (repo names and descriptions, the
    // city). nullptr inherits the screen's font, which is the theme's text font
    // (the assets' Noto/PuHui font) and follows it when the font is replaced.
    const lv_font_t* text;
};

// What the display shows while idle: a card with the solar and lunar date, the
// day's holiday and the weather (today plus seven days), the top GitHub AI repos, and a detail card
// for a repo the user taps. Every method must be called with the LVGL lock held (or from an LVGL
// timer or event callback, which already holds it).
class IdleDashboard {
public:
    static constexpr int kRepoRows = 5;

    // `content_parent` is filled top to bottom; `overlay_parent` hosts the
    // full-size detail overlay, covering everything inside it.
    IdleDashboard(lv_obj_t* content_parent, lv_obj_t* overlay_parent, int32_t content_width,
                  const DashboardFonts& fonts);
    ~IdleDashboard();

    void Update(const dashboard::Snapshot& snapshot);
    void RefreshClock();
    void ShowDetail(int index);
    void HideDetail();
    bool IsDetailOpen() const;

private:
    struct RepoRow {
        lv_obj_t* row = nullptr;
        lv_obj_t* badge = nullptr;
        lv_obj_t* rank = nullptr;
        lv_obj_t* name = nullptr;
        lv_obj_t* star = nullptr;
        lv_obj_t* stars = nullptr;
        IdleDashboard* owner = nullptr;
        int index = 0;
    };

    void BuildSummaryCard(lv_obj_t* parent, int32_t width);
    void BuildRepoList(lv_obj_t* parent, int32_t width);
    void BuildDetail(lv_obj_t* parent);
    void UpdateWeather(const dashboard::Source<dashboard::Weather>& source);
    void UpdateRepos(const dashboard::Source<std::vector<dashboard::Repo>>& source);

    static void OnRowClicked(lv_event_t* event);
    static void OnDetailClosed(lv_event_t* event);
    static void OnClockTimer(lv_timer_t* timer);

    DashboardFonts fonts_;

    lv_obj_t* weekday_ = nullptr;       // "Thứ Sáu"
    lv_obj_t* day_ = nullptr;           // "27"
    lv_obj_t* month_ = nullptr;         // "Tháng 11"
    lv_obj_t* year_ = nullptr;          // "2026"
    lv_obj_t* lunar_column_ = nullptr;  // the five lunar rows, hidden until the clock is set
    lv_obj_t* lunar_date_ = nullptr;    // "18/10", "15/6 nhuận"
    lv_obj_t* lunar_year_ = nullptr;    // "Bính Ngọ"
    lv_obj_t* day_name_ = nullptr;      // day can chi
    lv_obj_t* month_name_ = nullptr;    // month can chi
    lv_obj_t* term_ = nullptr;          // solar term name
    lv_obj_t* event_ = nullptr;         // today's holiday or giỗ, hidden when there is none
    lv_obj_t* city_icon_ = nullptr;
    lv_obj_t* city_ = nullptr;
    lv_obj_t* weather_box_ = nullptr;  // icon + temperature block, dimmed when stale
    lv_obj_t* weather_icon_ = nullptr;
    lv_obj_t* temp_ = nullptr;
    lv_obj_t* condition_ = nullptr;
    lv_obj_t* range_ = nullptr;
    lv_obj_t* weather_status_ = nullptr;  // "loading..." / "no data" / "offline"
    lv_obj_t* days_ = nullptr;
    struct DayColumn {
        lv_obj_t* weekday = nullptr;
        lv_obj_t* icon = nullptr;
        lv_obj_t* temps = nullptr;
    };
    std::array<DayColumn, 7> day_columns_;

    lv_obj_t* repo_tag_ = nullptr;  // "loading..." / "no data" / "offline"
    std::array<RepoRow, kRepoRows> rows_;
    std::vector<dashboard::Repo> repos_;

    lv_obj_t* scrim_ = nullptr;
    lv_obj_t* detail_name_ = nullptr;
    lv_obj_t* detail_description_ = nullptr;
    lv_obj_t* detail_meta_ = nullptr;
    lv_obj_t* detail_topics_ = nullptr;

    lv_timer_t* clock_timer_ = nullptr;
};

#endif  // JC8048W550_IDLE_DASHBOARD_H
