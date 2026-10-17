// Touch navigation on the mini-jarvis round screen, smartwatch-like.
//
// Sideways moves between areas (Menu, Início, Clima, Casa...; the list comes
// from the Core's /satellite/nav), up goes deeper — the next page of a long
// list, then the screen's `next` —, down goes back, a tap follows the item
// under the finger. The content follows the finger: a drag to the right
// brings the area on the left. What each screen holds comes from the Core
// (/satellite/screen/<id>, r2d2 repo agent_core/channels/satellite/screens.py);
// this file only knows how to draw the kinds: list, metrics, chart, photo,
// note and the device's own status. "home" is the display's idle clock.
#pragma once

#include "display/lvgl_display/lvgl_image.h"

#include <cJSON.h>
#include <lvgl.h>

#include <memory>
#include <string>
#include <vector>

namespace r2d2 {

class CoreClient;

enum class Gesture { kTap, kLeft, kRight, kUp, kDown };  // kLeft: the finger moved left

class NavView {
public:
    NavView(lv_obj_t* parent, CoreClient* client);

    // All with the display lock held.
    void SetNav(const cJSON* nav);
    void OnScreen(uint32_t seq, const std::string& id, cJSON* screen);  // takes ownership
    void OnImage(uint32_t seq, std::unique_ptr<LvglAllocatedImage> image);
    void OnAction(uint32_t seq, bool ok);
    void OnGesture(Gesture gesture, int x, int y);
    void Tick(int64_t now_us);   // auto-home, live refreshes
    bool AtHome() const;         // home area, top level: the display shows its clock
    bool ready() const { return !areas_.empty(); }
    lv_obj_t* obj() const { return root_; }

private:
    struct Area {
        std::string name, screen;
    };
    struct Item {
        std::string label, value, state, open, area, local, act;  // act: JSON body
    };
    struct Screen {
        std::string id, kind, title, next, next_name, text, image, caption;
        std::vector<Item> items;
        std::vector<std::pair<std::string, std::string>> metrics;  // value, label
        std::vector<float> chart;                                  // NAN = gap
        std::string chart_value, chart_range;
        bool loading = true;
    };

    void GoArea(int area, const std::string& open = "");
    void Push(const std::string& id);
    void Pop();
    void Load(const std::string& id, bool quiet = false);  // quiet: keep showing the old content
    void Render();
    void RenderDots();
    void RenderList();
    void RenderMetrics();
    void RenderChart();
    void RenderDevice();
    void Tap(int x, int y);
    void RunLocal(Item& item);
    int Pages() const;

    CoreClient* client_;
    lv_obj_t* root_ = nullptr;
    lv_obj_t* area_dots_ = nullptr;
    lv_obj_t* depth_dots_ = nullptr;
    lv_obj_t* title_ = nullptr;
    lv_obj_t* footer_ = nullptr;
    lv_obj_t* footer_arrow_ = nullptr;
    lv_obj_t* note_ = nullptr;
    std::vector<lv_obj_t*> rows_;
    std::vector<lv_obj_t*> row_labels_, row_values_, row_dots_;
    lv_obj_t* metrics_ = nullptr;
    std::vector<lv_obj_t*> metric_values_, metric_labels_;
    lv_obj_t* chart_box_ = nullptr;
    lv_obj_t* chart_ = nullptr;
    lv_chart_series_t* chart_series_ = nullptr;
    lv_obj_t* chart_value_ = nullptr;
    lv_obj_t* chart_range_ = nullptr;
    lv_obj_t* image_ = nullptr;
    lv_obj_t* caption_ = nullptr;
    std::unique_ptr<LvglAllocatedImage> image_data_;

    std::vector<Area> areas_;
    int home_ = 0;
    int area_ = 0;
    int64_t home_timeout_us_ = 30LL * 1000 * 1000;
    std::vector<std::string> stack_;  // screen ids within the area, top = shown
    Screen screen_;
    int page_ = 0;
    uint32_t seq_ = 0;
    int64_t last_touch_us_ = 0;
    int64_t next_refresh_us_ = 0;
    bool reload_pending_ = false;
    int restore_page_ = 0;
};

}  // namespace r2d2
