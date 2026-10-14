#include "hilily_display.h"
#include "application.h"
#include "device_state.h"

#include <cstdio>
#include <algorithm>
#include <cstdlib>
#include <limits>
#include <utility>

namespace {
void SetupText(lv_obj_t* label, int width) {
    lv_obj_set_width(label, width);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xF5FAFF), 0);
}
}


// LRC timestamp parser: [mm:ss.xx], [mm:ss.xxx], multiple stamps/line.
namespace {
std::vector<std::pair<uint32_t,std::string>> ParseLrc(const std::string& lrc) {
    std::vector<std::pair<uint32_t,std::string>> result;
    size_t begin = 0;
    while (begin < lrc.size() && result.size() < 256) {
        size_t end = lrc.find('\n', begin);
        if (end == std::string::npos) end = lrc.size();
        std::string line = lrc.substr(begin, end - begin);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::vector<uint32_t> stamps;
        size_t pos = 0;
        while (pos < line.size() && line[pos] == '[') {
            size_t close = line.find(']', pos + 1);
            if (close == std::string::npos) break;
            const std::string ts = line.substr(pos + 1, close - pos - 1);
            size_t colon = ts.find(':');
            if (colon == std::string::npos || colon == 0 || colon > 3) break;
            auto digits = [](const std::string& s) {
                return !s.empty() && std::all_of(s.begin(),s.end(),[](unsigned char ch){return ch >= '0' && ch <= '9';});
            };
            std::string mm = ts.substr(0, colon), sec = ts.substr(colon+1);
            size_t dot = sec.find('.');
            std::string ss = sec.substr(0,dot), frac = dot == std::string::npos ? "" : sec.substr(dot+1);
            if (!digits(mm) || ss.size()!=2 || !digits(ss) || (!frac.empty() && (!digits(frac) || frac.size()>3)) || (dot!=std::string::npos && frac.empty())) break;
            unsigned m = std::strtoul(mm.c_str(),nullptr,10), s = std::strtoul(ss.c_str(),nullptr,10);
            if (s>=60 || m>999) break;
            uint32_t ms = (m*60+s)*1000;
            if (frac.size()==1) ms += std::strtoul(frac.c_str(),nullptr,10)*100;
            if (frac.size()==2) ms += std::strtoul(frac.c_str(),nullptr,10)*10;
            if (frac.size()==3) ms += std::strtoul(frac.c_str(),nullptr,10);
            stamps.push_back(ms);
            pos=close+1;
        }
        std::string lyric = line.substr(pos);
        while (!lyric.empty() && lyric.front()==' ') lyric.erase(0,1);
        if (lyric.size()>192) lyric.resize(192);
        if (!lyric.empty()) for(uint32_t ms:stamps) {
            if(result.size()>=256)break;
            result.emplace_back(ms,lyric);
        }
        begin = end == lrc.size()?end:end+1;
    }
    std::stable_sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.first<b.first;});
    return result;
}
std::string FormatTime(uint32_t ms) {
    char s[20]; std::snprintf(s,sizeof(s),"%02lu:%02lu",(unsigned long)(ms/60000),(unsigned long)(ms/1000%60));
    return s;
}
}

void HiLilyDisplay::SetMusicTrack(std::string title, std::string artist, uint32_t duration_ms,
                                 const std::string& lrc) {
    auto parsed=ParseLrc(lrc);
    std::lock_guard<std::mutex> lock(music_mutex_);
    music_title_=std::move(title); music_artist_=std::move(artist);
    music_duration_ms_=duration_ms; music_position_ms_=0;
    lyric_lines_.clear();
    for(auto& item:parsed) lyric_lines_.push_back({item.first,std::move(item.second)});
    music_active_=true; music_playing_=false; ++music_revision_;
}
void HiLilyDisplay::SetMusicPosition(uint32_t position_ms,bool playing) {
    std::lock_guard<std::mutex> lock(music_mutex_);
    if(!music_active_)return;
    uint32_t next = music_duration_ms_ ? std::min(position_ms,music_duration_ms_) : position_ms;
    // Limit LVGL redraws to ~7Hz; seek/pause still forces an immediate redraw.
    const bool changed_state = music_playing_ != playing;
    if (!changed_state && next >= music_position_ms_ &&
        next - music_position_ms_ < 140 && next != music_duration_ms_) return;
    music_position_ms_ = next;
    music_playing_ = playing;
    ++music_revision_;
}
void HiLilyDisplay::ClearMusic() {
    std::lock_guard<std::mutex> lock(music_mutex_);
    music_active_=false; music_playing_=false; lyric_lines_.clear(); ++music_revision_;
}

void HiLilyDisplay::SetHomeData(std::string location, std::string condition,
                               std::string lunar_date, double temperature) {
    std::lock_guard<std::mutex> guard(home_mutex_);
    location_ = std::move(location);
    condition_ = std::move(condition);
    lunar_date_ = std::move(lunar_date);
    temperature_ = temperature;
    available_ = true;
    ++revision_;
}

void HiLilyDisplay::SetupUI() {
    SpiLcdDisplay::SetupUI();
    if (!Lock(200)) return;

    panel_ = lv_obj_create(lv_screen_active());
    lv_obj_set_size(panel_, 224, 154);
    lv_obj_align(panel_, LV_ALIGN_CENTER, 0, 12);
    lv_obj_set_style_bg_color(panel_, lv_color_hex(0x11243A), 0);
    lv_obj_set_style_bg_opa(panel_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel_, 0, 0);
    lv_obj_set_style_radius(panel_, 14, 0);
    lv_obj_set_style_pad_all(panel_, 8, 0);
    lv_obj_set_flex_flow(panel_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(panel_, 9, 0);
    lv_obj_clear_flag(panel_, LV_OBJ_FLAG_SCROLLABLE);

    location_label_ = lv_label_create(panel_);
    temperature_label_ = lv_label_create(panel_);
    condition_label_ = lv_label_create(panel_);
    lunar_label_ = lv_label_create(panel_);
    for (auto* label : {location_label_, temperature_label_, condition_label_, lunar_label_}) {
        SetupText(label, 207);
    }
    lv_label_set_text(location_label_, "Hi Lily");
    lv_label_set_text(temperature_label_, "--°C");
    lv_label_set_text(condition_label_, "Đang tải thời tiết...");
    lv_label_set_text(lunar_label_, "Âm lịch: --/--");
    lv_obj_add_flag(panel_, LV_OBJ_FLAG_HIDDEN);

    music_panel_ = lv_obj_create(lv_screen_active());
    lv_obj_set_size(music_panel_,224,158);
    lv_obj_align(music_panel_,LV_ALIGN_CENTER,0,12);
    lv_obj_set_style_bg_color(music_panel_,lv_color_hex(0x101927),0);
    lv_obj_set_style_bg_opa(music_panel_,LV_OPA_COVER,0);
    lv_obj_set_style_border_width(music_panel_,0,0);
    lv_obj_set_style_pad_all(music_panel_,8,0);
    lv_obj_clear_flag(music_panel_,LV_OBJ_FLAG_SCROLLABLE);
    music_title_label_=lv_label_create(music_panel_);
    music_artist_label_=lv_label_create(music_panel_);
    music_lyric_label_=lv_label_create(music_panel_);
    music_time_label_=lv_label_create(music_panel_);
    music_progress_=lv_bar_create(music_panel_);
    for(auto* label:{music_title_label_,music_artist_label_,music_lyric_label_,music_time_label_})
        SetupText(label,204);
    lv_obj_align(music_title_label_,LV_ALIGN_TOP_MID,0,0);
    lv_obj_align(music_artist_label_,LV_ALIGN_TOP_MID,0,24);
    lv_obj_set_size(music_lyric_label_,204,52);
    lv_label_set_long_mode(music_lyric_label_,LV_LABEL_LONG_WRAP);
    lv_obj_align(music_lyric_label_,LV_ALIGN_CENTER,0,2);
    lv_obj_set_style_text_color(music_lyric_label_,lv_color_hex(0x7DD3FC),0);
    lv_obj_align(music_time_label_,LV_ALIGN_BOTTOM_MID,0,0);
    lv_obj_set_size(music_progress_,194,5);
    lv_obj_align(music_progress_,LV_ALIGN_BOTTOM_MID,0,-22);
    lv_bar_set_range(music_progress_,0,1000);
    lv_obj_add_flag(music_panel_,LV_OBJ_FLAG_HIDDEN);
    Unlock();
}

void HiLilyDisplay::UpdateStatusBar(bool update_all) {
    SpiLcdDisplay::UpdateStatusBar(update_all);
    if (!panel_) return;

    const bool idle = Application::GetInstance().GetDeviceState() == kDeviceStateIdle;
    std::string loc, condition, lunar;
    double temp = 0;
    bool data_ready;
    uint32_t revision;
    {
        std::lock_guard<std::mutex> guard(home_mutex_);
        data_ready = available_;
        revision = revision_;
        if (revision != rendered_revision_) {
            loc = location_;
            condition = condition_;
            lunar = lunar_date_;
            temp = temperature_;
        }
    }

    bool music_active,playing;
    uint32_t music_revision, position, duration;
    std::string title, artist, lyric;
    {
        std::lock_guard<std::mutex> guard(music_mutex_);
        music_active=music_active_; playing=music_playing_;
        music_revision=music_revision_; position=music_position_ms_;
        duration=music_duration_ms_;
        if(music_revision!=music_rendered_revision_) {
            title=music_title_; artist=music_artist_;
            lyric="Chưa có lời bài hát";
            if(!lyric_lines_.empty()) {
                auto it=std::upper_bound(lyric_lines_.begin(),lyric_lines_.end(),position,
                    [](uint32_t ms,const LyricLine& item){return ms<item.time_ms;});
                lyric = it==lyric_lines_.begin() ? "" : std::prev(it)->text;
            }
        }
    }
    if (!Lock(50)) return;
    if (music_active != was_music_) {
        if(music_active) {
            lv_obj_remove_flag(music_panel_,LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(panel_,LV_OBJ_FLAG_HIDDEN);
            if(emoji_box_)lv_obj_add_flag(emoji_box_,LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(music_panel_,LV_OBJ_FLAG_HIDDEN);
            if (idle) {
                lv_obj_remove_flag(panel_, LV_OBJ_FLAG_HIDDEN);
                if (emoji_box_) lv_obj_add_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
            } else if (emoji_box_) lv_obj_remove_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
        }
        was_music_=music_active;
    }
    if(music_active && music_revision!=music_rendered_revision_) {
        lv_label_set_text(music_title_label_,title.c_str());
        lv_label_set_text(music_artist_label_,artist.c_str());
        lv_label_set_text(music_lyric_label_,lyric.c_str());
        std::string time=FormatTime(position)+" / "+FormatTime(duration);
        lv_label_set_text(music_time_label_,time.c_str());
        lv_bar_set_value(music_progress_,duration ?
            (int)((uint64_t)std::min(position,duration)*1000/duration):0,LV_ANIM_OFF);
        music_rendered_revision_=music_revision;
    }
    if (idle != was_idle_) {
        if (idle && !music_active) {
            lv_obj_remove_flag(panel_, LV_OBJ_FLAG_HIDDEN);
            if (emoji_box_) lv_obj_add_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(panel_, LV_OBJ_FLAG_HIDDEN);
            if (emoji_box_ && !music_active) lv_obj_remove_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
        }
        was_idle_ = idle;
    }
    if (data_ready && revision != rendered_revision_) {
        char number[32];
        std::snprintf(number, sizeof(number), "%.0f°C", temp);
        lv_label_set_text(location_label_, loc.c_str());
        lv_label_set_text(temperature_label_, number);
        lv_label_set_text(condition_label_, condition.c_str());
        lv_label_set_text_fmt(lunar_label_, "Âm lịch: %s", lunar.c_str());
        rendered_revision_ = revision;
    }
    Unlock();
}
