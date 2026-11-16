#ifndef REALTIME_AUDIO_BASE64_H
#define REALTIME_AUDIO_BASE64_H

#include <stddef.h>
#include <stdint.h>

namespace realtime_audio {
namespace detail {
struct Base64Table {
    uint8_t values[256]{};
    constexpr Base64Table() {
        for (auto& value : values) {
            value = 0xff;
        }
        for (unsigned i = 0; i < 26; ++i) {
            values['A' + i] = i;
            values['a' + i] = i + 26;
        }
        for (unsigned i = 0; i < 10; ++i) {
            values['0' + i] = i + 52;
        }
        values['+'] = 62;
        values['/'] = 63;
    }
};
inline constexpr Base64Table kBase64Table{};
}  // namespace detail

// Fast path for ordinary, unbroken Base64 audio. Returning false asks the
// caller to use the general decoder (including its whitespace validation).
// This lookup table is for audio payloads, never cryptographic key material.
inline bool DecodeBase64(const char* input, size_t length, uint8_t* output, size_t capacity,
                         size_t& written) {
    written = 0;
    if (length == 0) {
        return true;
    }
    if (input == nullptr || output == nullptr || length % 4 != 0) {
        return false;
    }
    const size_t padding = input[length - 1] == '=' ? (input[length - 2] == '=' ? 2 : 1) : 0;
    const size_t size = length / 4 * 3 - padding;
    if (capacity < size) {
        return false;
    }
    const auto& table = detail::kBase64Table.values;
    size_t offset = 0;
    for (size_t i = 0; i < length; i += 4) {
        const bool last = i + 4 == length;
        const unsigned a = table[static_cast<uint8_t>(input[i])];
        const unsigned b = table[static_cast<uint8_t>(input[i + 1])];
        const unsigned c = last && padding == 2 ? 0 : table[static_cast<uint8_t>(input[i + 2])];
        const unsigned d = last && padding != 0 ? 0 : table[static_cast<uint8_t>(input[i + 3])];
        if ((a | b | c | d) & 0x80) {
            return false;
        }
        output[offset++] = static_cast<uint8_t>((a << 2) | (b >> 4));
        if (!last || padding < 2) {
            output[offset++] = static_cast<uint8_t>((b << 4) | (c >> 2));
        }
        if (!last || padding == 0) {
            output[offset++] = static_cast<uint8_t>((c << 6) | d);
        }
    }
    written = offset;
    return true;
}
}  // namespace realtime_audio

#endif
