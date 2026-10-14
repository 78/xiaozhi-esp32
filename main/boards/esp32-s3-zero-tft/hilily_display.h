#ifndef HILILY_DISPLAY_H
#define HILILY_DISPLAY_H

#include "display/lcd_display.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

// Weather/lunar UI for this board ONLY. The regular Xiaozhi emotion and
// subtitle surfaces remain unchanged outside the idle state.
class HiLilyDisplay : public SpiLcdDisplay {
public:
    using SpiLcdDisplay::SpiLcdDisplay;

    void SetupUI() override;
    // Playback controller must provide actual audio position; no fake timer.
    void SetMusicTrack(std::string title, std::string artist, uint32_t duration_ms,
                       const std::string& lrc);
    void SetMusicPosition(uint32_t position_ms, bool playing);
    void ClearMusic();
    void UpdateStatusBar(bool update_all = false) override;
    void SetHomeData(std::string location, std::string condition,
                     std::string lunar_date, double temperature);

private:
    struct LyricLine { uint32_t time_ms; std::string text; };
    std::mutex music_mutex_;
    std::vector<LyricLine> lyric_lines_;
    std::string music_title_, music_artist_;
    uint32_t music_duration_ms_ = 0, music_position_ms_ = 0, music_revision_ = 0;
    uint32_t music_rendered_revision_ = UINT32_MAX;
    bool music_active_ = false, music_playing_ = false, was_music_ = false;
    lv_obj_t* music_panel_ = nullptr;
    lv_obj_t* music_title_label_ = nullptr;
    lv_obj_t* music_artist_label_ = nullptr;
    lv_obj_t* music_lyric_label_ = nullptr;
    lv_obj_t* music_time_label_ = nullptr;
    lv_obj_t* music_progress_ = nullptr;

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
