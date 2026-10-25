"""Exercise the real matrix emotion setter and startup fallback on the host."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
DISPLAY = ROOT / "main/boards/waveshare/esp32-s3-rgb-matrix/rgb_matrix_display.cc"


def function(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


class RgbMatrixEmotionTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which("c++"), "requires a host C++ compiler")
    def test_startup_preserves_robot_and_missing_emotions_still_fail(self):
        display = DISPLAY.read_text()
        startup = function(
            (ROOT / "main/application.cc").read_text(),
            'if (!display->SetEmotion("robot_2"))',
        )
        source = r'''
#include <cassert>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
struct Object { bool visible = false; std::string text; const void* image = nullptr; };
struct LvglImage { const void* image_dsc() const { return this; } };
struct EmojiCollection {
    LvglImage neutral;
    std::vector<std::string> requests;
    const LvglImage* GetEmojiImage(const char* name) {
        requests.emplace_back(name);
        return strcmp(name, "neutral") == 0 ? &neutral : nullptr;
    }
};
struct CustomMatrixDisplay {
    Object image, icon;
    Object* emoji_image_ = &image;
    Object* emotion_icon_label_ = &icon;
    std::shared_ptr<EmojiCollection> emoji_collection_ = std::make_shared<EmojiCollection>();
    bool SetEmotion(const char* emotion);
};
struct DisplayLockGuard { explicit DisplayLockGuard(CustomMatrixDisplay*) {} };
void SetObjectVisible(Object* object, bool visible) { if (object) object->visible = visible; }
void lv_image_set_src(Object* object, const void* image) { object->image = image; }
void lv_label_set_text(Object* object, const char* text) { object->text = text; }
#define MATERIAL_SYMBOLS_ROBOT_2 "robot glyph"
'''
        source += function(display, "bool UseBuiltInEmotionIcon(")
        source += function(display, "bool CustomMatrixDisplay::SetEmotion(")
        source += "\nvoid Startup(CustomMatrixDisplay* display) {\n" + startup + "\n}\n"
        source += r'''
int main() {
    CustomMatrixDisplay display;
    assert(display.SetEmotion("robot_2"));
    assert(display.icon.visible && !display.image.visible);
    assert(display.icon.text == MATERIAL_SYMBOLS_ROBOT_2);
    assert(display.emoji_collection_->requests.empty());

    // A loaded neutral image must not overwrite the requested startup robot.
    Startup(&display);
    assert(display.icon.visible && !display.image.visible);
    assert(display.emoji_collection_->requests.empty());

    assert(display.SetEmotion("neutral"));
    assert(display.image.visible && !display.icon.visible);
    assert(display.image.image == display.emoji_collection_->neutral.image_dsc());
    assert(!display.SetEmotion("unknown"));
    assert(display.icon.visible && !display.image.visible);
    assert(!display.SetEmotion("link"));
    assert(!display.SetEmotion(nullptr));
    assert(!display.icon.visible && !display.image.visible);

    // Without a collection the built-in robot still works, but unknown names fail.
    display.emoji_collection_.reset();
    assert(display.SetEmotion("robot_2"));
    assert(!display.SetEmotion("unknown"));
    display.emotion_icon_label_ = nullptr;
    assert(!display.SetEmotion("robot_2"));

    CustomMatrixDisplay missing_image;
    missing_image.emoji_image_ = nullptr;
    assert(!missing_image.SetEmotion("neutral"));
}
'''
        with tempfile.TemporaryDirectory() as directory:
            binary = str(pathlib.Path(directory) / "matrix-emotion-test")
            subprocess.run(
                ["c++", "-std=c++17", "-fsanitize=undefined", "-x", "c++", "-", "-o", binary],
                input=source, text=True, check=True,
            )
            subprocess.run([binary], check=True)
