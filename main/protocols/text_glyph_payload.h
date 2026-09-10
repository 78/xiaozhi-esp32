#pragma once

#include "display/text_glyph.h"

#include <cJSON.h>

#include <cstdint>
#include <vector>

namespace TextGlyphPayload {

bool Parse(const cJSON* root, std::vector<TextGlyph>& result, uint8_t& bpp);

}  // namespace TextGlyphPayload
