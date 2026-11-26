#ifndef JC8048W550_WEATHER_ICON_H
#define JC8048W550_WEATHER_ICON_H

#include "dashboard_model.h"

#include <lvgl.h>

// Weather icons drawn from LVGL shapes: the built-in icon font has no weather
// glyphs. The icon is a transparent square object; call both functions with
// the display lock held.
lv_obj_t* CreateWeatherIcon(lv_obj_t* parent, int32_t size);
// Redraws only when `kind` differs from what the icon already shows.
void SetWeatherIcon(lv_obj_t* icon, dashboard::WeatherIcon kind);

#endif  // JC8048W550_WEATHER_ICON_H
