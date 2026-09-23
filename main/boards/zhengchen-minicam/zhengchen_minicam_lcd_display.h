#ifndef ZHENGCHEN_MINICAM_LCD_DISPLAY_H
#define ZHENGCHEN_MINICAM_LCD_DISPLAY_H

#include "display/lcd_display.h"

#include <array>
#include <cstdint>
#include <functional>

class ZhengchenMinicamLcdDisplay : public SpiLcdDisplay {
public:
    using SpiLcdDisplay::SpiLcdDisplay;

    void SetupUI() override;
    void SetTheme(Theme* theme) override;
    void UpdateStatusBar(bool update_all = false) override;

    void ShowSettingsMenu();
    void HideSettingsMenu();
    void ToggleSettingsMenu();
    bool IsSettingsMenuVisible() const;

    void SelectPreviousMenuItem();
    void SelectNextMenuItem();
    void ActivateSelectedMenuItem();

    void SetAecHandlers(std::function<bool()> getter, std::function<void()> toggle_callback);
    void SetOrientationHandlers(std::function<bool()> getter, std::function<void()> toggle_callback);
    void SetReprovisionCallback(std::function<void()> callback);
    bool IsLandscape() const;
    void ToggleOrientation();
    void SetPreviewImage(std::unique_ptr<LvglImage> image) override;

private:
    enum class MenuItem : uint8_t {
        Aec = 0,
        Orientation,
        Reprovision,
        Count,
    };

    static constexpr const char* kSettingsNamespace = "zc_minicam";
    static constexpr const char* kOrientationKey = "lcd_landscape";

    lv_obj_t* settings_menu_ = nullptr;
    lv_obj_t* settings_title_label_ = nullptr;
    std::array<lv_obj_t*, static_cast<size_t>(MenuItem::Count)> menu_item_rows_ = {};
    std::array<lv_obj_t*, static_cast<size_t>(MenuItem::Count)> menu_item_labels_ = {};
    int selected_index_ = 0;

    std::function<bool()> aec_getter_;
    std::function<void()> aec_toggle_callback_;
    std::function<bool()> orientation_getter_;
    std::function<void()> orientation_toggle_callback_;
    std::function<void()> reprovision_callback_;
    bool is_landscape_ = false;

    void CreateSettingsMenuLocked();
    void ApplyMenuThemeLocked();
    void RefreshSettingsMenuLocked();
    void SetSelectedIndex(int index);

    const char* GetAecLabel() const;
    const char* GetOrientationLabel() const;
    const char* GetMenuLabel(MenuItem item) const;
};

#endif // ZHENGCHEN_MINICAM_LCD_DISPLAY_H
