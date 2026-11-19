#ifndef PETOI_RUNTIME_LOCALE_POLICY_H_
#define PETOI_RUNTIME_LOCALE_POLICY_H_

#include <string_view>

namespace petoi {
// The country service has a flat object schema. Reject nesting before invoking
// cJSON so a small but deeply nested response cannot exhaust the worker stack.
inline bool IsFlatCountryObject(std::string_view body) {
    const auto first = body.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos || body[first] != '{' ||
        body.find('\0') != std::string_view::npos || body.find("\\u0000") != std::string_view::npos)
        return false;
    bool quoted = false, escaped = false;
    int depth = 0;
    for (const unsigned char c : body) {
        if (quoted) {
            if (escaped)
                escaped = false;
            else if (c == '\\')
                escaped = true;
            else if (c == '"')
                quoted = false;
            else if (c < 32)
                return false;
        } else if (c == '"') {
            quoted = true;
        } else if (c == '{') {
            if (++depth > 1)
                return false;
        } else if (c == '}') {
            if (--depth < 0)
                return false;
        } else if (c == '[' || c == ']') {
            return false;
        }
    }
    return !quoted && !escaped && depth == 0;
}

enum class ProductLocale { Chinese, English };

inline const char* LocaleCode(ProductLocale locale) {
    return locale == ProductLocale::Chinese ? "zh-CN" : "en-US";
}

inline bool IsSupportedLocale(std::string_view code) { return code == "zh-CN" || code == "en-US"; }

inline ProductLocale CachedLocale(std::string_view code, ProductLocale fallback) {
    if (code == "zh-CN")
        return ProductLocale::Chinese;
    if (code == "en-US")
        return ProductLocale::English;
    return fallback;
}

inline bool NeedsCountryLookup(std::string_view code, std::string_view cached_ssid,
                               std::string_view connected_ssid) {
    // SSIDs are opaque bytes. Never normalize them or send them to GeoIP.
    return !connected_ssid.empty() && connected_ssid.size() <= 32 &&
           (!IsSupportedLocale(code) || cached_ssid != connected_ssid);
}

struct LocaleDecision {
    ProductLocale locale;
    bool persist = false;
};

inline LocaleDecision DecideLocale(std::string_view code, std::string_view cached_ssid,
                                   std::string_view connected_ssid,
                                   std::string_view valid_country_code, ProductLocale fallback) {
    LocaleDecision decision{CachedLocale(code, fallback), false};
    if (!NeedsCountryLookup(code, cached_ssid, connected_ssid))
        return decision;
    // Empty, malformed, failed or truncated HTTP/JSON responses must not alter
    // the language or mark this SSID as successfully resolved.
    if (valid_country_code.size() != 2 || valid_country_code[0] < 'A' ||
        valid_country_code[0] > 'Z' || valid_country_code[1] < 'A' || valid_country_code[1] > 'Z')
        return decision;
    decision.locale = valid_country_code == "CN" ? ProductLocale::Chinese : ProductLocale::English;
    decision.persist = true;
    return decision;
}
}  // namespace petoi
#endif
