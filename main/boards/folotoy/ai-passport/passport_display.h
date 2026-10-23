#ifndef PASSPORT_DISPLAY_H
#define PASSPORT_DISPLAY_H

#include "display/lcd_display.h"

#include <lvgl.h>

#include <string>

// The Passport's SPI LCD. With the reading message style it labels the physical
// keys along the screen edges every time the firmware starts: the power key on the
// left, UP / DOWN / OK on the right. The labels stay for kKeyHintShowMs, turn grey
// for kKeyHintGreyMs and are then removed. Text appearing in the transcript removes
// them at once; status row text does not.
class PassportDisplay : public SpiLcdDisplay {
public:
    using SpiLcdDisplay::SpiLcdDisplay;

#if CONFIG_USE_READING_MESSAGE_STYLE
    ~PassportDisplay();

    virtual void SetupUI() override;
    virtual void SetTheme(Theme* theme) override;
    virtual void SetStatus(const char* status) override;
    virtual void SetChatMessage(const char* role, const char* content) override;

    // Shows the key labels from the start; SetupUI() calls it on every start-up.
    void ShowKeyHints();
    bool KeyHintsVisible();

private:
    lv_obj_t* CreateKeyHint(bool left_edge, int key_center_y, const std::string& text);
    void StyleKeyHint(lv_obj_t* hint);
    void PlaceKeyHints();
    void GreyKeyHints();
    void HideKeyHints(const char* reason);
    static void OnKeyHintTimer(lv_timer_t* timer);

    lv_obj_t* key_hints_ = nullptr;  // full-screen layer holding the labels
    lv_timer_t* key_hint_timer_ = nullptr;
    bool key_hints_grey_ = false;
#endif
};

#endif  // PASSPORT_DISPLAY_H
