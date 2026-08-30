#include "morse_code.h"

#include <cctype>
#include <sstream>
#include <unordered_map>

#include <esp_log.h>

#define TAG "MorseCode"

namespace {

const std::unordered_map<char, std::string>& MorseTable() {
    static const std::unordered_map<char, std::string> table = {
        {'A', ".-"},    {'B', "-..."},  {'C', "-.-."}, {'D', "-.."},
        {'E', "."},     {'F', "..-."},  {'G', "--."},  {'H', "...."},
        {'I', ".."},    {'J', ".---"},  {'K', "-.-"},  {'L', ".-.."},
        {'M', "--"},    {'N', "-."},    {'O', "---"},  {'P', ".--."},
        {'Q', "--.-"},  {'R', ".-."},   {'S', "..."},  {'T', "-"},
        {'U', "..-"},   {'V', "...-"},  {'W', ".--"},  {'X', "-..-"},
        {'Y', "-.--"},  {'Z', "--.."},
        {'0', "-----"}, {'1', ".----"}, {'2', "..---"}, {'3', "...--"},
        {'4', "....-"}, {'5', "....."}, {'6', "-...."}, {'7', "--..."},
        {'8', "---.."}, {'9', "----."},
        {'.', ".-.-.-"}, {',', "--..--"}, {'?', "..--.."}, {'\'', ".----."},
        {'!', "-.-.--"}, {'/', "-..-."},  {'(', "-.--."},  {')', "-.--.-"},
        {'&', ".-..."},  {':', "---..."}, {';', "-.-.-."}, {'=', "-...-"},
        {'+', ".-.-."},  {'-', "-....-"}, {'_', "..--.-"}, {'"', ".-..-."},
        {'$', "...-..-"}, {'@', ".--.-."},
    };
    return table;
}

std::vector<std::string> SplitWords(const std::string& text) {
    std::vector<std::string> words;
    std::istringstream iss(text);
    std::string word;
    while (iss >> word) {
        words.push_back(word);
    }
    return words;
}

}  // namespace

std::vector<bool> EncodeMorseTicks(const std::string& text) {
    const auto& table = MorseTable();
    std::vector<bool> ticks;

    auto words = SplitWords(text);
    for (size_t w = 0; w < words.size(); ++w) {
        const auto& word = words[w];
        for (size_t c = 0; c < word.size(); ++c) {
            char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(word[c])));
            auto it = table.find(ch);
            if (it == table.end()) {
                ESP_LOGW(TAG, "Skipping unsupported character: '%c'", ch);
                continue;
            }
            const auto& pattern = it->second;
            for (size_t i = 0; i < pattern.size(); ++i) {
                int on_ticks = (pattern[i] == '.') ? 1 : 3;
                ticks.insert(ticks.end(), on_ticks, true);
                if (i + 1 < pattern.size()) {
                    ticks.push_back(false);  // intra-character gap
                }
            }
            if (c + 1 < word.size()) {
                ticks.insert(ticks.end(), 3, false);  // inter-character gap
            }
        }
        if (w + 1 < words.size()) {
            ticks.insert(ticks.end(), 7, false);  // inter-word gap
        }
    }
    return ticks;
}
