#ifndef __CUSTOM_MATRIX_DISPLAY_H__
#define __CUSTOM_MATRIX_DISPLAY_H__

#include <memory>
#include <string>
#include "lvgl_display.h"

struct Hub75Context;
class EmojiCollection;

class CustomMatrixDisplay : public LvglDisplay {
public:
    CustomMatrixDisplay(int width, int height);
    ~CustomMatrixDisplay();

    void SetBrightness(uint8_t brightness_0_100);

    virtual void SetEmotion(const char* emotion) override;
    virtual void SetChatMessage(const char* role, const char* content) override;
    virtual void SetStatus(const char* status) override;
    virtual void ShowNotification(const char* notification, int duration_ms = 3000) override;
    virtual void UpdateStatusBar(bool update_all = false) override;

protected:
    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

private:
    void SetupUI();
    void RefreshStatusLabelLocked();
    static void LvglFlushCallback(lv_display_t* disp, const lv_area_t* area, uint8_t* color_map);

    Hub75Context* hub75_context_ = nullptr;

    // UI 控件
    lv_obj_t* main_container_ = nullptr;
    lv_obj_t* emoji_image_ = nullptr;
    lv_obj_t* emotion_icon_label_ = nullptr;
    lv_obj_t* message_label_ = nullptr;

    std::shared_ptr<EmojiCollection> emoji_collection_ = nullptr;
    std::string status_text_;
    std::string time_text_;
};

#endif  // __CUSTOM_MATRIX_DISPLAY_H__
