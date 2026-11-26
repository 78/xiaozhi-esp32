#include "weather_icon.h"

#include <cstdint>

using dashboard::WeatherIcon;

namespace {

// Every shape is laid out on a 48x48 grid and scaled to the icon's size.
constexpr int kGrid = 48;

const lv_color_t kSun = lv_color_hex(0xf5c542);
const lv_color_t kCloud = lv_color_hex(0xc9ccd6);
const lv_color_t kDarkCloud = lv_color_hex(0x8f95a3);
const lv_color_t kRain = lv_color_hex(0x4aa3ff);
const lv_color_t kSnow = lv_color_hex(0xffffff);

class Painter {
public:
    Painter(lv_obj_t* parent, int32_t size) : parent_(parent), size_(size) {}

    int32_t S(int grid_units) const { return (grid_units * size_ + kGrid / 2) / kGrid; }

    // A filled rounded rectangle at grid coordinates; radius_units >= half the
    // short side makes it a pill or a disc.
    lv_obj_t* Rect(int x, int y, int w, int h, int radius_units, lv_color_t color) {
        lv_obj_t* shape = lv_obj_create(parent_);
        lv_obj_remove_flag(shape, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(shape, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(shape, S(w), S(h));
        lv_obj_set_pos(shape, S(x), S(y));
        lv_obj_set_style_radius(shape, S(radius_units), 0);
        lv_obj_set_style_bg_color(shape, color, 0);
        lv_obj_set_style_bg_opa(shape, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(shape, 0, 0);
        lv_obj_set_style_pad_all(shape, 0, 0);
        return shape;
    }

    void Disc(int cx, int cy, int r, lv_color_t color) {
        Rect(cx - r, cy - r, 2 * r, 2 * r, r, color);
    }

    // A cloud made of a base bar and three discs, shifted by (dx, dy).
    void Cloud(int dx, int dy, lv_color_t color) {
        Rect(7 + dx, 28 + dy, 36, 12, 6, color);
        Disc(26 + dx, 24 + dy, 11, color);
        Disc(15 + dx, 29 + dy, 8, color);
        Disc(36 + dx, 30 + dy, 8, color);
    }

    // A disc with eight dots around it.
    void Sun(int cx, int cy, int r, int ray_distance, int ray_r) {
        Disc(cx, cy, r, kSun);
        constexpr int kDirs[8][2] = {{1, 0},  {1, 1},   {0, 1},  {-1, 1},
                                     {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};
        for (const auto& d : kDirs) {
            // Diagonal rays sit at ~0.7 of the distance so all eight are on a circle.
            int dist = (d[0] != 0 && d[1] != 0) ? ray_distance * 7 / 10 : ray_distance;
            Disc(cx + d[0] * dist, cy + d[1] * dist, ray_r, kSun);
        }
    }

    // A lightning bolt from three bars stepping down and to the left. No
    // rotation: a rotated object makes LVGL render it through a temporary layer.
    void Bolt(int x, int y, lv_color_t color) {
        Rect(x + 6, y, 6, 6, 1, color);
        Rect(x + 2, y + 5, 10, 5, 1, color);
        Rect(x, y + 9, 6, 7, 1, color);
    }

private:
    lv_obj_t* parent_;
    int32_t size_;
};

}  // namespace

lv_obj_t* CreateWeatherIcon(lv_obj_t* parent, int32_t size) {
    lv_obj_t* icon = lv_obj_create(parent);
    lv_obj_remove_flag(icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(icon, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(icon, size, size);
    lv_obj_set_style_bg_opa(icon, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(icon, 0, 0);
    lv_obj_set_style_pad_all(icon, 0, 0);
    lv_obj_set_style_radius(icon, 0, 0);
    return icon;
}

void SetWeatherIcon(lv_obj_t* icon, WeatherIcon kind) {
    if (icon == nullptr) {
        return;
    }
    // user_data holds kind + 1 once something is drawn; 0 means empty.
    uintptr_t drawn = reinterpret_cast<uintptr_t>(lv_obj_get_user_data(icon));
    uintptr_t wanted = static_cast<uintptr_t>(kind) + 1;
    if (drawn == wanted) {
        return;
    }
    lv_obj_set_user_data(icon, reinterpret_cast<void*>(wanted));
    lv_obj_clean(icon);

    Painter p(icon, lv_obj_get_width(icon));
    switch (kind) {
        case WeatherIcon::kSunny:
            p.Sun(24, 24, 10, 17, 2);
            break;
        case WeatherIcon::kPartlyCloudy:
            p.Sun(17, 17, 7, 12, 1);
            p.Cloud(3, 5, kCloud);
            break;
        case WeatherIcon::kCloudy:
            p.Cloud(-4, -6, kDarkCloud);
            p.Cloud(3, 2, kCloud);
            break;
        case WeatherIcon::kFog:
            p.Cloud(0, -9, kCloud);
            p.Rect(8, 32, 32, 3, 1, kDarkCloud);
            p.Rect(12, 38, 28, 3, 1, kDarkCloud);
            p.Rect(8, 44, 32, 3, 1, kDarkCloud);
            break;
        case WeatherIcon::kRain:
            p.Cloud(0, -7, kCloud);
            p.Rect(13, 36, 3, 8, 1, kRain);
            p.Rect(23, 38, 3, 8, 1, kRain);
            p.Rect(33, 36, 3, 8, 1, kRain);
            break;
        case WeatherIcon::kThunder:
            p.Cloud(0, -7, kDarkCloud);
            p.Bolt(21, 30, kSun);
            break;
        case WeatherIcon::kSnow:
            p.Cloud(0, -7, kCloud);
            p.Disc(15, 38, 2, kSnow);
            p.Disc(24, 41, 2, kSnow);
            p.Disc(33, 38, 2, kSnow);
            p.Disc(19, 45, 2, kSnow);
            p.Disc(29, 45, 2, kSnow);
            break;
    }
}
