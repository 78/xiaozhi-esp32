#include "emoji_collection.h"

#include <esp_log.h>

#define TAG "EmojiCollection"

void EmojiCollection::AddEmoji(const std::string& name, LvglImage* image) {
    emoji_collection_[name] = image;
}

const LvglImage* EmojiCollection::GetEmojiImage(const char* name) {
    auto it = emoji_collection_.find(name);
    if (it != emoji_collection_.end()) {
        return it->second;
    }

    ESP_LOGW(TAG, "Emoji not found: %s", name);
    return nullptr;
}

EmojiCollection::~EmojiCollection() {
    for (auto it = emoji_collection_.begin(); it != emoji_collection_.end(); ++it) {
        delete it->second;
    }
    emoji_collection_.clear();
}
