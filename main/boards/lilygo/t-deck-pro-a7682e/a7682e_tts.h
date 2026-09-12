#ifndef _T_DECK_PRO_A7682E_TTS_H_
#define _T_DECK_PRO_A7682E_TTS_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <string_view>
#include <vector>

namespace a7682e {

constexpr size_t kTtsMaxTextBytes = 160;
constexpr size_t kTtsMaxCommandBytes = 768;
constexpr size_t kTtsQueueCapacity = 4;
constexpr size_t kTtsMaxInputBytes = kTtsMaxTextBytes * kTtsQueueCapacity;

std::string SanitizeTtsText(std::string_view text);
std::string TruncateUtf8(std::string_view text, size_t max_bytes);
std::vector<std::string> SplitTtsText(std::string_view text);
bool Utf8ToUcs2Hex(std::string_view text, std::string& output);
bool EscapeAscii(std::string_view text, std::string& output);
bool BuildTtsCommand(std::string_view text, std::string& command, int* mode = nullptr);
uint32_t EstimateTtsDurationMs(std::string_view text);
int VolumeToModemGain(int volume);

}  // namespace a7682e

#endif  // _T_DECK_PRO_A7682E_TTS_H_
