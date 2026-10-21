#ifndef RETERMINAL_D1001_SETTINGS_UI_H
#define RETERMINAL_D1001_SETTINGS_UI_H

#include "display/lcd_display.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

enum class SettingsPage {
    Wifi,
    Display,
    Volume,
    Face,
};

enum class WifiSettingsState {
    Idle,
    Scanning,
    SelectWifi,
    InputSsid,
    InputPassword,
    SavedList,
    Connecting,
    Success,
    Failed,
};

// One entry per supported orientation. The LCD flags go to the panel / LVGL
// port, the touch flags to esp_lcd_touch. The table is kept in a single place
// so per-angle calibration only ever edits one function.
struct RotationProfile {
    int degrees;
    bool lcd_mirror_x;
    bool lcd_mirror_y;
    bool lcd_swap_xy;
    bool touch_swap_xy;
    bool touch_mirror_x;
    bool touch_mirror_y;
};

struct WifiScanItem {
    std::string ssid;
    int8_t rssi;
    bool encrypted;
};

class SettingsUi {
public:
    using ConnectCallback = std::function<void(const std::string& ssid,
                                               const std::string& password)>;
    using CloseCallback = std::function<void()>;
    // Fonts are owned by the theme and are freed when the theme is reloaded, so
    // they are always fetched through this hook and never cached.
    using IconFontProvider = std::function<const lv_font_t*(bool large)>;

    SettingsUi(LcdDisplay* display, ConnectCallback connect_cb, CloseCallback close_cb);
    ~SettingsUi();

    void SetIconFontProvider(IconFontProvider provider) {
        icon_font_provider_ = std::move(provider);
    }

    // Invoked after the sleep/auto-off settings change so the board can
    // reconfigure its power save timer immediately.
    void SetPowerSaveChangedCallback(std::function<void()> callback) {
        power_save_changed_ = std::move(callback);
    }

    // Face recognition page hooks: get returns {mode, endpoint}; apply saves
    // them. Wired by the board to the FaceService.
    using FaceGetCallback = std::function<void(int&, std::string&)>;
    using FaceApplyCallback = std::function<void(int, const std::string&)>;
    void SetFaceCallbacks(FaceGetCallback get, FaceApplyCallback apply) {
        face_get_ = std::move(get);
        face_apply_ = std::move(apply);
    }

    // Must run inside the LVGL context (event callback) or while the display
    // lock is held.
    // Opens straight on one page; the page's back arrow closes the overlay.
    void Open(SettingsPage page);
    void Close();
    bool IsOpen() const { return root_ != nullptr; }

    // Re-applies the icon font after the theme was reloaded.
    void RefreshIconFonts();

    // Called from worker tasks; both take the display lock themselves.
    void OnScanComplete(std::vector<WifiScanItem> results, esp_err_t error, uint32_t generation);
    void OnConnectResult(bool success, const std::string& message);

    // Screen rotation persisted in NVS (namespace "reterminal", key "rotation").
    static RotationProfile LoadRotationProfile();
    static RotationProfile MakeRotationProfile(int degrees);
    static bool SaveRotation(int degrees);

private:
    enum class Action {
        Close,
        ShowDisplay,
        ThemePick,
        WifiRescan,
        WifiSavedList,
        WifiManualSsid,
        WifiManualNext,
        WifiPickScanned,
        WifiConnectConfirm,
        WifiPasswordCancel,
        WifiTogglePassword,
        WifiConnectSaved,
        WifiDeleteSaved,
        WifiResultBack,
        RotationPick,
        RotationSave,
        RotationConfirm,
        VolumeMute,
        WifiStaticIp,
        WifiStaticSave,
        WifiStaticUseDhcp,
        SleepDimCycle,
        SleepOffCycle,
        FacePick,
        FaceSave,
        FaceParamCycle,
    };

    struct EventCtx {
        SettingsUi* ui;
        Action action;
        int index;
    };

    // A page that is fading out behind the new one. Its widgets stay alive
    // until the animation completes so the transition never flashes the bare
    // background.
    struct RetiredPage {
        SettingsUi* ui;
        lv_obj_t* header;
        lv_obj_t* body;
        std::vector<std::unique_ptr<EventCtx>> event_ctx;
        std::vector<std::pair<lv_obj_t*, bool>> icon_labels;
    };

    static void EventThunk(lv_event_t* event);
    static void VolumeSliderThunk(lv_event_t* event);
    static void AsyncThunk(void* arg);
    void DrainPendingActions();
    void HandleAction(Action action, int index);
    void ShowStaticIpPage();
    void ShowFacePage();
    void Bind(lv_obj_t* obj, Action action, int index = 0,
              lv_event_code_t code = LV_EVENT_CLICKED);

    // Styling helpers.
    const lv_font_t* IconFont(bool large) const;
    lv_color_t CardColor() const;
    // Theme state is read from Display::GetTheme(), not from the active
    // screen: LcdDisplay::SetTheme() leaves the screen background untouched
    // until the next SetupUI(), so style lookups go stale after a switch.
    bool IsLightTheme() const;
    lv_color_t ScreenBackground() const;
    lv_color_t ThemeTextColor() const;
    lv_color_t AccentInkColor() const;
    lv_color_t SurfaceColor() const;
    lv_color_t SurfaceBorderColor() const;
    lv_color_t RaisedSurfaceColor() const;
    lv_color_t AccentWashStrongColor() const;
    lv_color_t PressedSurfaceColor() const;
    lv_color_t TrackColor() const;
    lv_color_t IconBadgeNeutralColor() const;
    lv_obj_t* MakeIconLabel(lv_obj_t* parent, const char* glyph, bool large);
    lv_obj_t* MakeIconBadge(lv_obj_t* parent, const char* glyph, bool large);
    lv_obj_t* MakeIconButton(lv_obj_t* parent, const char* glyph, Action action);
    lv_obj_t* MakeSectionLabel(lv_obj_t* parent, const char* text);
    lv_obj_t* BuildPage(const char* title, Action back_action, bool with_refresh);
    lv_obj_t* MakeCard(lv_obj_t* parent);
    // ready_action is dispatched when the keyboard's OK key is pressed.
    lv_obj_t* MakeKeyboard(lv_obj_t* parent, lv_obj_t* textarea, Action ready_action,
                           bool push_to_bottom = true);
    lv_obj_t* MakeTextArea(lv_obj_t* parent, const char* placeholder, int max_length);
    void StyleTextArea(lv_obj_t* textarea);
    void UpdateVolumeIcon(int volume);
    lv_obj_t* MakeListItem(lv_obj_t* parent, const char* icon, const char* title,
                           const char* subtitle, Action action, int index,
                           const char* trailing_icon);
    lv_obj_t* MakeTextButton(lv_obj_t* parent, const char* icon, const char* text, Action action,
                             int index, bool primary);
    lv_obj_t* MakeScrollArea(lv_obj_t* parent);
    void RetireCurrentPage();
    void AnimateRetired(RetiredPage* page);
    static void RetiredDoneThunk(lv_anim_t* anim);
    void AnimatePageIn(lv_obj_t* header, lv_obj_t* body);
    void AnimateOpen();
    void AnimateClose();
    void FinishClose();

    void ShowWifiPage();
    void StartWifiScan();
    void ShowWifiList();
    void ShowManualSsid();
    void ShowPasswordInput(const std::string& ssid, bool encrypted);
    void ShowSavedList();
    void ShowConnecting(const std::string& ssid);
    void ShowResult(bool success, const char* message);
    void ShowDisplaySettings();
    void ShowVolumeSettings();
    void ShowRotationConfirm();
    void CommitRotation();

    LcdDisplay* display_;
    ConnectCallback connect_cb_;
    CloseCallback close_cb_;
    IconFontProvider icon_font_provider_;

    lv_obj_t* root_ = nullptr;
    lv_obj_t* static_ta_[4] = {nullptr, nullptr, nullptr, nullptr};
    std::function<void()> power_save_changed_;
    FaceGetCallback face_get_;
    FaceApplyCallback face_apply_;
    int pending_face_mode_ = -1;  // -1 = load from saved config on page entry
    lv_obj_t* header_ = nullptr;
    lv_obj_t* body_ = nullptr;
    lv_obj_t* textarea_ = nullptr;
    lv_obj_t* keyboard_ = nullptr;
    lv_obj_t* volume_slider_ = nullptr;
    lv_obj_t* volume_value_ = nullptr;
    lv_obj_t* volume_icon_ = nullptr;

    SettingsPage page_ = SettingsPage::Wifi;
    WifiSettingsState wifi_state_ = WifiSettingsState::Idle;
    std::vector<WifiScanItem> scan_results_;
    std::string scan_error_;
    std::vector<std::string> saved_ssids_;
    std::string pending_ssid_;
    bool pending_encrypted_ = true;
    int pending_rotation_ = 0;
    // Level restored by the mute button; only ever holds a non-zero volume.
    int volume_restore_ = 60;
    bool operation_active_ = false;
    bool closing_ = false;
    uint32_t scan_generation_ = 0;
    std::vector<std::unique_ptr<EventCtx>> event_ctx_;
    std::vector<std::pair<lv_obj_t*, bool>> icon_labels_;
    std::vector<std::unique_ptr<RetiredPage>> retired_pages_;
    static void OnSwipeUpGesture(lv_event_t* event);
    lv_point_t swipe_start_ = {};
    bool swipe_tracking_ = false;

    std::vector<std::pair<Action, int>> pending_actions_;
    bool async_scheduled_ = false;
};

#endif  // RETERMINAL_D1001_SETTINGS_UI_H
