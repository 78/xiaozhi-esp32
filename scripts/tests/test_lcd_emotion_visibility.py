"""Exercise real LCD emotion and message cleanup methods with LVGL objects."""

import pathlib
import shutil
import subprocess
import tempfile
import unittest

from test_startup_wake_word_hint import definition


ROOT = pathlib.Path(__file__).resolve().parents[2]


@unittest.skipUnless(shutil.which("c++"), "requires a host C++ compiler")
class LcdEmotionVisibilityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.directory.cleanup)
        cls.executable = pathlib.Path(cls.directory.name) / "lcd-emotion-test"
        display = (ROOT / "main/display/lcd_display.cc").read_text()
        driver = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#define CONFIG_USE_WECHAT_MESSAGE_STYLE 1
#define ESP_LOGW(...)
#define ESP_LOGI(...)
#define ESP_LOGE(...)
constexpr int LV_OBJ_FLAG_HIDDEN = 1;
struct lv_font_t {};
struct lv_obj_t {
    bool hidden = false;
    std::string text = "robot";
    const char* role = nullptr;
    lv_obj_t* parent = nullptr;
    std::vector<lv_obj_t*> children;
    const void* source = nullptr;
};
void lv_obj_add_flag(lv_obj_t* o, int) { o->hidden = true; }
void lv_obj_remove_flag(lv_obj_t* o, int) { o->hidden = false; }
uint32_t lv_obj_get_child_cnt(lv_obj_t* o) { return o->children.size(); }
lv_obj_t* lv_obj_get_child(lv_obj_t* o, uint32_t i) { return o->children.at(i); }
void* lv_obj_get_user_data(lv_obj_t* o) { return const_cast<char*>(o->role); }
bool lv_obj_is_valid(lv_obj_t*) { return true; }
void lv_obj_del(lv_obj_t* o) {
    auto& children = o->parent->children;
    children.erase(std::find(children.begin(), children.end(), o));
}
void lv_obj_clean(lv_obj_t* o) { o->children.clear(); }
const char* lv_label_get_text(lv_obj_t* o) { return o->text.c_str(); }
void lv_label_set_text(lv_obj_t* o, const char* s) { o->text = s; }
void lv_obj_set_style_text_font(lv_obj_t*, const lv_font_t*, int) {}
void lv_image_set_src(lv_obj_t* o, const void* s) { o->source = s; }
const char* noto_emoji_get_utf8(const char* emotion) {
    return strcmp(emotion, "happy") == 0 ? "happy glyph" : nullptr;
}
const char* material_symbols_get_utf8(const char* emotion) {
    return strcmp(emotion, "robot_2") == 0 ? "robot" : nullptr;
}
struct Image {
    bool gif = false;
    bool decodable = true;
    bool IsGif() const { return gif; }
    const Image* image_dsc() const { return this; }
};
struct EmojiCollection {
    Image neutral, sleepy{true};
    Image* GetEmojiImage(const char* emotion) {
        if (strcmp(emotion, "neutral") == 0) return &neutral;
        if (strcmp(emotion, "sleepy") == 0) return &sleepy;
        return nullptr;
    }
};
struct Font {
    lv_font_t data;
    const lv_font_t* font() const { return &data; }
};
struct LvglTheme {
    EmojiCollection emojis;
    Font glyph_font;
    EmojiCollection* emoji_collection() { return &emojis; }
    Font* emoji_font() { return &glyph_font; }
    Font* large_icon_font() { return &glyph_font; }
};
struct LvglGif {
    bool playing = false;
    bool loaded;
    Image frame;
    explicit LvglGif(const Image* image) : loaded(image->decodable) {}
    bool IsLoaded() const { return loaded; }
    bool IsPlaying() const { return playing; }
    void Start() { playing = true; }
    void Stop() { playing = false; }
    void Pause() { playing = false; }
    const Image* image_dsc() const { return &frame; }
    void SetFrameCallback(std::function<void()>) {}
};
struct DisplayLockGuard { explicit DisplayLockGuard(void*) {} };
class LcdDisplay {
public:
    bool setup_ui_called_ = true;
    bool emotion_uses_image_ = false;
    LvglTheme theme;
    LvglTheme* current_theme_ = &theme;
    lv_obj_t content, label, image;
    lv_obj_t* content_ = &content;
    lv_obj_t* emoji_label_ = &label;
    lv_obj_t* emoji_image_ = &image;
    lv_obj_t* chat_message_label_ = nullptr;
    std::unique_ptr<LvglGif> gif_controller_;
    LcdDisplay() { image.hidden = true; }
    bool SetEmotion(const char*);
    void ShowEmotion();
    void RemoveChatMessage(const char*, const char*);
    void ClearChatMessages();
};
'''
        driver += definition(display, "void LcdDisplay::ShowEmotion()")
        for signature in (
            "bool LcdDisplay::SetEmotion(",
            "void LcdDisplay::RemoveChatMessage(",
            "void LcdDisplay::ClearChatMessages(",
        ):
            driver += definition(display, signature)
        driver += r'''
int main(int argc, char** argv) {
    assert(argc == 2);
    LcdDisplay display;
    lv_obj_t hint, hint_label;
    hint.role = "assistant";
    hint_label.text = "Startup hint";
    hint.children = {&hint_label};
    hint.parent = display.content_;
    const std::string scenario = argv[1];
    if (scenario == "failed-gif") {
        assert(display.SetEmotion("sleepy"));
        display.theme.emojis.sleepy.decodable = false;
        assert(!display.SetEmotion("sleepy"));
        display.ClearChatMessages();
        assert(display.image.hidden && !display.label.hidden);
        assert(!display.gif_controller_);
        return 0;
    }
    const bool glyph = scenario == "glyph";
    const bool animated = scenario == "gif";
    const char* emotion = glyph ? "happy" : animated ? "sleepy" : "neutral";
    const bool hidden = scenario == "hidden-neutral" || scenario == "hidden-neutral-gif";
    if (scenario == "hidden-neutral-gif") {
        display.theme.emojis.neutral.gif = true;
    }
    if (hidden) {
        display.content.children = {&hint};
    }
    assert(display.SetEmotion(emotion));
    if (hidden) {
        assert(display.label.hidden && display.image.hidden);
        if (display.gif_controller_) assert(!display.gif_controller_->IsPlaying());
    } else {
        assert(display.label.hidden != display.image.hidden);
    }
    const void* source = display.image.source;
    display.content.children = {&hint};
    display.chat_message_label_ = &hint_label;
    // WeChat hides the centered glyph when an assistant bubble is added.
    display.label.hidden = true;
    if (scenario == "clear") {
        display.ClearChatMessages();
    } else {
        display.RemoveChatMessage("assistant", "Startup hint");
    }
    assert(display.content.children.empty());
    assert(display.chat_message_label_ == nullptr);
    assert(display.label.hidden != display.image.hidden && "two emotions overlap or neither is visible");
    assert(display.label.hidden == !glyph);
    if (glyph) {
        assert(display.label.text == "happy glyph");
    } else {
        assert(display.image.source == source);
    }
    if (animated || scenario == "hidden-neutral-gif") {
        assert(display.gif_controller_ && display.gif_controller_->IsPlaying());
    }
    // Switching from an image to a glyph and clearing messages keeps the glyph.
    assert(display.SetEmotion("robot_2"));
    display.ClearChatMessages();
    assert(!display.label.hidden && display.image.hidden);
    assert(display.label.text == "robot");
}
'''
        source = pathlib.Path(cls.directory.name) / "lcd-emotion-test.cc"
        source.write_text(driver)
        subprocess.run(
            ["c++", "-std=c++17", "-fsanitize=undefined", "-fno-sanitize-recover=undefined",
             str(source), "-o", str(cls.executable)],
            check=True, capture_output=True, text=True,
        )

    def check_scenario(self, name):
        result = subprocess.run([str(self.executable), name], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_startup_hint_expiry_keeps_only_the_current_image(self):
        self.check_scenario("startup")

    def test_clearing_messages_keeps_only_the_current_image(self):
        self.check_scenario("clear")

    def test_removing_last_bubble_restores_an_intentionally_hidden_neutral_image(self):
        self.check_scenario("hidden-neutral")

    def test_font_emotion_is_restored_instead_of_an_old_image(self):
        self.check_scenario("glyph")

    def test_animated_emotion_keeps_its_image_and_animation(self):
        self.check_scenario("gif")

    def test_hidden_neutral_gif_resumes_after_last_bubble_is_removed(self):
        self.check_scenario("hidden-neutral-gif")

    def test_failed_gif_decode_does_not_restore_a_freed_image(self):
        self.check_scenario("failed-gif")
