/*
 * @Date: 2025-04-04 22:19:19
 * @LastEditors: zhouke
 * @LastEditTime: 2025-04-04 22:23:35
 * @FilePath: \xiaozhi-esp32\main\boards\abrobot-1.28tft-wifi\lunar_calendar.h
 */
#pragma once

#include <string>
#include <ctime>

namespace LunarCalendar {
    // 根据公历日期获取农历日期字符串
    std::string GetLunarDate(int year, int month, int day);
} 