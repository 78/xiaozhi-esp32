"""Exercise the real product display controller with fake LVGL widgets."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class ExpressionDisplayTests(unittest.TestCase):
    def test_no_dialogue_retention_and_system_screen_lifecycle(self):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("requires a host C++17 compiler")
        stub = r'''
#pragma once
#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "device_state.h"
struct TextGlyph {};
struct LvglImage {};
struct Font { void* font() { return nullptr; } };
struct Theme { std::shared_ptr<Font> GetTextFont() { return {}; } };
inline int64_t clock_us=0;
inline int64_t esp_timer_get_time() { return clock_us; }
constexpr int LV_TEXT_ALIGN_CENTER=0,LV_ALIGN_TOP_MID=0,LV_ALIGN_CENTER=0,LV_OBJ_FLAG_HIDDEN=1;
struct lv_obj_t { std::string text; int flags=0; };
struct lv_timer_t { void (*callback)(lv_timer_t*); void* data; };
inline std::vector<lv_obj_t*> widgets;
inline std::vector<lv_timer_t*> timers;
inline lv_obj_t screen;
inline lv_obj_t* lv_display_get_screen_active(void*) { return &screen; }
inline lv_obj_t* lv_label_create(lv_obj_t*) { auto p=new lv_obj_t; widgets.push_back(p); return p; }
inline void lv_obj_delete(lv_obj_t* p) { widgets.erase(std::find(widgets.begin(),widgets.end(),p)); delete p; }
inline void lv_label_set_text(lv_obj_t* p,const char* text) { p->text=text; }
inline const char* lv_label_get_text(lv_obj_t* p) { return p->text.c_str(); }
inline void lv_obj_add_flag(lv_obj_t* p,int flag) { p->flags|=flag; }
inline void lv_obj_remove_flag(lv_obj_t* p,int flag) { p->flags&=~flag; }
inline int lv_color_black() { return 0; }
inline int lv_color_white() { return 1; }
template<class... A> void lv_obj_set_style_bg_color(A...) {}
template<class... A> void lv_obj_set_style_text_color(A...) {}
template<class... A> void lv_obj_set_style_text_font(A...) {}
template<class... A> void lv_obj_set_style_text_align(A...) {}
template<class... A> void lv_obj_set_width(A...) {}
template<class... A> void lv_obj_align(A...) {}
inline lv_timer_t* lv_timer_create(void (*f)(lv_timer_t*),int,void* data) {
    auto p=new lv_timer_t{f,data}; timers.push_back(p); return p;
}
inline void lv_timer_delete(lv_timer_t* p) { timers.erase(std::find(timers.begin(),timers.end(),p)); delete p; }
inline void* lv_timer_get_user_data(lv_timer_t* p) { return p->data; }
inline void tick() { auto p=timers.front(); p->callback(p); }
struct Audio { float level=0; float GetPlaybackLevel() const { return level; } };
struct Application {
    DeviceState state=kDeviceStateIdle; bool voice=false; Audio audio;
    static Application& GetInstance() { static Application app; return app; }
    DeviceState GetDeviceState() const { return state; }
    bool IsVoiceDetected() const { return voice; }
    Audio& GetAudioService() { return audio; }
};
class Display {
public:
    virtual ~Display()=default;
    virtual void SetupUI() { setup_ui_called_=true; }
    bool IsSetupUICalled() const { return setup_ui_called_; }
    virtual void SetStatus(const char*) {}
    virtual void SetChatMessage(const char*,const char*) {}
    virtual void ClearChatMessages() {}
    virtual void RemoveChatMessage(const char*,const char*) {}
    virtual void ShowNotification(const char*,int=3000) {}
    virtual void ShowNotification(const std::string&,int=3000) {}
    virtual void UpdateStatusBar(bool=false) {}
    virtual bool SetEmotion(const char*) { return false; }
    virtual bool AddTextGlyphs(const std::vector<TextGlyph>&,uint8_t) { return false; }
    virtual bool SupportsGuiOperations() const { return false; }
    virtual void SetPreviewImage(std::unique_ptr<LvglImage>) {}
    virtual void SetTheme(Theme*) {}
protected:
    bool setup_ui_called_=false;
    int width_=240,height_=280;
    Theme* current_theme_=nullptr;
};
class SpiLcdDisplay : public Display {
protected:
    void* display_=nullptr;
    lv_obj_t* status_label_=nullptr;
    lv_obj_t* chat_message_label_=nullptr;
};
inline bool lock_allowed=true;
struct DisplayLockGuard {
    explicit DisplayLockGuard(Display*) {}
    explicit operator bool() const { return lock_allowed; }
};
class FaceExpression {
public:
    inline static FaceExpression* last=nullptr;
    bool visible=false,recording=false,speaking=false,voice=false,moving=false;
    float level=0; int mouth=0;
    explicit FaceExpression(lv_obj_t*) { last=this; }
    ~FaceExpression() { last=nullptr; }
    static int MakeDefaultConfig(int,int) { return 0; }
    void Show(int) {}
    void SetVisible(bool v) { visible=v; }
    void SetState(bool r,bool s) { recording=r; speaking=s; }
    void SetVoiceActive(bool v) { voice=v; }
    void SetSpeechMotion(bool v,float l) { moving=v; level=l; }
    void SetManualMouthWidth(int w) { mouth=w; }
    void Look(const std::string&) {}
    void Blink(int) {}
};
'''
        driver = r'''
#include <cassert>
#include "expression_display.h"
int main() {
    auto& app=Application::GetInstance();
    {
        PetoiExpressionDisplay display;
        display.SetupUI(); display.SetupUI();
        assert(widgets.size()==2 && timers.size()==1);
        assert(FaceExpression::last->visible);
        display.SetChatMessage("user","stand up");
        display.SetChatMessage("assistant","% self.robot.send_command...");
        assert(widgets[1]->text.empty()); // no dialogue retained in widgets
        display.SetChatMessage("system","network error");
        assert(!FaceExpression::last->visible && widgets[1]->text=="network error");
        display.ClearChatMessages(); assert(FaceExpression::last->visible);
        app.state=kDeviceStateListening; app.voice=true; tick();
        assert(FaceExpression::last->recording && FaceExpression::last->voice);
        app.state=kDeviceStateSpeaking; app.audio.level=0.5f; tick();
        assert(FaceExpression::last->speaking && FaceExpression::last->level==0.5f);
        app.audio.level=0; tick(); assert(!FaceExpression::last->moving);
        display.SetQuietAfterSpeech(true);
        app.state=kDeviceStateListening; tick(); assert(FaceExpression::last->mouth==5);
        display.ShowNotification("notice",10); assert(!FaceExpression::last->visible);
        clock_us=10001; tick(); assert(FaceExpression::last->visible);
        assert(widgets[1]->text.empty());
        app.state=kDeviceStateWifiConfiguring; tick(); assert(!FaceExpression::last->visible);
        app.state=kDeviceStateIdle;
        display.ShowNotification("temporary",10);
        display.SetChatMessage("system","persistent error");
        clock_us+=20000; tick();
        assert(widgets[1]->text=="persistent error" && !FaceExpression::last->visible);
        display.ClearChatMessages();
        display.SetTemporaryMouthWidth(170);
        app.state=kDeviceStateSpeaking; app.audio.level=0.8f; tick();
        assert(FaceExpression::last->mouth==170 && !FaceExpression::last->moving);
        app.state=kDeviceStateListening; tick();
        clock_us+=2000001; tick(); assert(FaceExpression::last->mouth==0);
        lock_allowed=false; display.SetChatMessage("system","ignored");
        assert(widgets[1]->text.empty()); lock_allowed=true;
    }
    assert(widgets.empty() && timers.empty() && !FaceExpression::last);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / "stub.h").write_text(stub, encoding="utf-8")
            for name in ("application.h", "display/lcd_display.h", "face_expression.h", "lvgl_font.h"):
                target = path / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_text('#include "stub.h"\n', encoding="utf-8")
            production = ROOT / "main/boards/petoi/bittle-ai-c3/expression_display.h"
            shutil.copy2(production, path / "expression_display.h")
            source, binary = path / "test.cc", path / "test.exe"
            source.write_text(driver, encoding="utf-8")
            subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-I", str(path),
                            "-I", str(ROOT / "main"), str(source), "-o", str(binary)],
                           check=True, capture_output=True, text=True)
            subprocess.run([str(binary)], check=True, capture_output=True, text=True)


if __name__ == "__main__":
    unittest.main()
