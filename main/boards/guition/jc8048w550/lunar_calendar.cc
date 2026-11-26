#include "lunar_calendar.h"

#include <cmath>

// The algorithm is Ho Ngoc Duc's (https://www.informatik.uni-leipzig.de/~duc/amlich/),
// which follows the Vietnamese lunar calendar (time zone UTC+7).
namespace lunar {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTimeZone = 7.0;

int Floor(double value) { return static_cast<int>(std::floor(value)); }

// Julian day number of the k-th new moon after 1900-01-01 (fractional).
double NewMoon(int k) {
    const double t = k / 1236.85;
    const double t2 = t * t;
    const double t3 = t2 * t;
    const double dr = kPi / 180;
    double jd1 = 2415020.75933 + 29.53058868 * k + 0.0001178 * t2 - 0.000000155 * t3;
    jd1 += 0.00033 * std::sin((166.56 + 132.87 * t - 0.009173 * t2) * dr);
    const double m = 359.2242 + 29.10535608 * k - 0.0000333 * t2 - 0.00000347 * t3;
    const double mpr = 306.0253 + 385.81691806 * k + 0.0107306 * t2 + 0.00001236 * t3;
    const double f = 21.2964 + 390.67050646 * k - 0.0016528 * t2 - 0.00000239 * t3;
    double c1 = (0.1734 - 0.000393 * t) * std::sin(m * dr) + 0.0021 * std::sin(2 * dr * m);
    c1 -= 0.4068 * std::sin(mpr * dr) - 0.0161 * std::sin(dr * 2 * mpr);
    c1 -= 0.0004 * std::sin(dr * 3 * mpr);
    c1 += 0.0104 * std::sin(dr * 2 * f) - 0.0051 * std::sin(dr * (m + mpr));
    c1 -= 0.0074 * std::sin(dr * (m - mpr)) - 0.0004 * std::sin(dr * (2 * f + m));
    c1 -= 0.0004 * std::sin(dr * (2 * f - m)) + 0.0006 * std::sin(dr * (2 * f + mpr));
    c1 += 0.0010 * std::sin(dr * (2 * f - mpr)) + 0.0005 * std::sin(dr * (2 * mpr + m));
    const double delta_t =
        t < -11 ? 0.001 + 0.000839 * t + 0.0002261 * t2 - 0.00000845 * t3 - 0.000000081 * t * t3
                : -0.000278 + 0.000265 * t + 0.000262 * t2;
    return jd1 + c1 - delta_t;
}

// Sun's ecliptic longitude in radians [0, 2*pi) at the given Julian day.
double SunLongitude(double jdn) {
    const double t = (jdn - 2451545.0) / 36525;
    const double t2 = t * t;
    const double dr = kPi / 180;
    const double m = 357.52910 + 35999.05030 * t - 0.0001559 * t2 - 0.00000048 * t * t2;
    const double l0 = 280.46645 + 36000.76983 * t + 0.0003032 * t2;
    double dl = (1.914600 - 0.004817 * t - 0.000014 * t2) * std::sin(dr * m);
    dl += (0.019993 - 0.000101 * t) * std::sin(dr * 2 * m) + 0.000290 * std::sin(dr * 3 * m);
    double l = (l0 + dl) * dr;
    l -= kPi * 2 * Floor(l / (kPi * 2));
    return l;
}

// Which 30-degree sector (0..11) the sun is in at local midnight of day_number.
int SunSector(int day_number) {
    return Floor(SunLongitude(day_number - 0.5 - kTimeZone / 24.0) / kPi * 6);
}

int NewMoonDay(int k) { return Floor(NewMoon(k) + 0.5 + kTimeZone / 24.0); }

// Day number of the start of lunar month 11 of the given solar year.
int LunarMonth11(int year) {
    const int off = JulianDay(31, 12, year) - 2415021;
    const int k = Floor(off / 29.530588853);
    int nm = NewMoonDay(k);
    if (SunSector(nm) >= 9) {
        nm = NewMoonDay(k - 1);
    }
    return nm;
}

// Offset (in months after month 11) of the leap month in a 13-month year.
int LeapMonthOffset(int a11) {
    const int k = Floor((a11 - 2415021.076998695) / 29.530588853 + 0.5);
    int i = 1;
    int arc = SunSector(NewMoonDay(k + i));
    int last;
    do {
        last = arc;
        ++i;
        arc = SunSector(NewMoonDay(k + i));
    } while (arc != last && i < 14);
    return i - 1;
}

const char* const kCan[10] = {"Giáp", "Ất",   "Bính", "Đinh", "Mậu",
                              "Kỷ",   "Canh", "Tân",  "Nhâm", "Quý"};
const char* const kChi[12] = {"Tý",  "Sửu", "Dần",  "Mão", "Thìn", "Tỵ",
                              "Ngọ", "Mùi", "Thân", "Dậu", "Tuất", "Hợi"};
// Indexed by floor(sun longitude / 15 degrees), starting at the March equinox.
const char* const kTerms[24] = {"Xuân phân",  "Thanh minh", "Cốc vũ",    "Lập hạ",   "Tiểu mãn",
                                "Mang chủng", "Hạ chí",     "Tiểu thử",  "Đại thử",  "Lập thu",
                                "Xử thử",     "Bạch lộ",    "Thu phân",  "Hàn lộ",   "Sương giáng",
                                "Lập đông",   "Tiểu tuyết", "Đại tuyết", "Đông chí", "Tiểu hàn",
                                "Đại hàn",    "Lập xuân",   "Vũ thủy",   "Kinh trập"};

int Mod(int value, int divisor) { return ((value % divisor) + divisor) % divisor; }

std::string CanChi(int can, int chi) {
    return std::string(kCan[Mod(can, 10)]) + " " + kChi[Mod(chi, 12)];
}

}  // namespace

int JulianDay(int day, int month, int year) {
    const int a = (14 - month) / 12;
    const int y = year + 4800 - a;
    const int m = month + 12 * a - 3;
    int jd = day + (153 * m + 2) / 5 + 365 * y + y / 4 - y / 100 + y / 400 - 32045;
    if (jd < 2299161) {
        jd = day + (153 * m + 2) / 5 + 365 * y + y / 4 - 32083;
    }
    return jd;
}

LunarDate SolarToLunar(int day, int month, int year) {
    const int day_number = JulianDay(day, month, year);
    const int k = Floor((day_number - 2415021.076998695) / 29.530588853);
    int month_start = NewMoonDay(k + 1);
    if (month_start > day_number) {
        month_start = NewMoonDay(k);
    }
    int a11 = LunarMonth11(year);
    int b11 = a11;
    int lunar_year;
    if (a11 >= month_start) {
        lunar_year = year;
        a11 = LunarMonth11(year - 1);
    } else {
        lunar_year = year + 1;
        b11 = LunarMonth11(year + 1);
    }
    LunarDate result;
    result.day = day_number - month_start + 1;
    const int diff = (month_start - a11) / 29;
    result.month = diff + 11;
    if (b11 - a11 > 365) {
        const int leap_offset = LeapMonthOffset(a11);
        if (diff >= leap_offset) {
            result.month = diff + 10;
            result.leap = diff == leap_offset;
        }
    }
    if (result.month > 12) {
        result.month -= 12;
    }
    if (result.month >= 11 && diff < 4) {
        lunar_year -= 1;
    }
    result.year = lunar_year;
    return result;
}

std::string YearCanChi(int lunar_year) { return CanChi(lunar_year + 6, lunar_year + 8); }

std::string MonthCanChi(const LunarDate& date) {
    return CanChi(date.year * 12 + date.month + 3, date.month + 1);
}

std::string DayCanChi(int jdn) { return CanChi(jdn + 9, jdn + 1); }

const char* SolarTerm(int day, int month, int year) {
    // Longitude at the end of the day, so the day a term begins is named after it.
    const int jd = JulianDay(day, month, year);
    const double longitude = SunLongitude(jd + 1 - 0.5 - kTimeZone / 24.0);
    return kTerms[Mod(Floor(longitude / kPi * 12), 24)];
}

}  // namespace lunar
