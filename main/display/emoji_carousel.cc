#include "emoji_carousel.h"
#include <font_awesome.h>
#include <esp_log.h>

#define TAG "EmojiCarousel"

// 20 funny Font Awesome emojis - using available icons
const char* const EmojiCarousel::kEmojis[kEmojiCount] = {
    FONT_AWESOME_HAPPY,        // 0 - Happy
    FONT_AWESOME_LAUGHING,     // 1 - Laughing
    FONT_AWESOME_FUNNY,        // 2 - Funny
    FONT_AWESOME_LOVING,       // 3 - Loving (heart eyes)
    FONT_AWESOME_WINKING,      // 4 - Winking
    FONT_AWESOME_KISSY,        // 5 - Kissy
    FONT_AWESOME_COOL,         // 6 - Cool (sunglasses)
    FONT_AWESOME_SURPRISED,    // 7 - Surprised
    FONT_AWESOME_SHOCKED,      // 8 - Shocked
    FONT_AWESOME_THINKING,     // 9 - Thinking
    FONT_AWESOME_SILLY,        // 10 - Silly
    FONT_AWESOME_DELICIOUS,    // 11 - Delicious (yummy)
    FONT_AWESOME_CONFIDENT,    // 12 - Confident
    FONT_AWESOME_RELAXED,      // 13 - Relaxed
    FONT_AWESOME_EMBARRASSED,  // 14 - Embarrassed
    FONT_AWESOME_CONFUSED,     // 15 - Confused
    FONT_AWESOME_SLEEPY,       // 16 - Sleepy
    FONT_AWESOME_SAD,          // 17 - Sad
    FONT_AWESOME_CRYING,       // 18 - Crying
    FONT_AWESOME_NEUTRAL,      // 19 - Neutral
};

EmojiCarousel::EmojiCarousel() {
}

EmojiCarousel::~EmojiCarousel() {
    Destroy();
}

void EmojiCarousel::Create(lv_obj_t* parent) {
    if (is_created_) {
        return;
    }

    parent_ = parent;

    // Create large centered emoji label
    emoji_label_ = lv_label_create(parent);
    lv_obj_center(emoji_label_);

    // Use largest available font - font_awesome_30_4
    extern const lv_font_t font_awesome_30_4;
    lv_obj_set_style_text_font(emoji_label_, &font_awesome_30_4, 0);
    lv_obj_set_style_text_color(emoji_label_, lv_color_hex(0xFFD700), 0); // Gold color

    // Set initial emoji
    lv_label_set_text(emoji_label_, kEmojis[current_index_]);

    is_created_ = true;
    ESP_LOGI(TAG, "EmojiCarousel created with %d emojis, showing: %d", kEmojiCount, current_index_);
}

void EmojiCarousel::Destroy() {
    if (!is_created_) {
        return;
    }

    if (emoji_label_ != nullptr) {
        lv_obj_del(emoji_label_);
        emoji_label_ = nullptr;
    }

    is_created_ = false;
    ESP_LOGI(TAG, "EmojiCarousel destroyed");
}

void EmojiCarousel::NextEmoji() {
    current_index_ = (current_index_ + 1) % kEmojiCount;
    UpdateDisplay();
    ESP_LOGI(TAG, "Next emoji: %d", current_index_);
}

void EmojiCarousel::PreviousEmoji() {
    current_index_ = (current_index_ - 1 + kEmojiCount) % kEmojiCount;
    UpdateDisplay();
    ESP_LOGI(TAG, "Previous emoji: %d", current_index_);
}

void EmojiCarousel::UpdateDisplay() {
    if (!is_created_ || emoji_label_ == nullptr) {
        return;
    }
    lv_label_set_text(emoji_label_, kEmojis[current_index_]);
}
