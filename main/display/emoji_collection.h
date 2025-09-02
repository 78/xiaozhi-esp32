#ifndef EMOJI_COLLECTION_H
#define EMOJI_COLLECTION_H

#include <lvgl.h>

#include <map>
#include <string>


// Define interface for emoji collection
class EmojiCollection {
public:
    virtual const lv_img_dsc_t* GetEmojiImage(const char* name) const = 0;
    virtual ~EmojiCollection() = default;
};

class Twemoji32 : public EmojiCollection {
public:
    virtual const lv_img_dsc_t* GetEmojiImage(const char* name) const override;
};

class Twemoji64 : public EmojiCollection {
public:
    virtual const lv_img_dsc_t* GetEmojiImage(const char* name) const override;
};

class CustomEmojiCollection : public EmojiCollection {
private:
    std::map<std::string, lv_img_dsc_t*> emoji_collection_;
public:
    void AddEmoji(const std::string& name, lv_img_dsc_t* image);
    virtual const lv_img_dsc_t* GetEmojiImage(const char* name) const override;
    virtual ~CustomEmojiCollection();
};

#endif
