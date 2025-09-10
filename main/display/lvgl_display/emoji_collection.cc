#include "emoji_collection.h"

#include <esp_log.h>
#include <unordered_map>
#include <string>

#define TAG "EmojiCollection"

// These are declared in xiaozhi-fonts/src/font_emoji_32.c
extern const lv_image_dsc_t emoji_1f636_32; // neutral
extern const lv_image_dsc_t emoji_1f642_32; // happy
extern const lv_image_dsc_t emoji_1f606_32; // laughing
extern const lv_image_dsc_t emoji_1f602_32; // funny
extern const lv_image_dsc_t emoji_1f614_32; // sad
extern const lv_image_dsc_t emoji_1f620_32; // angry
extern const lv_image_dsc_t emoji_1f62d_32; // crying
extern const lv_image_dsc_t emoji_1f60d_32; // loving
extern const lv_image_dsc_t emoji_1f633_32; // embarrassed
extern const lv_image_dsc_t emoji_1f62f_32; // surprised
extern const lv_image_dsc_t emoji_1f631_32; // shocked
extern const lv_image_dsc_t emoji_1f914_32; // thinking
extern const lv_image_dsc_t emoji_1f609_32; // winking
extern const lv_image_dsc_t emoji_1f60e_32; // cool
extern const lv_image_dsc_t emoji_1f60c_32; // relaxed
extern const lv_image_dsc_t emoji_1f924_32; // delicious
extern const lv_image_dsc_t emoji_1f618_32; // kissy
extern const lv_image_dsc_t emoji_1f60f_32; // confident
extern const lv_image_dsc_t emoji_1f634_32; // sleepy
extern const lv_image_dsc_t emoji_1f61c_32; // silly
extern const lv_image_dsc_t emoji_1f644_32; // confused

const lv_img_dsc_t* Twemoji32::GetEmojiImage(const char* name) const {
    static const std::unordered_map<std::string, const lv_img_dsc_t*> emoji_map = {
        {"neutral", &emoji_1f636_32},
        {"happy", &emoji_1f642_32},
        {"laughing", &emoji_1f606_32},
        {"funny", &emoji_1f602_32},
        {"sad", &emoji_1f614_32},
        {"angry", &emoji_1f620_32},
        {"crying", &emoji_1f62d_32},
        {"loving", &emoji_1f60d_32},
        {"embarrassed", &emoji_1f633_32},
        {"surprised", &emoji_1f62f_32},
        {"shocked", &emoji_1f631_32},
        {"thinking", &emoji_1f914_32},
        {"winking", &emoji_1f609_32},
        {"cool", &emoji_1f60e_32},
        {"relaxed", &emoji_1f60c_32},
        {"delicious", &emoji_1f924_32},
        {"kissy", &emoji_1f618_32},
        {"confident", &emoji_1f60f_32},
        {"sleepy", &emoji_1f634_32},
        {"silly", &emoji_1f61c_32},
        {"confused", &emoji_1f644_32},
    };
    
    auto it = emoji_map.find(name);
    if (it != emoji_map.end()) {
        return it->second;
    }
    
    ESP_LOGW(TAG, "Emoji not found: %s", name);
    return nullptr;
}


// These are declared in xiaozhi-fonts/src/font_emoji_64.c
extern const lv_image_dsc_t emoji_1f636_64; // neutral
extern const lv_image_dsc_t emoji_1f642_64; // happy
extern const lv_image_dsc_t emoji_1f606_64; // laughing
extern const lv_image_dsc_t emoji_1f602_64; // funny
extern const lv_image_dsc_t emoji_1f614_64; // sad
extern const lv_image_dsc_t emoji_1f620_64; // angry
extern const lv_image_dsc_t emoji_1f62d_64; // crying
extern const lv_image_dsc_t emoji_1f60d_64; // loving
extern const lv_image_dsc_t emoji_1f633_64; // embarrassed
extern const lv_image_dsc_t emoji_1f62f_64; // surprised
extern const lv_image_dsc_t emoji_1f631_64; // shocked
extern const lv_image_dsc_t emoji_1f914_64; // thinking
extern const lv_image_dsc_t emoji_1f609_64; // winking
extern const lv_image_dsc_t emoji_1f60e_64; // cool
extern const lv_image_dsc_t emoji_1f60c_64; // relaxed
extern const lv_image_dsc_t emoji_1f924_64; // delicious
extern const lv_image_dsc_t emoji_1f618_64; // kissy
extern const lv_image_dsc_t emoji_1f60f_64; // confident
extern const lv_image_dsc_t emoji_1f634_64; // sleepy
extern const lv_image_dsc_t emoji_1f61c_64; // silly
extern const lv_image_dsc_t emoji_1f644_64; // confused

const lv_img_dsc_t* Twemoji64::GetEmojiImage(const char* name) const {
    static const std::unordered_map<std::string, const lv_img_dsc_t*> emoji_map = {
        {"neutral", &emoji_1f636_64},
        {"happy", &emoji_1f642_64},
        {"laughing", &emoji_1f606_64},
        {"funny", &emoji_1f602_64},
        {"sad", &emoji_1f614_64},
        {"angry", &emoji_1f620_64},
        {"crying", &emoji_1f62d_64},
        {"loving", &emoji_1f60d_64},
        {"embarrassed", &emoji_1f633_64},
        {"surprised", &emoji_1f62f_64},
        {"shocked", &emoji_1f631_64},
        {"thinking", &emoji_1f914_64},
        {"winking", &emoji_1f609_64},
        {"cool", &emoji_1f60e_64},
        {"relaxed", &emoji_1f60c_64},
        {"delicious", &emoji_1f924_64},
        {"kissy", &emoji_1f618_64},
        {"confident", &emoji_1f60f_64},
        {"sleepy", &emoji_1f634_64},
        {"silly", &emoji_1f61c_64},
        {"confused", &emoji_1f644_64},
    };
    
    auto it = emoji_map.find(name);
    if (it != emoji_map.end()) {
        return it->second;
    }
    
    ESP_LOGW(TAG, "Emoji not found: %s", name);
    return nullptr;
}


void CustomEmojiCollection::AddEmoji(const std::string& name, LvglImage* image) {
    emoji_collection_[name] = image;
}

const lv_img_dsc_t* CustomEmojiCollection::GetEmojiImage(const char* name) const {
    auto it = emoji_collection_.find(name);
    if (it != emoji_collection_.end()) {
        return it->second->image_dsc();
    }

    ESP_LOGW(TAG, "Emoji not found: %s", name);
    return nullptr;
}

CustomEmojiCollection::~CustomEmojiCollection() {
    for (auto it = emoji_collection_.begin(); it != emoji_collection_.end(); ++it) {
        delete it->second;
    }
    emoji_collection_.clear();
}