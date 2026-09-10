#include "text_glyph.h"

bool TextGlyphStorageUsesPsram() { return heap_caps_get_total_size(MALLOC_CAP_SPIRAM) > 0; }
