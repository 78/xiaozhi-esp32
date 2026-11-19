#ifndef PETOI_ROBOT_LANGUAGE_H_
#define PETOI_ROBOT_LANGUAGE_H_
#include <algorithm>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>
#include "robot_plan.h"
namespace petoi {
// Ported from the stable product's action vocabulary and per-clause numbers.
inline bool ContainsAny(const std::string& text, std::initializer_list<const char*> words) {
    for (const char* word : words) {
        if (text.find(word) != std::string::npos) {
            return true;
        }
    }
    return false;
}

inline std::string ToLowerAscii(std::string text) {
    for (char& ch : text) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return text;
}

inline int ExtractFirstNumber(const std::string& text, int default_value) {
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] >= '0' && text[i] <= '9') {
            int value = 0;
            while (i < text.size() && text[i] >= '0' && text[i] <= '9') {
                value = value * 10 + (text[i] - '0');
                ++i;
            }
            return value;
        }
    }

    struct WordNumber {
        const char* text;
        int value;
    };
    static constexpr WordNumber numbers[] = {
        {"ten", 10},  {"nine", 9}, {"eight", 8}, {"seven", 7}, {"six", 6}, {"five", 5}, {"four", 4},
        {"three", 3}, {"two", 2},  {"one", 1},   {"十", 10},   {"九", 9},  {"八", 8},   {"七", 7},
        {"六", 6},    {"五", 5},   {"四", 4},    {"三", 3},    {"两", 2},  {"二", 2},   {"一", 1}};
    const std::string lower_text = ToLowerAscii(text);
    for (const auto& number : numbers) {
        if (lower_text.find(number.text) != std::string::npos) {
            return number.value;
        }
    }
    return default_value;
}

inline int ExtractActionNumber(const std::string& text, size_t pos, int default_value) {
    if (pos >= text.size()) {
        return default_value;
    }

    std::string action_clause = text.substr(pos);
    const std::string lower_clause = ToLowerAscii(action_clause);
    size_t clause_end = std::string::npos;
    const auto keep_earliest_boundary = [&clause_end](size_t boundary) {
        if (boundary != std::string::npos &&
            (clause_end == std::string::npos || boundary < clause_end)) {
            clause_end = boundary;
        }
    };

    // Only read a quantity from the current action clause. Without this
    // boundary, "向前走三步，然后左转30度" sees the later Arabic "30"
    // first and incorrectly turns the walking command into "kwkF 30".
    for (const char* boundary :
         {"然后", "接着", "随后", "之后", "并且", "最后", "再向", "再往", "再左", "再右",
          "再打", "再做", "再站", "再坐", "再伸", "再点", "再摇", "再退", "再前", "再后",
          "再起", "再趴", "，",   "。",   "；",   "、",   ",",    ".",    ";"}) {
        keep_earliest_boundary(action_clause.find(boundary));
    }
    for (const char* boundary :
         {" and then ", " then ", " followed by ", " after that ", " next ", " and "}) {
        keep_earliest_boundary(lower_clause.find(boundary));
    }
    if (clause_end != std::string::npos) {
        action_clause.resize(clause_end);
    }

    return ExtractFirstNumber(action_clause, default_value);
}

inline size_t FindFirstPhrasePos(const std::string& text,
                                 std::initializer_list<const char*> phrases) {
    size_t best = std::string::npos;
    const auto word = [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
               c == '_';
    };
    for (const char* phrase : phrases) {
        const size_t length = std::char_traits<char>::length(phrase);
        auto pos = text.find(phrase);
        while (pos != std::string::npos) {
            const bool left = word(phrase[0]) && pos > 0 && word(text[pos - 1]);
            const bool right =
                word(phrase[length - 1]) && pos + length < text.size() && word(text[pos + length]);
            if (!left && !right) {
                best = std::min(best, pos);
                break;
            }
            pos = text.find(phrase, pos + length);
        }
    }
    return best;
}

inline void AddRobotActionMatch(std::vector<std::pair<size_t, std::string>>& matches, size_t pos,
                                const std::string& command) {
    if (pos == std::string::npos || command.empty()) {
        return;
    }

    matches.emplace_back(pos, command);
}

inline bool BuildRawActionText(const std::string& text, std::string& command) {
    command.clear();
    const std::string lower_text = ToLowerAscii(text);

    if (ContainsAny(text, {"天气", "故事", "笑话", "介绍", "你会", "什么", "怎么样", "在吗"}) ||
        ContainsAny(lower_text,
                    {"weather", "story", "joke", "introduce", "what can you", "how are you"})) {
        return false;
    }

    std::vector<std::pair<size_t, std::string>> matches;
    AddRobotActionMatch(matches, FindFirstPhrasePos(text, {"再见", "拜拜", "休息", "趴下", "待机"}),
                        "d");
    AddRobotActionMatch(
        matches, FindFirstPhrasePos(lower_text, {"goodbye", "bye", "rest", "lie down", "lay down"}),
        "d");
    AddRobotActionMatch(
        matches, FindFirstPhrasePos(text, {"打招呼", "打个招呼", "打一个招呼", "招手", "你好"}),
        "khi");
    AddRobotActionMatch(matches, FindFirstPhrasePos(lower_text, {"say hi", "wave", "greet"}),
                        "khi");
    AddRobotActionMatch(matches, FindFirstPhrasePos(text, {"坐下", "坐下来", "坐好"}), "ksit");
    AddRobotActionMatch(matches, FindFirstPhrasePos(lower_text, {"sit down", "sit"}), "ksit");
    AddRobotActionMatch(matches, FindFirstPhrasePos(text, {"站起来", "起立", "起来"}), "kup");
    AddRobotActionMatch(matches, FindFirstPhrasePos(lower_text, {"stand up", "stand", "sand up"}),
                        "kup");
    AddRobotActionMatch(matches, FindFirstPhrasePos(text, {"前空翻", "前空", "起空翻", "前滚翻"}),
                        "kff");
    AddRobotActionMatch(matches, FindFirstPhrasePos(lower_text, {"front flip", "forward flip"}),
                        "kff");
    AddRobotActionMatch(matches, FindFirstPhrasePos(text, {"后空翻", "后空", "后孔", "后滚翻"}),
                        "kbf");
    AddRobotActionMatch(matches, FindFirstPhrasePos(lower_text, {"back flip", "backflip"}), "kbf");
    AddRobotActionMatch(matches, FindFirstPhrasePos(text, {"俯卧撑", "俯卧撑动作", "卧撑"}), "kpu");
    AddRobotActionMatch(matches, FindFirstPhrasePos(lower_text, {"push up", "push-up", "pushup"}),
                        "kpu");
    AddRobotActionMatch(matches,
                        FindFirstPhrasePos(text, {"装死", "假死", "装死动作", "装饰", "装实",
                                                  "装饰动作", "做个装饰"}),
                        "kpd");
    AddRobotActionMatch(matches, FindFirstPhrasePos(lower_text, {"play dead", "pretend dead"}),
                        "kpd");
    AddRobotActionMatch(matches, FindFirstPhrasePos(text, {"大风车", "风车"}), "kpinWheel");
    AddRobotActionMatch(matches, FindFirstPhrasePos(lower_text, {"pinwheel"}), "kpinWheel");
    AddRobotActionMatch(matches, FindFirstPhrasePos(text, {"握手"}), "khsk");
    AddRobotActionMatch(matches, FindFirstPhrasePos(lower_text, {"shake hand", "handshake"}),
                        "khsk");
    AddRobotActionMatch(
        matches,
        FindFirstPhrasePos(text, {"伸懒腰", "伸个懒腰", "伸一下懒腰", "伸一伸懒腰", "伸伸懒腰"}),
        "kstr");
    AddRobotActionMatch(matches, FindFirstPhrasePos(lower_text, {"stretch"}), "kstr");
    AddRobotActionMatch(matches, FindFirstPhrasePos(text, {"点头"}), "knd");
    AddRobotActionMatch(matches, FindFirstPhrasePos(lower_text, {"nod"}), "knd");
    AddRobotActionMatch(matches, FindFirstPhrasePos(text, {"摇头", "歪头"}), "kwh");
    AddRobotActionMatch(matches, FindFirstPhrasePos(lower_text, {"shake head"}), "kwh");

    size_t pos = FindFirstPhrasePos(text, {"向左转", "左转"});
    if (pos == std::string::npos) {
        pos = FindFirstPhrasePos(lower_text,
                                 {"turn left", "left turn", "rotate left", "to the left"});
    }
    AddRobotActionMatch(matches, pos, "kvtL " + std::to_string(ExtractActionNumber(text, pos, 90)));

    pos = FindFirstPhrasePos(text, {"向右转", "右转"});
    if (pos == std::string::npos) {
        pos = FindFirstPhrasePos(lower_text,
                                 {"turn right", "right turn", "rotate right", "to the right"});
    }
    AddRobotActionMatch(matches, pos, "kvtR " + std::to_string(ExtractActionNumber(text, pos, 90)));

    pos = FindFirstPhrasePos(text, {"向前走", "往前走", "下前走", "下去走", "向前", "往前", "前走",
                                    "前进", "走五步", "走几步"});
    if (pos == std::string::npos) {
        pos = FindFirstPhrasePos(
            lower_text, {"go forward", "move forward", "walk forward", "step forward",
                         "steps forward", "step ahead", "go ahead", "move ahead", "walk ahead"});
    }
    AddRobotActionMatch(matches, pos, "kwkF " + std::to_string(ExtractActionNumber(text, pos, 5)));

    pos = FindFirstPhrasePos(text, {"向后退", "往后退", "后退", "退后"});
    if (pos == std::string::npos) {
        pos = FindFirstPhrasePos(lower_text, {"go back", "move back", "walk back", "step back",
                                              "go backward", "move backward", "walk backward",
                                              "step backward", "steps backward", "backward"});
    }
    AddRobotActionMatch(matches, pos, "kbk " + std::to_string(ExtractActionNumber(text, pos, 5)));

    if (matches.empty()) {
        return false;
    }

    std::sort(matches.begin(), matches.end(),
              [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });
    for (const auto& match : matches) {
        if (!command.empty()) {
            command += ",";
        }
        command += match.second;
    }

    return true;
}
inline bool BuildLocalActionText(const std::string& text, std::string& commands) {
    commands.clear();
    std::string candidate;
    if (text.empty() || text.size() > 512)
        return false;
    const auto lower = ToLowerAscii(text);
    if (ContainsAny(lower, {"不要",     "不用",     "不许",     "别",      "不能",    "不是",
                            "不想",     "如果",     "假如",     "为什么",  "解释",    "教我",
                            "会不会",   "don't",    "do not",   "never",   "not ",    "if ",
                            "why ",     "explain ", "how ",     "怎么",    "如何",    "eleven",
                            "twelve",   "thirteen", "fourteen", "fifteen", "sixteen", "seventeen",
                            "eighteen", "nineteen", "twenty",   "thirty",  "forty",   "fifty",
                            "sixty",    "seventy",  "eighty",   "ninety",  "hundred", "thousand"}))
        return false;
    for (const char* first : {"一", "二", "三", "四", "五", "六", "七", "八", "九", "十", "两"})
        for (const char* second :
             {"一", "二", "三", "四", "五", "六", "七", "八", "九", "十", "百", "千", "万"})
            if (text.find(std::string(first) + second) != std::string::npos)
                return false;
    for (size_t i = 0; i + 1 < text.size(); ++i)
        if ((text[i] == '-' || text[i] == '.') && text[i + 1] >= '0' && text[i + 1] <= '9')
            return false;
    unsigned value = 0;
    for (char c : lower) {
        if (c >= '0' && c <= '9') {
            value = value * 10 + (c - '0');
            if (value > 120000)
                return false;
        } else
            value = 0;
    }
    size_t begin = 0;
    while (begin < text.size()) {
        size_t end = std::string::npos, skip = 0;
        for (const char* separator : {"然后", "接着", "随后", "之后", "，", ",", "；", ";",
                                      " and then ", " then ", " followed by "}) {
            const auto pos = lower.find(separator, begin);
            if (pos != std::string::npos && (end == std::string::npos || pos < end)) {
                end = pos;
                skip = std::char_traits<char>::length(separator);
            }
        }
        auto again = lower.find("再", begin);
        while (again != std::string::npos && lower.compare(again, 6, "再见") == 0)
            again = lower.find("再", again + 3);
        if (again != std::string::npos && again > begin &&
            (end == std::string::npos || again < end)) {
            end = again;
            skip = 3;
        }
        if (end == begin) {
            begin += skip;
            continue;
        }
        std::string part;
        if (!BuildRawActionText(text.substr(begin, end == std::string::npos ? end : end - begin),
                                part))
            return false;
        if (!candidate.empty())
            candidate += ',';
        candidate += part;
        if (end == std::string::npos)
            break;
        begin = end + skip;
    }
    RobotPlan plan;
    if (!BuildRobotPlan(candidate, plan))
        return false;
    commands = std::move(candidate);
    return true;
}
}  // namespace petoi
#endif
