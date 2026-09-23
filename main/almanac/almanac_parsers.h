#ifndef ALMANAC_PARSERS_H
#define ALMANAC_PARSERS_H

#include <string>
#include <vector>

struct AlmanacData {
    bool ok = false;
    bool credentials_invalid = false;
    bool quota_exceeded = false;  // Juhe codes 10012/10013: daily limit hit
    std::string lunar_date;  // e.g. "八月十三" (part of yinli after 年)
    std::vector<std::string> yi;
    std::vector<std::string> ji;
};

// Parse the Juhe (聚合数据) 老黄历 response:
// GET https://v.juhe.cn/laohuangli/d?date=YYYY-M-D&key=...
AlmanacData ParseAlmanacResponse(const std::string& body, int http_status);

#endif  // ALMANAC_PARSERS_H
