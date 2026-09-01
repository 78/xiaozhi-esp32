#include "a7682e_tts.h"

#include <algorithm>
#include <cstdint>

namespace {

constexpr uint32_t kTtsBaseDurationMs = 400;
constexpr uint32_t kTtsMsPerCharacter = 180;
constexpr uint32_t kTtsMaxDurationMs = 60000;

size_t Utf8SequenceLength(std::string_view source, size_t offset) {
    if (offset >= source.size()) {
        return 0;
    }

    const uint8_t first = static_cast<uint8_t>(source[offset]);
    if (first <= 0x7F) {
        return 1;
    }

    size_t length = 0;
    if (first >= 0xC2 && first <= 0xDF) {
        length = 2;
    } else if (first >= 0xE0 && first <= 0xEF) {
        length = 3;
    } else if (first >= 0xF0 && first <= 0xF4) {
        length = 4;
    } else {
        return 0;
    }

    if (offset + length > source.size()) {
        return 0;
    }

    const uint8_t second = static_cast<uint8_t>(source[offset + 1]);
    if (second < 0x80 || second > 0xBF) {
        return 0;
    }
    if (first == 0xE0 && second < 0xA0) {
        return 0;
    }
    if (first == 0xED && second > 0x9F) {
        return 0;
    }
    if (first == 0xF0 && second < 0x90) {
        return 0;
    }
    if (first == 0xF4 && second > 0x8F) {
        return 0;
    }

    for (size_t i = 2; i < length; ++i) {
        const uint8_t continuation = static_cast<uint8_t>(source[offset + i]);
        if (continuation < 0x80 || continuation > 0xBF) {
            return 0;
        }
    }
    return length;
}

bool IsAsciiSpace(uint8_t value) {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

bool IsTtsMarkup(uint8_t value) {
    switch (value) {
        case '*':
        case '_':
        case '`':
        case '#':
        case '~':
        case '>':
        case '[':
        case ']':
        case '{':
        case '}':
        case '|':
            return true;
        default:
            return false;
    }
}

uint32_t DecodeUtf8(std::string_view source, size_t offset, size_t length) {
    const uint8_t first = static_cast<uint8_t>(source[offset]);
    if (length == 1) {
        return first;
    }

    uint32_t codepoint = length == 2 ? first & 0x1F : first & 0x0F;
    for (size_t i = 1; i < length; ++i) {
        codepoint = (codepoint << 6) | (static_cast<uint8_t>(source[offset + i]) & 0x3F);
    }
    return codepoint;
}

char HexDigit(uint8_t value) {
    return value < 10 ? static_cast<char>('0' + value) : static_cast<char>('A' + value - 10);
}

bool ContainsNonAscii(std::string_view text) {
    for (const unsigned char value : text) {
        if (value >= 0x80) {
            return true;
        }
    }
    return false;
}

bool IsPreferredBreak(uint32_t codepoint) {
    switch (codepoint) {
        case ' ':
        case ',':
        case '.':
        case '!':
        case '?':
        case ';':
        case ':':
        case ')':
        case ']':
        case '}':
        case 0x3001:  // ideographic comma
        case 0x3002:  // ideographic full stop
        case 0xFF0C:  // full-width comma
        case 0xFF1A:  // full-width colon
        case 0xFF1B:  // full-width semicolon
        case 0xFF01:  // full-width exclamation mark
        case 0xFF1F:  // full-width question mark
            return true;
        default:
            return false;
    }
}

size_t Utf8CharacterCount(std::string_view text) {
    size_t count = 0;
    for (size_t offset = 0; offset < text.size();) {
        const size_t length = Utf8SequenceLength(text, offset);
        if (length == 0) {
            ++offset;
            continue;
        }
        ++count;
        offset += length;
    }
    return count;
}

}  // namespace

namespace a7682e {

std::string TruncateUtf8(std::string_view text, size_t max_bytes) {
    std::string result;
    result.reserve(std::min(text.size(), max_bytes));

    for (size_t offset = 0; offset < text.size();) {
        const size_t length = Utf8SequenceLength(text, offset);
        if (length == 0) {
            ++offset;
            continue;
        }
        if (result.size() + length > max_bytes) {
            break;
        }
        result.append(text.data() + offset, length);
        offset += length;
    }
    return result;
}

std::string SanitizeTtsText(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    bool pending_space = false;

    for (size_t offset = 0; offset < text.size();) {
        const uint8_t first = static_cast<uint8_t>(text[offset]);
        if (first <= 0x7F) {
            ++offset;
            if (IsAsciiSpace(first)) {
                pending_space = true;
                continue;
            }
            if (first < 0x20 || first == 0x7F || IsTtsMarkup(first)) {
                continue;
            }
            if (pending_space && !result.empty()) {
                result += ' ';
            }
            pending_space = false;
            result += static_cast<char>(first);
            continue;
        }

        const size_t length = Utf8SequenceLength(text, offset);
        if (length == 0) {
            ++offset;
            continue;
        }

        // CTTS mode 1 accepts UCS2 only; four-byte symbols are outside that range.
        if (length == 4) {
            offset += length;
            continue;
        }

        if (pending_space && !result.empty()) {
            result += ' ';
        }
        pending_space = false;
        result.append(text.data() + offset, length);
        offset += length;
    }

    while (!result.empty() && result.back() == ' ') {
        result.pop_back();
    }
    return result;
}

std::vector<std::string> SplitTtsText(std::string_view text) {
    const size_t bounded_size = std::min(text.size(), kTtsMaxInputBytes);
    const std::string sanitized = SanitizeTtsText(text.substr(0, bounded_size));
    std::vector<std::string> chunks;
    if (sanitized.empty()) {
        return chunks;
    }

    size_t start = 0;
    while (start < sanitized.size()) {
        while (start < sanitized.size() && IsAsciiSpace(static_cast<uint8_t>(sanitized[start]))) {
            ++start;
        }
        if (start >= sanitized.size()) {
            break;
        }

        size_t offset = start;
        size_t last_fit = start;
        size_t preferred_fit = start;
        while (offset < sanitized.size()) {
            const size_t length = Utf8SequenceLength(sanitized, offset);
            if (length == 0 || offset + length - start > kTtsMaxTextBytes) {
                break;
            }

            const uint32_t codepoint = DecodeUtf8(sanitized, offset, length);
            offset += length;
            last_fit = offset;
            if (IsPreferredBreak(codepoint)) {
                preferred_fit = offset;
            }
        }

        if (last_fit == start) {
            // kTtsMaxTextBytes is larger than every supported UTF-8 sequence,
            // but keep the loop forward-moving if the limit changes later.
            const size_t length = Utf8SequenceLength(sanitized, start);
            if (length == 0) {
                ++start;
                continue;
            }
            last_fit = start + length;
        }

        const size_t cut = preferred_fit > start ? preferred_fit : last_fit;
        std::string chunk = sanitized.substr(start, cut - start);
        while (!chunk.empty() && IsAsciiSpace(static_cast<uint8_t>(chunk.back()))) {
            chunk.pop_back();
        }
        if (!chunk.empty()) {
            chunks.push_back(std::move(chunk));
        }
        start = cut;
    }
    return chunks;
}

bool Utf8ToUcs2Hex(std::string_view text, std::string& output) {
    output.clear();
    output.reserve(text.size() * 2 + 1);

    for (size_t offset = 0; offset < text.size();) {
        const size_t length = Utf8SequenceLength(text, offset);
        if (length == 0 || length == 4) {
            return false;
        }
        const uint32_t codepoint = DecodeUtf8(text, offset, length);
        if (codepoint > 0xFFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
            return false;
        }

        output += HexDigit(static_cast<uint8_t>((codepoint >> 12) & 0x0F));
        output += HexDigit(static_cast<uint8_t>((codepoint >> 8) & 0x0F));
        output += HexDigit(static_cast<uint8_t>((codepoint >> 4) & 0x0F));
        output += HexDigit(static_cast<uint8_t>(codepoint & 0x0F));
        offset += length;
    }
    return !output.empty();
}

bool EscapeAscii(std::string_view text, std::string& output) {
    output.clear();
    output.reserve(text.size() + 1);
    for (const char value : text) {
        if (value == '\r' || value == '\n') {
            output += ' ';
        } else if (value == '"' || value == '\\') {
            output += '\\';
            output += value;
        } else if (static_cast<uint8_t>(value) >= 0x20 && static_cast<uint8_t>(value) < 0x7F) {
            output += value;
        }
    }
    return !output.empty();
}

bool BuildTtsCommand(std::string_view text, std::string& command, int* mode) {
    const std::string sanitized = SanitizeTtsText(text);
    if (sanitized.empty()) {
        command.clear();
        return false;
    }

    const bool use_ucs2 = ContainsNonAscii(sanitized);
    std::string payload;
    if (use_ucs2) {
        if (!Utf8ToUcs2Hex(sanitized, payload)) {
            command.clear();
            return false;
        }
    } else if (!EscapeAscii(sanitized, payload)) {
        command.clear();
        return false;
    }

    command = use_ucs2 ? "AT+CTTS=1,\"" : "AT+CTTS=2,\"";
    command += payload;
    command += "\"";
    if (command.size() >= kTtsMaxCommandBytes) {
        command.clear();
        return false;
    }
    if (mode != nullptr) {
        *mode = use_ucs2 ? 1 : 2;
    }
    return true;
}

uint32_t EstimateTtsDurationMs(std::string_view text) {
    const std::string sanitized = SanitizeTtsText(text);
    const uint64_t character_limit = (kTtsMaxDurationMs - kTtsBaseDurationMs) / kTtsMsPerCharacter;
    const uint64_t characters = std::min<uint64_t>(Utf8CharacterCount(sanitized), character_limit);
    return kTtsBaseDurationMs + static_cast<uint32_t>(characters * kTtsMsPerCharacter);
}

int VolumeToModemGain(int volume) {
    volume = std::clamp(volume, 0, 100);
    return std::clamp((volume * 7 + 50) / 100, 0, 7);
}

}  // namespace a7682e
