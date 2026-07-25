#ifndef MHAIBOT_DISPLAY_H
#define MHAIBOT_DISPLAY_H

#include "display/lcd_display.h"
#include "display/mhaibot_face.h"

#include <memory>
#include <string>
#include <unordered_set>

/**
 * Freenove board-local LcdDisplay subclass that shows a MhaiBot face for
 * normal conversation states (including unrecognized/unknown emotions) and
 * falls back to the stock emoji only for the legacy exceptional-state set
 * (warning, error, sad, angry, surprised, cancel).
 */
class MhaiBotDisplay : public SpiLcdDisplay {
public:
    MhaiBotDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width,
                   int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y,
                   bool swap_xy);
    ~MhaiBotDisplay() override;

    void SetupUI() override;
    void SetEmotion(const char* emotion) override;
    void SetPreviewImage(std::unique_ptr<LvglImage> image) override;
    void SetTheme(Theme* theme) override;

private:
    static bool IsLegacyEmotion(const char* emotion);
    static MhaiBotFace::Emotion ToFaceEmotion(const char* emotion);
    void LogUnknownEmotionOnce(const char* emotion);
    void ApplyFaceVisibility();

    std::unique_ptr<MhaiBotFace> face_;
    bool face_visible_ = true;
    std::unordered_set<std::string> logged_unknown_emotions_;
};

#endif  // MHAIBOT_DISPLAY_H
