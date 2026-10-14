#ifndef HILILY_DISPLAY_H
#define HILILY_DISPLAY_H

#include "display/lcd_display.h"

#include <cstdint>
#include <mutex>
#include <string>

// Weather/lunar UI for this board ONLY. The regular Xiaozhi emotion and
// subtitle surfaces remain unchanged outside the idle state.
class HiLilyDisplay : public SpiLcdDisplay {
public:
    using SpiLcdDisplay::SpiLcdDisplay;

    void SetupUI() override;
    void UpdateStatusBar(bool update_all = false) override;
    void SetHomeData(std::string location, std::string condition,
                     std::string lunar_date, double temperature);

private:
    std::mutex home_mutex_;
    std::string location_;
    std::string condition_;
    std::string lunar_date_;
    double temperature_ = 0;
    uint32_t revision_ = 0;
    uint32_t rendered_revision_ = UINT32_MAX;
    bool available_ = false;
    bool was_idle_ = false;

    lv_obj_t* panel_ = nullptr;
    lv_obj_t* location_label_ = nullptr;
    lv_obj_t* temperature_label_ = nullptr;
    lv_obj_t* condition_label_ = nullptr;
    lv_obj_t* lunar_label_ = nullptr;
};

#endif  // HILILY_DISPLAY_H
