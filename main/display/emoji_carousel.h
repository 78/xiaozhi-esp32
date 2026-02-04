#ifndef EMOJI_CAROUSEL_H
#define EMOJI_CAROUSEL_H

#include <lvgl.h>

class EmojiCarousel {
public:
    EmojiCarousel();
    ~EmojiCarousel();

    void Create(lv_obj_t* parent);
    void Destroy();
    bool IsCreated() const { return is_created_; }

    void NextEmoji();
    void PreviousEmoji();
    int GetCurrentIndex() const { return current_index_; }
    int GetEmojiCount() const { return kEmojiCount; }

private:
    static constexpr int kEmojiCount = 20;
    static const char* const kEmojis[kEmojiCount];

    lv_obj_t* parent_ = nullptr;
    lv_obj_t* emoji_label_ = nullptr;
    int current_index_ = 0;
    bool is_created_ = false;

    void UpdateDisplay();
};

#endif // EMOJI_CAROUSEL_H
