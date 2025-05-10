#include "lunar_calendar.h"
#include <esp_log.h>

namespace LunarCalendar {

// 农历数据表，从1900年到2100年
// 每个数据的16进制表示形式的最后4位代表闰月，前12位代表每个月的大小月情况（1为大月30天，0为小月29天）
// 例如：0x04baa 表示无闰月，从1月到12月为：0-1-0-0-1-0-1-0-1-0-1-0，即小大小小大小大小大小大小
static const unsigned int LUNAR_INFO[] = {
    0x04bd8, 0x04ae0, 0x0a570, 0x054d5, 0x0d260, 0x0d950, 0x16554, 0x056a0, 0x09ad0, 0x055d2,
    0x04ae0, 0x0a5b6, 0x0a4d0, 0x0d250, 0x1d255, 0x0b540, 0x0d6a0, 0x0ada2, 0x095b0, 0x14977,
    0x04970, 0x0a4b0, 0x0b4b5, 0x06a50, 0x06d40, 0x1ab54, 0x02b60, 0x09570, 0x052f2, 0x04970,
    0x06566, 0x0d4a0, 0x0ea50, 0x06e95, 0x05ad0, 0x02b60, 0x186e3, 0x092e0, 0x1c8d7, 0x0c950,
    0x0d4a0, 0x1d8a6, 0x0b550, 0x056a0, 0x1a5b4, 0x025d0, 0x092d0, 0x0d2b2, 0x0a950, 0x0b557,
    0x06ca0, 0x0b550, 0x15355, 0x04da0, 0x0a5b0, 0x14573, 0x052b0, 0x0a9a8, 0x0e950, 0x06aa0,
    0x0aea6, 0x0ab50, 0x04b60, 0x0aae4, 0x0a570, 0x05260, 0x0f263, 0x0d950, 0x05b57, 0x056a0,
    0x096d0, 0x04dd5, 0x04ad0, 0x0a4d0, 0x0d4d4, 0x0d250, 0x0d558, 0x0b540, 0x0b6a0, 0x195a6,
    0x095b0, 0x049b0, 0x0a974, 0x0a4b0, 0x0b27a, 0x06a50, 0x06d40, 0x0af46, 0x0ab60, 0x09570,
    0x04af5, 0x04970, 0x064b0, 0x074a3, 0x0ea50, 0x06b58, 0x055c0, 0x0ab60, 0x096d5, 0x092e0,
    0x0c960, 0x0d954, 0x0d4a0, 0x0da50, 0x07552, 0x056a0, 0x0abb7, 0x025d0, 0x092d0, 0x0cab5,
    0x0a950, 0x0b4a0, 0x0baa4, 0x0ad50, 0x055d9, 0x04ba0, 0x0a5b0, 0x15176, 0x052b0, 0x0a930,
    0x07954, 0x06aa0, 0x0ad50, 0x05b52, 0x04b60, 0x0a6e6, 0x0a4e0, 0x0d260, 0x0ea65, 0x0d530,
    0x05aa0, 0x076a3, 0x096d0, 0x04bd7, 0x04ad0, 0x0a4d0, 0x1d0b6, 0x0d250, 0x0d520, 0x0dd45,
    0x0b5a0, 0x056d0, 0x055b2, 0x049b0, 0x0a577, 0x0a4b0, 0x0aa50, 0x1b255, 0x06d20, 0x0ada0,
    0x14b63, 0x09370, 0x049f8, 0x04970, 0x064b0, 0x168a6, 0x0ea50, 0x06b20, 0x1a6c4, 0x0aae0,
    0x0a2e0, 0x0d2e3, 0x0c960, 0x0d557, 0x0d4a0, 0x0da50, 0x05d55, 0x056a0, 0x0a6d0, 0x055d4,
    0x052d0, 0x0a9b8, 0x0a950, 0x0b4a0, 0x0b6a6, 0x0ad50, 0x055a0, 0x0aba4, 0x0a5b0, 0x052b0,
    0x0b273, 0x06930, 0x07337, 0x06aa0, 0x0ad50, 0x14b55, 0x04b60, 0x0a570, 0x054e4, 0x0d160,
    0x0e968, 0x0d520, 0x0daa0, 0x16aa6, 0x056d0, 0x04ae0, 0x0a9d4, 0x0a2d0, 0x0d150, 0x0f252
};

// 天干
static const char* GAN[] = {"甲", "乙", "丙", "丁", "戊", "己", "庚", "辛", "壬", "癸"};
// 地支
static const char* ZHI[] = {"子", "丑", "寅", "卯", "辰", "巳", "午", "未", "申", "酉", "戌", "亥"};
// 生肖
static const char* ANIMALS[] = {"鼠", "牛", "虎", "兔", "龙", "蛇", "马", "羊", "猴", "鸡", "狗", "猪"};
// 农历月份
static const char* LUNAR_MONTH[] = {"正", "二", "三", "四", "五", "六", "七", "八", "九", "十", "冬", "腊"};
// 农历日期
static const char* LUNAR_DAY[] = {
    "初一", "初二", "初三", "初四", "初五", "初六", "初七", "初八", "初九", "初十",
    "十一", "十二", "十三", "十四", "十五", "十六", "十七", "十八", "十九", "二十",
    "廿一", "廿二", "廿三", "廿四", "廿五", "廿六", "廿七", "廿八", "廿九", "三十"
};

// 获取某年的农历信息
unsigned int GetLunarInfo(int year) {
    return LUNAR_INFO[year - 1900];
}

// 获取闰月，如果没有闰月返回0
int GetLeapMonth(int year) {
    return GetLunarInfo(year) & 0xf;
}

// 获取闰月的天数，如果没有闰月返回0
int GetLeapDays(int year) {
    if (GetLeapMonth(year)) {
        return (GetLunarInfo(year) & 0x10000) ? 30 : 29;
    }
    return 0;
}

// 获取农历某年某月的天数
int GetMonthDays(int year, int month) {
    return (GetLunarInfo(year) & (0x10000 >> month)) ? 30 : 29;
}

// 获取农历某年的总天数
int GetYearDays(int year) {
    int sum = 348;
    for (int i = 0x8000; i > 0x8; i >>= 1) {
        sum += (GetLunarInfo(year) & i) ? 1 : 0;
    }
    return sum + GetLeapDays(year);
}

// 将公历日期转换为农历日期
void SolarToLunar(int year, int month, int day, int &lunarYear, int &lunarMonth, int &lunarDay, bool &isLeap) {
    // 边界检查
    if (year < 1900 || year > 2100) {
        ESP_LOGE("LunarCalendar", "年份超出范围 [1900-2100]: %d", year);
        lunarYear = 0;
        lunarMonth = 0;
        lunarDay = 0;
        isLeap = false;
        return;
    }

    // 计算距离1900年1月31日的天数
    // 1900年1月31日是农历1900年正月初一
    struct tm t = {};
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = day;
    t.tm_hour = 12; // 中午12点避免时区问题
    time_t solar_time = mktime(&t);
    
    struct tm base_time = {};
    base_time.tm_year = 0; // 1900年
    base_time.tm_mon = 0;  // 1月
    base_time.tm_mday = 31; // 31日
    base_time.tm_hour = 12;
    time_t base = mktime(&base_time);
    
    // 计算相差的天数
    int offset = (solar_time - base) / 86400;
    
    // 计算农历年份
    lunarYear = 1900;
    int days_in_lunar_year = 0;
    while (lunarYear < 2101 && offset > 0) {
        days_in_lunar_year = GetYearDays(lunarYear);
        offset -= days_in_lunar_year;
        lunarYear++;
    }
    
    if (offset < 0) {
        offset += days_in_lunar_year;
        lunarYear--;
    }
    
    // 计算闰月
    int leap_month = GetLeapMonth(lunarYear);
    isLeap = false;
    
    // 计算农历月份
    lunarMonth = 1;
    int days_in_month = 0;
    while (lunarMonth < 13 && offset > 0) {
        // 如果是闰月，需要额外判断
        if (leap_month > 0 && lunarMonth == leap_month && !isLeap) {
            days_in_month = GetLeapDays(lunarYear);
            isLeap = true;
        } else {
            days_in_month = GetMonthDays(lunarYear, lunarMonth);
            isLeap = false;
        }
        
        offset -= days_in_month;
        
        // 如果不是闰月或已经处理了闰月，月份递增
        if (!isLeap || (lunarMonth != leap_month)) {
            lunarMonth++;
        }
    }
    
    // 如果恰好减完，说明是月末
    if (offset == 0) {
        if (leap_month > 0 && lunarMonth == leap_month + 1) {
            isLeap = false;
        }
        if (isLeap && lunarMonth == leap_month) {
            isLeap = true;
        }
    } else {
        offset += days_in_month;
        lunarMonth--;
        if (lunarMonth == 0) {
            lunarMonth = 12;
            lunarYear--;
        }
    }
    
    // 计算农历日期
    lunarDay = offset + 1;
}

// 获取天干地支年份
std::string GetGanZhiYear(int lunarYear) {
    int offset = lunarYear - 1900 + 36; // 1900年是庚子年
    return std::string(GAN[offset % 10]) + std::string(ZHI[offset % 12]);
}

// 获取农历月份字符串
std::string GetLunarMonthString(int lunarMonth, bool isLeap) {
    std::string result;
    if (isLeap) {
        result = "闰";
    }
    
    if (lunarMonth > 0 && lunarMonth <= 12) {
        result += LUNAR_MONTH[lunarMonth - 1];
    } else {
        result = "未知";
    }
    
    result += "月";
    return result;
}

// 获取农历日期字符串
std::string GetLunarDayString(int lunarDay) {
    if (lunarDay > 0 && lunarDay <= 30) {
        return LUNAR_DAY[lunarDay - 1];
    }
    return "未知";
}

// 获取生肖
std::string GetZodiac(int lunarYear) {
    int offset = (lunarYear - 1900) % 12;
    return ANIMALS[offset];
}

// 主要接口：根据公历日期获取农历日期字符串
std::string GetLunarDate(int year, int month, int day) {
    int lunarYear, lunarMonth, lunarDay;
    bool isLeap;
    
    // 转换为农历日期
    SolarToLunar(year, month, day, lunarYear, lunarMonth, lunarDay, isLeap);
    
    if (lunarYear == 0) {
        return "农历日期错误";
    }
    
    // 构建农历日期字符串
    std::string lunarDateStr = "农历";
    lunarDateStr += GetGanZhiYear(lunarYear);
    lunarDateStr += "年";
    lunarDateStr += GetLunarMonthString(lunarMonth, isLeap);
    lunarDateStr += GetLunarDayString(lunarDay);
    
    return lunarDateStr;
}

} // namespace LunarCalendar 