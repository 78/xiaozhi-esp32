#ifndef RUNTIME_I18N_H_
#define RUNTIME_I18N_H_

#include <string_view>
#if __has_include("sdkconfig.h")
#include "sdkconfig.h"
#endif

#include "assets/lang_config.h"
#if CONFIG_RUNTIME_LOCALE
#include "board.h"
#endif

namespace I18n {
inline const char* Text(const char* text) {
#if CONFIG_RUNTIME_LOCALE
    return Board::GetInstance().LocalizeText(text);
#else
    return text;
#endif
}

inline const char* LanguageCode() {
#if CONFIG_RUNTIME_LOCALE
    if (const char* code = Board::GetInstance().GetRuntimeLanguage())
        return code;
#endif
    return Lang::CODE;
}

inline std::string_view Sound(std::string_view sound) {
#if CONFIG_RUNTIME_LOCALE
    return Board::GetInstance().LocalizeSound(sound);
#else
    return sound;
#endif
}
}  // namespace I18n
#endif
