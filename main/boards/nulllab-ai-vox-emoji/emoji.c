#include "emoji.h"
#include "lvgl.h"

extern const lv_image_dsc_t emoji_neutral; // neutral
extern const lv_image_dsc_t emoji_happy;   // happy
// extern const lv_image_dsc_t emoji_1f606;       // laughing
// extern const lv_image_dsc_t emoji_1f602;       // funny
// extern const lv_image_dsc_t emoji_1f614;       // sad
extern const lv_image_dsc_t emoji_angry;  // angry
extern const lv_image_dsc_t emoji_crying; // crying
// extern const lv_image_dsc_t emoji_1f60d;       // loving
extern const lv_image_dsc_t emoji_embarrassed; // embarrassed
extern const lv_image_dsc_t emoji_surprised;   // surprised
extern const lv_image_dsc_t emoji_shocked;     // shocked
extern const lv_image_dsc_t emoji_thinking;    // thinking
// extern const lv_image_dsc_t emoji_1f609;       // winking
extern const lv_image_dsc_t emoji_cool; // cool
// extern const lv_image_dsc_t emoji_1f60c;       // relaxed
// extern const lv_image_dsc_t emoji_1f924;       // delicious
// extern const lv_image_dsc_t emoji_1f618;       // kissy
extern const lv_image_dsc_t emoji_confident; // confident
extern const lv_image_dsc_t emoji_sleepy;    // sleepy
extern const lv_image_dsc_t emoji_silly;     // silly
extern const lv_image_dsc_t emoji_confused;  // confused

typedef struct emoji {
    const lv_image_dsc_t *emoji;
    uint32_t unicode;
} emoji_t;

static const void *get_imgfont_path(const lv_font_t *font, uint32_t unicode, uint32_t unicode_next, int32_t *offset_y,
                                    void *user_data) {
    static const emoji_t emoji_table[] = {
        {&emoji_neutral, 0x1f636},     // neutral
        {&emoji_happy, 0x1f642},       // happy
        {&emoji_neutral, 0x1f606},     // laughing
        {&emoji_neutral, 0x1f602},     // funny
        {&emoji_neutral, 0x1f614},     // sad
        {&emoji_angry, 0x1f620},       // angry
        {&emoji_crying, 0x1f62d},      // crying
        {&emoji_neutral, 0x1f60d},     // loving
        {&emoji_embarrassed, 0x1f633}, // embarrassed
        {&emoji_surprised, 0x1f62f},   // surprised
        {&emoji_shocked, 0x1f631},     // shocked
        {&emoji_thinking, 0x1f914},    // thinking
        {&emoji_neutral, 0x1f609},     // winking
        {&emoji_cool, 0x1f60e},        // cool
        {&emoji_neutral, 0x1f60c},     // relaxed
        {&emoji_neutral, 0x1f924},     // delicious
        {&emoji_neutral, 0x1f618},     // kissy
        {&emoji_confident, 0x1f60f},   // confident
        {&emoji_sleepy, 0x1f634},      // sleepy
        {&emoji_silly, 0x1f61c},       // silly
        {&emoji_confused, 0x1f644},    // confused
    };

    for (size_t i = 0; i < sizeof(emoji_table) / sizeof(emoji_table[0]); i++) {
        if (emoji_table[i].unicode == unicode) {
            return emoji_table[i].emoji;
        }
    }
    return NULL;
}

const lv_font_t *font_emoji_init(void) {
    static lv_font_t *font = NULL;
    if (font == NULL) {
        font = lv_imgfont_create(100, get_imgfont_path, NULL);
        if (font == NULL) {
            LV_LOG_ERROR("Failed to allocate memory for emoji font");
            return NULL;
        }
        font->base_line = 0;
        font->fallback = NULL;
    }
    return font;
}
