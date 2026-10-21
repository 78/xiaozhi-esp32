#ifndef RETERMINAL_D1001_PUSH_PANEL_H
#define RETERMINAL_D1001_PUSH_PANEL_H

#include "display/lcd_display.h"

#include <esp_http_server.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <functional>
#include <string>
#include <deque>
#include <mutex>
#include <vector>

// Small HTTP server that lets LAN clients push content onto the screen.
//
//   POST /panel/markdown   body: markdown text, rendered full-screen.
//   POST /panel/choice     body: {"title": "...", "options": ["A", ...],
//                                 "timeout_s": 60}
//                          Shows the options as buttons and blocks until the
//                          user taps one; answers
//                          {"selected": <index>, "option": "<text>"}.
//   POST /panel/close      closes the panel.
//   GET  /                 plain-text usage.
//
// The markdown renderer supports a small subset: #/##/### headings, - and *
// bullets, | tables | (drawn with lv_table) and plain paragraphs; **bold**
// markers are stripped. All UI work runs under the display lock, so the panel
// is safe alongside the normal chat UI.
class PushPanel {
public:
    explicit PushPanel(LcdDisplay* display);
    ~PushPanel();

    // Requires lwip/esp_netif to be initialized; call after network start.
    void Start();

    // Height (px) the card must keep clear at the screen bottom, so the chat
    // bar underneath stays visible. Queried under the display lock on every
    // push.
    using BottomInsetProvider = std::function<int32_t()>;
    void SetBottomInsetProvider(BottomInsetProvider provider) {
        bottom_inset_ = std::move(provider);
    }

    // Optional camera hooks: snap returns a JPEG of the current view
    // (GET /camera/snap), tune applies manual exposure/gain/white-balance
    // values, -1 skipping a field (POST /camera/tune?exp_pct=&gain_idx=&r=&b=).
    using CameraSnapFn = std::function<bool(std::vector<uint8_t>&)>;
    using CameraTuneFn = std::function<bool(int, int, int, int)>;
    using CameraRawFn = std::function<bool(std::vector<uint8_t>&, std::string&)>;
    void SetCameraHooks(CameraSnapFn snap, CameraTuneFn tune, CameraRawFn raw = nullptr) {
        camera_snap_ = std::move(snap);
        camera_tune_ = std::move(tune);
        camera_raw_ = std::move(raw);
    }

    // Face recognition hooks: status returns the JSON for GET /face/status,
    // config applies the POST /face/config body.
    using FaceStatusFn = std::function<std::string()>;
    using FaceConfigFn = std::function<bool(const std::string&, std::string*)>;
    void SetFaceHooks(FaceStatusFn status, FaceConfigFn config) {
        face_status_ = std::move(status);
        face_config_ = std::move(config);
    }

private:
    static esp_err_t MarkdownThunk(httpd_req_t* req);
    static esp_err_t ChoiceThunk(httpd_req_t* req);
    static esp_err_t CloseThunk(httpd_req_t* req);
    static esp_err_t UsageThunk(httpd_req_t* req);
    static esp_err_t SnapThunk(httpd_req_t* req);
    static esp_err_t TuneThunk(httpd_req_t* req);
    static esp_err_t RawThunk(httpd_req_t* req);
    static esp_err_t FaceStatusThunk(httpd_req_t* req);
    static esp_err_t FaceConfigThunk(httpd_req_t* req);
    static esp_err_t WakeStatusThunk(httpd_req_t* req);
    static esp_err_t WakeThunk(httpd_req_t* req);
    static void OnChoiceClicked(lv_event_t* event);
    static void OnCloseClicked(lv_event_t* event);
    static void OnBackdropClicked(lv_event_t* event);

    esp_err_t HandleMarkdown(httpd_req_t* req);
    esp_err_t HandleChoice(httpd_req_t* req);
    esp_err_t HandleClose(httpd_req_t* req);
    esp_err_t HandleSnap(httpd_req_t* req);
    esp_err_t HandleTune(httpd_req_t* req);
    esp_err_t HandleRaw(httpd_req_t* req);
    esp_err_t HandleFaceStatus(httpd_req_t* req);
    esp_err_t HandleFaceConfig(httpd_req_t* req);
    esp_err_t HandleWakeStatus(httpd_req_t* req);
    esp_err_t HandleWake(httpd_req_t* req);
    bool ReadBody(httpd_req_t* req, std::string* body);

    // All UI helpers must run with the display lock held.
    void OpenRoot(const char* title);
    void CloseRoot();
    void ArmTtl(int ttl_s);  // 0 cancels; display lock must be held
    void DismissFromUi();    // close button / backdrop / state change
    void RenderMarkdown(const std::string& text);
    void RenderTable(const std::vector<std::string>& lines);
    void AddTextBlock(const std::string& text, int heading_level);

    LcdDisplay* display_;
    BottomInsetProvider bottom_inset_;
    CameraSnapFn camera_snap_;
    CameraTuneFn camera_tune_;
    CameraRawFn camera_raw_;
    FaceStatusFn face_status_;
    FaceConfigFn face_config_;
    httpd_handle_t server_ = nullptr;
    lv_obj_t* root_ = nullptr;
    lv_obj_t* body_ = nullptr;
    lv_obj_t* backdrop_ = nullptr;     // full-screen tap-outside-to-close layer
    lv_timer_t* state_timer_ = nullptr;  // dismisses when the device state changes
    int open_state_ = -1;              // device state snapshot at open time

    lv_timer_t* ttl_timer_ = nullptr;  // auto-dismiss timer (nullptr = keep)
    bool large_text_ = false;          // body text uses the 30px font

    // Pending /panel/choice state. The HTTP worker blocks on the semaphore;
    // the LVGL task gives it when an option (or the close button) is tapped.
    SemaphoreHandle_t choice_sem_ = nullptr;
    std::vector<std::string> choice_options_;
    volatile int choice_selected_ = -1;
    volatile bool choice_pending_ = false;
    struct WakeRequestRecord {
        std::string id;
        std::string fingerprint;
        int64_t accepted_us;
    };
    std::deque<WakeRequestRecord> wake_request_history_;
    std::mutex wake_request_mutex_;
};

#endif  // RETERMINAL_D1001_PUSH_PANEL_H
