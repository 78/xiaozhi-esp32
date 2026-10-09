#ifndef SOLAR_TERMS_H
#define SOLAR_TERMS_H

// Return the Chinese solar term (节气) falling on the given Gregorian date,
// e.g. "秋分" on 2026-09-23. Returns an empty string when the date is not a
// term day. Valid for 2000-2099 via the 寿星公式 (C table for the 21st
// century), which covers the device's expected lifetime. The formula is
// accurate to ±1 day; its nine off-by-one cases in this range are corrected
// by an exception table in solar_terms.cc (verified against HKO data).
const char* GetSolarTerm(int year, int month, int day);

#endif  // SOLAR_TERMS_H
