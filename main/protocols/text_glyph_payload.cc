#include "text_glyph_payload.h"
#include "assets.h"

#include <esp_log.h>
#include <mbedtls/base64.h>

#include <cstring>
#include <utility>

#define TAG "TextGlyphPayload"

namespace TextGlyphPayload {

bool Parse(const cJSON* root, std::vector<TextGlyph>& result, uint8_t& bpp) {
    auto payload = cJSON_GetObjectItem(root, "glyph_push");
    if (!cJSON_IsObject(payload)) {
        return true;
    }
    auto version = cJSON_GetObjectItem(payload, "v");
    auto bundle = cJSON_GetObjectItem(payload, "bundle");
    auto size = cJSON_GetObjectItem(payload, "size");
    auto wire_bpp = cJSON_GetObjectItem(payload, "bpp");
    auto glyphs = cJSON_GetObjectItem(payload, "glyphs");
    auto capability = Assets::GetInstance().text_font_capability();
    if (!capability.glyph_push || !cJSON_IsNumber(version) || version->valueint != 1 ||
        !cJSON_IsString(bundle) || capability.bundle != bundle->valuestring ||
        !cJSON_IsNumber(size) || size->valuedouble != capability.size ||
        !cJSON_IsNumber(wire_bpp) || wire_bpp->valuedouble != capability.bpp ||
        !cJSON_IsArray(glyphs) || cJSON_GetArraySize(glyphs) > 64) {
        ESP_LOGW(TAG, "Rejected incompatible glyph payload");
        return false;
    }

    const uint8_t parsed_bpp = static_cast<uint8_t>(wire_bpp->valueint);
    std::vector<TextGlyph> parsed;
    parsed.reserve(cJSON_GetArraySize(glyphs));
    size_t total_bitmap_bytes = 0;
    cJSON* item = nullptr;
    cJSON_ArrayForEach (item, glyphs) {
        auto codepoint = cJSON_GetObjectItem(item, "codepoint");
        auto adv_w = cJSON_GetObjectItem(item, "adv_w");
        auto box_w = cJSON_GetObjectItem(item, "box_w");
        auto box_h = cJSON_GetObjectItem(item, "box_h");
        auto ofs_x = cJSON_GetObjectItem(item, "ofs_x");
        auto ofs_y = cJSON_GetObjectItem(item, "ofs_y");
        auto bitmap = cJSON_GetObjectItem(item, "bitmap");
        if (!cJSON_IsNumber(codepoint) || codepoint->valuedouble <= 0 ||
            codepoint->valuedouble > 0x10FFFF || !cJSON_IsNumber(adv_w) || adv_w->valuedouble < 0 ||
            adv_w->valuedouble > UINT32_MAX || !cJSON_IsNumber(box_w) || box_w->valueint < 0 ||
            box_w->valueint > 64 || !cJSON_IsNumber(box_h) || box_h->valueint < 0 ||
            box_h->valueint > 64 || !cJSON_IsNumber(ofs_x) || ofs_x->valueint < INT16_MIN ||
            ofs_x->valueint > INT16_MAX || !cJSON_IsNumber(ofs_y) || ofs_y->valueint < INT16_MIN ||
            ofs_y->valueint > INT16_MAX || !cJSON_IsString(bitmap)) {
            ESP_LOGW(TAG, "Rejected malformed glyph");
            return false;
        }

        const size_t expected =
            (static_cast<size_t>(box_w->valueint) * box_h->valueint * parsed_bpp + 7) / 8;
        total_bitmap_bytes += expected;
        if (total_bitmap_bytes > 64 * 1024) {
            ESP_LOGW(TAG, "Rejected oversized glyph payload");
            return false;
        }

        TextGlyph glyph;
        glyph.codepoint = static_cast<uint32_t>(codepoint->valuedouble);
        glyph.adv_w = static_cast<uint32_t>(adv_w->valuedouble);
        glyph.box_w = static_cast<uint16_t>(box_w->valueint);
        glyph.box_h = static_cast<uint16_t>(box_h->valueint);
        glyph.ofs_x = static_cast<int16_t>(ofs_x->valueint);
        glyph.ofs_y = static_cast<int16_t>(ofs_y->valueint);
        glyph.bitmap.resize(expected);
        const size_t encoded_length = strlen(bitmap->valuestring);
        if (encoded_length > ((expected + 2) / 3) * 4 + 4) {
            ESP_LOGW(TAG, "Rejected oversized base64 bitmap");
            return false;
        }
        if (expected == 0) {
            if (encoded_length != 0) {
                return false;
            }
            parsed.push_back(std::move(glyph));
            continue;
        }
        size_t decoded = 0;
        int rc = mbedtls_base64_decode(glyph.bitmap.data(), glyph.bitmap.size(), &decoded,
                                       reinterpret_cast<const unsigned char*>(bitmap->valuestring),
                                       encoded_length);
        if (rc != 0 || decoded != expected) {
            ESP_LOGW(TAG, "Rejected invalid bitmap for U+%04lX",
                     static_cast<unsigned long>(glyph.codepoint));
            return false;
        }
        parsed.push_back(std::move(glyph));
    }
    result = std::move(parsed);
    bpp = parsed_bpp;
    return true;
}

}  // namespace TextGlyphPayload
