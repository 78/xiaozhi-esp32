"""Run the real startup chat hint against host-side application dependencies."""

import json
import os
import shlex
import shutil
import subprocess
import tempfile
import textwrap
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def definition(source, signature, occurrence=0):
    start = source.index(signature)
    for _ in range(occurrence):
        start = source.index(signature, start + len(signature))
    brace = source.index("{", start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


class StartupWakeWordHintTests(unittest.TestCase):
    def compile_and_run(self, driver):
        compiler = shlex.split(os.environ.get("CXX", "c++"))
        if not compiler or shutil.which(compiler[0]) is None:
            self.skipTest("requires a host C++23 compiler")
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            source = path / "startup_wake_word_hint.cc"
            source.write_text(driver, encoding="utf-8")
            executable = path / "startup_wake_word_hint"
            compiled = subprocess.run(
                compiler + ["-std=c++23", str(source), "-o", str(executable)],
                capture_output=True, text=True,
            )
            self.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)
            result = subprocess.run([str(executable)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_startup_hint_is_a_localized_one_time_assistant_message_with_a_five_second_timeout(self):
        locales = ROOT / "main/assets/locales"
        english = json.loads((locales / "en-US/language.json").read_text())["strings"]["WAKE_WORD_HINT"]
        chinese = json.loads((locales / "zh-CN/language.json").read_text())["strings"]["WAKE_WORD_HINT"]
        method = definition(
            (ROOT / "main/application.cc").read_text(),
            "void Application::ShowStartupWakeWordHint()",
        )
        hide = definition(
            (ROOT / "main/application.cc").read_text(),
            "void Application::HideStartupWakeWordHint()",
        )
        platform = r'''
            #include <algorithm>
            #include <cassert>
            #include <cstdint>
            #include <memory>
            #include <string>
            #include <utility>
            #include <vector>
            #define TAG "HostApplication"
            #define ESP_LOGI(...) ((void)0)
            #define ESP_LOGW(...) ((void)0)
            #define ESP_LOGE(...) ((void)0)
            #define ESP_ERROR_CHECK(call) assert((call) == 0)
            constexpr uint32_t MAIN_EVENT_STATE_CHANGED = 1 << 12;
            constexpr uint32_t MAIN_EVENT_STARTUP_HINT_TIMEOUT = 1 << 14;
            uint32_t xEventGroupGetBits(const uint32_t* events) { return *events; }
            void xEventGroupSetBits(uint32_t* events, uint32_t bits) { *events |= bits; }
            constexpr int ESP_TIMER_TASK = 0;
            struct esp_timer_create_args_t {
                void (*callback)(void*);
                void* arg;
                int dispatch_method;
                const char* name;
                bool skip_unhandled_events;
            };
            struct Timer {
                esp_timer_create_args_t args;
                int64_t deadline = 0;
                bool active = false;
            };
            using esp_timer_handle_t = Timer*;
            std::vector<std::unique_ptr<Timer>> timers;
            int64_t now = 0;
            int esp_timer_create(const esp_timer_create_args_t* args, esp_timer_handle_t* handle) {
                timers.push_back(std::make_unique<Timer>(Timer{*args}));
                *handle = timers.back().get();
                return 0;
            }
            int esp_timer_start_once(esp_timer_handle_t handle, int64_t delay) {
                assert(!handle->active);
                handle->active = true;
                handle->deadline = now + delay;
                return 0;
            }
            void advance_time(int64_t duration) {
                now += duration;
                for (auto& timer : timers) {
                    if (timer->active && timer->deadline <= now) {
                        timer->active = false;
                        timer->args.callback(timer->args.arg);
                    }
                }
            }
            enum DeviceState { kDeviceStateIdle, kDeviceStateListening };
            namespace Lang::Strings { const char* WAKE_WORD_HINT = ENGLISH_TEMPLATE; }
            class AudioService {
            public:
                bool running = true;
                std::vector<std::string> words = {"Hi,ESP", "", "Hi,ESP", "Alexa"};
                bool IsWakeWordRunning() const { return running; }
                std::vector<std::string> GetWakeWords() const { return words; }
            };
            class Display {
            public:
                std::string status = "Standby";
                std::vector<std::pair<std::string, std::string>> messages;
                void SetChatMessage(const char* role, const char* content) {
                    messages.emplace_back(role, content);
                }
                void RemoveChatMessage(const char* role, const char* content) {
                    auto it = std::find(messages.begin(), messages.end(), std::make_pair(
                        std::string(role), std::string(content)));
                    if (it != messages.end()) messages.erase(it);
                }
            };
            class Board {
                Display display_;
            public:
                static Board& GetInstance() { static Board board; return board; }
                Display* GetDisplay() { return &display_; }
            };
            class Application {
            public:
                bool pending_startup_wake_word_hint_ = false;
                std::string startup_wake_word_hint_message_;
                esp_timer_handle_t startup_wake_word_hint_timer_ = nullptr;
                uint32_t events = 0;
                uint32_t* event_group_ = &events;
                DeviceState state = kDeviceStateIdle;
                std::string last_error_message_;
                AudioService audio_service_;
                DeviceState GetDeviceState() const { return state; }
                void ShowStartupWakeWordHint();
                void HideStartupWakeWordHint();
            };
        '''
        checks = r'''
            int main() {
                auto display = Board::GetInstance().GetDisplay();
                Application app;
                app.ShowStartupWakeWordHint();
                assert(display->messages.empty());

                // Defer the message until queued idle UI cleanup has completed.
                app.pending_startup_wake_word_hint_ = true;
                app.events = MAIN_EVENT_STATE_CHANGED;
                app.ShowStartupWakeWordHint();
                assert(app.pending_startup_wake_word_hint_);
                assert(display->messages.empty());
                app.events = 0;
                app.ShowStartupWakeWordHint();
                assert(!app.pending_startup_wake_word_hint_);
                assert(display->messages.size() == 1);
                assert(display->messages[0].first == "assistant");
                assert(display->messages[0].second == "Say \"Hi,ESP / Alexa\" to wake me.");
                assert(display->status == "Standby");
                app.ShowStartupWakeWordHint();
                assert(display->messages.size() == 1);
                advance_time(4999000);
                assert(!(app.events & MAIN_EVENT_STARTUP_HINT_TIMEOUT));
                assert(display->messages.size() == 1);
                advance_time(1000);
                assert(app.events & MAIN_EVENT_STARTUP_HINT_TIMEOUT);
                // The timer only queues an event; UI changes happen in the main task.
                assert(display->messages.size() == 1);
                app.HideStartupWakeWordHint();
                assert(display->messages.empty());
                assert(app.startup_wake_word_hint_message_.empty());
                app.HideStartupWakeWordHint();
                assert(display->messages.empty());
                app.events = 0;
                app.ShowStartupWakeWordHint();
                assert(display->messages.empty());

                // Failed startup, disabled detection, and missing words stay silent.
                for (int condition = 0; condition < 4; ++condition) {
                    display->messages.clear();
                    Application skipped;
                    skipped.pending_startup_wake_word_hint_ = true;
                    if (condition == 0) skipped.state = kDeviceStateListening;
                    if (condition == 1) skipped.last_error_message_ = "Network error";
                    if (condition == 2) skipped.audio_service_.running = false;
                    if (condition == 3) skipped.audio_service_.words.clear();
                    skipped.ShowStartupWakeWordHint();
                    assert(display->messages.empty());
                    assert(!skipped.pending_startup_wake_word_hint_);
                    assert(skipped.startup_wake_word_hint_timer_ == nullptr);
                }

                Lang::Strings::WAKE_WORD_HINT = CHINESE_TEMPLATE;
                Application chinese;
                chinese.pending_startup_wake_word_hint_ = true;
                chinese.audio_service_.words = {"你好小智"};
                chinese.ShowStartupWakeWordHint();
                assert(display->messages.size() == 1);
                assert(display->messages[0].first == "assistant");
                assert(display->messages[0].second == "说「你好小智」就能唤醒我。");
                display->SetChatMessage("assistant", "New reply");
                display->SetChatMessage("system", "New alert");
                advance_time(5000000);
                assert(chinese.events & MAIN_EVENT_STARTUP_HINT_TIMEOUT);
                chinese.HideStartupWakeWordHint();
                assert(display->messages.size() == 2);
                assert(display->messages[0].second == "New reply");
                assert(display->messages[1].second == "New alert");

                display->messages.clear();
                Lang::Strings::WAKE_WORD_HINT = "Invalid template";
                Application invalid;
                invalid.pending_startup_wake_word_hint_ = true;
                invalid.ShowStartupWakeWordHint();
                assert(display->messages.empty());
            }
        '''
        driver = textwrap.dedent(platform).replace("ENGLISH_TEMPLATE", json.dumps(english, ensure_ascii=False))
        driver += method + hide + textwrap.dedent(checks).replace("CHINESE_TEMPLATE", json.dumps(chinese, ensure_ascii=False))
        self.compile_and_run(driver)

    def test_expiry_removes_only_the_hint_on_oled_classic_lcd_and_wechat_lcd(self):
        platform = r'''
            #include <algorithm>
            #include <cassert>
            #include <cstdint>
            #include <cstring>
            #include <string>
            #include <vector>
            struct lv_obj_t {
                std::string text;
                const char* role = nullptr;
                lv_obj_t* parent = nullptr;
                std::vector<lv_obj_t*> children;
                bool hidden = false;
                bool deleted = false;
            };
            constexpr int LV_OBJ_FLAG_HIDDEN = 1;
            struct DisplayLockGuard { explicit DisplayLockGuard(void*) {} };
            const char* lv_label_get_text(lv_obj_t* obj) { return obj->text.c_str(); }
            void lv_label_set_text(lv_obj_t* obj, const char* text) { obj->text = text; }
            void lv_anim_delete(lv_obj_t*, void*) {}
            void lv_obj_add_flag(lv_obj_t* obj, int) { obj->hidden = true; }
            void lv_obj_remove_flag(lv_obj_t* obj, int) { obj->hidden = false; }
            uint32_t lv_obj_get_child_cnt(lv_obj_t* obj) { return obj->children.size(); }
            lv_obj_t* lv_obj_get_child(lv_obj_t* obj, uint32_t i) { return obj->children.at(i); }
            void* lv_obj_get_user_data(lv_obj_t* obj) { return const_cast<char*>(obj->role); }
            void lv_obj_del(lv_obj_t* obj) {
                obj->deleted = true;
                auto& siblings = obj->parent->children;
                siblings.erase(std::find(siblings.begin(), siblings.end(), obj));
            }
            class TestDisplay {
            public:
                lv_obj_t* chat_message_label_ = nullptr;
                lv_obj_t* content_right_ = nullptr;
                lv_obj_t* bottom_bar_ = nullptr;
                lv_obj_t* content_ = nullptr;
                lv_obj_t* emoji_label_ = nullptr;
                void RemoveChatMessage(const char* role, const char* content);
            };
        '''
        single_slot_checks = r'''
            int main() {
                TestDisplay display;
                display.RemoveChatMessage("assistant", "Hint");
                lv_obj_t label, bar;
                display.chat_message_label_ = &label;
                display.content_right_ = &bar;
                display.bottom_bar_ = &bar;
                label.text = "New reply";
                display.RemoveChatMessage("assistant", "Hint");
                assert(label.text == "New reply");
                assert(!bar.hidden);
                label.text = "Hint";
                display.RemoveChatMessage("assistant", "Hint");
                assert(label.text.empty());
                assert(bar.hidden);
                display.content_right_ = nullptr;
                display.bottom_bar_ = nullptr;
                label.text = "Hint";
                display.RemoveChatMessage("assistant", "Hint");
                assert(label.text.empty());
                label.text = "Network error";
                display.RemoveChatMessage("assistant", "Hint");
                assert(label.text == "Network error");
            }
        '''
        wechat_checks = r'''
            int main() {
                TestDisplay display;
                display.RemoveChatMessage("assistant", "Hint");
                lv_obj_t content, hint, hint_label, latest, latest_label, user, user_label, emoji;
                hint.role = "assistant";
                hint_label.text = "Hint";
                hint.children = {&hint_label};
                hint.parent = &content;
                latest.role = "assistant";
                latest_label.text = "New reply";
                latest.children = {&latest_label};
                latest.parent = &content;
                user.role = "user";
                user_label.text = "Hint";
                user.children = {&user_label};
                user.parent = &content;
                content.children = {&user, &hint, &latest};
                emoji.hidden = true;
                display.content_ = &content;
                display.emoji_label_ = &emoji;
                display.chat_message_label_ = &latest_label;
                display.RemoveChatMessage("assistant", "Hint");
                assert(hint.deleted);
                assert(!user.deleted && !latest.deleted);
                assert(content.children.size() == 2);
                assert(display.chat_message_label_ == &latest_label);
                assert(emoji.hidden);
                display.RemoveChatMessage("assistant", "Hint");
                assert(content.children.size() == 2);

                // Removing the last bubble restores the idle logo.
                content.children = {&hint};
                hint.deleted = false;
                display.chat_message_label_ = &hint_label;
                display.RemoveChatMessage("assistant", "Hint");
                assert(content.children.empty());
                assert(display.chat_message_label_ == nullptr);
                assert(!emoji.hidden);
            }
        '''
        for name, file, occurrence, checks in (
            ("oled", "oled_display.cc", 0, single_slot_checks),
            ("classic_lcd", "lcd_display.cc", 1, single_slot_checks),
            ("wechat_lcd", "lcd_display.cc", 0, wechat_checks),
        ):
            with self.subTest(display=name):
                source = (ROOT / "main/display" / file).read_text()
                signature = "void " + ("OledDisplay" if name == "oled" else "LcdDisplay") + "::RemoveChatMessage("
                method = definition(source, signature, occurrence)
                method = method.replace("OledDisplay::", "TestDisplay::").replace("LcdDisplay::", "TestDisplay::")
                self.compile_and_run(textwrap.dedent(platform) + method + textwrap.dedent(checks))
