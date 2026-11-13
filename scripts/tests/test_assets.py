"""Run the real Assets implementation against host platform and rendering doubles."""

import json
import os
import shlex
import shutil
import struct
import subprocess
import tempfile
import textwrap
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class AssetsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shlex.split(os.environ.get("CXX", "c++"))
        if not compiler or shutil.which(compiler[0]) is None:
            raise unittest.SkipTest("requires a host C++17 compiler")
        cls.directory = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.directory.cleanup)
        build = Path(cls.directory.name)

        cjson_flags = shlex.split(os.environ.get("ASSETS_CJSON_CFLAGS", "-I/usr/include/cjson"))
        cjson_libs = shlex.split(os.environ.get("ASSETS_CJSON_LIBS", "-lcjson"))
        probe = subprocess.run(
            compiler + ["-std=c++17", *cjson_flags, "-x", "c++", "-", *cjson_libs,
                        "-o", str(build / "probe")],
            input="#include <cJSON.h>\nint main() { cJSON_Delete(cJSON_CreateObject()); }",
            text=True, capture_output=True,
        )
        if probe.returncode:
            # ESP-IDF's already downloaded component is another real cJSON source.
            # Tests never fetch dependencies or modify managed components.
            cjson = ROOT / "managed_components/espressif__cjson/cJSON"
            cc = shlex.split(os.environ.get("CC", "cc"))
            if not (cjson / "cJSON.c").is_file() or not cc or not shutil.which(cc[0]):
                raise unittest.SkipTest("requires cJSON development files: " + probe.stderr)
            cjson_flags = [f"-I{cjson}"]
            cjson_object = build / "cjson.o"
            subprocess.run(
                cc + ["-std=c99", *cjson_flags, "-c", str(cjson / "cJSON.c"),
                      "-o", str(cjson_object)], check=True, capture_output=True, text=True,
            )
            cjson_libs = [str(cjson_object), "-lm"]

        stubs = {
            "host_state.h": r'''
                #pragma once
                #include <cassert>
                #include <cstdint>
                #include <cstdlib>
                #include <cstring>
                #include <memory>
                #include <string>
                #include <vector>
                inline std::vector<uint8_t> partition_data;
                inline bool lvgl_ready = false, gui_available = false, monochrome = false;
                inline int model_loads = 0, model_sets = 0, font_creations = 0;
                inline int image_creations = 0, ui_calls = 0, theme_refreshes = 0;
                inline int subtitle_updates = 0, lvgl_initializations = 0;
            ''',
            "platform.h": r'''
                #pragma once
                #define _APPLICATION_H_
                #include "host_state.h"
                #include "model_path.h"
                class AudioService {
                public:
                    void SetModelsList(srmodel_list_t* models) {
                        assert(models != nullptr);
                        ++model_sets;
                    }
                };
                class Application {
                public:
                    static Application& GetInstance() { static Application app; return app; }
                    AudioService& GetAudioService() { return audio_; }
                private:
                    AudioService audio_;
                };
            ''',
            "model_path.h": r'''
                #pragma once
                #include "host_state.h"
                struct srmodel_list_t {};
                inline srmodel_list_t* srmodel_load(uint8_t* data) {
                    assert(std::memcmp(data, "models", 6) == 0);
                    ++model_loads;
                    static srmodel_list_t models;
                    return &models;
                }
                inline void esp_srmodel_deinit(srmodel_list_t*) {}
            ''',
            "esp_partition.h": r'''
                #pragma once
                #include "host_state.h"
                using esp_err_t = int;
                using esp_partition_mmap_handle_t = uint32_t;
                constexpr int ESP_OK = 0, ESP_PARTITION_TYPE_ANY = 0;
                constexpr int ESP_PARTITION_SUBTYPE_ANY = 0, ESP_PARTITION_MMAP_DATA = 0;
                struct esp_partition_t { uint32_t size = 65536; };
                inline const char* esp_err_to_name(int) { return "host error"; }
                inline const esp_partition_t* esp_partition_find_first(int, int, const char*) {
                    static esp_partition_t partition;
                    return &partition;
                }
                inline int esp_partition_read(const esp_partition_t*, size_t offset,
                                               void* data, size_t size) {
                    std::memcpy(data, partition_data.data() + offset, size);
                    return ESP_OK;
                }
                inline int esp_partition_mmap(const esp_partition_t*, size_t, size_t, int,
                                               const void** data,
                                               esp_partition_mmap_handle_t* handle) {
                    *data = partition_data.data();
                    *handle = 1;
                    return ESP_OK;
                }
                inline void esp_partition_munmap(esp_partition_mmap_handle_t) {}
                inline size_t esp_partition_get_main_flash_sector_size() { return 4096; }
                inline int esp_partition_erase_range(const esp_partition_t*, size_t, size_t) {
                    std::abort();
                }
                inline int esp_partition_write(const esp_partition_t*, size_t,
                                                const void*, size_t) { std::abort(); }
            ''',
            "spi_flash_mmap.h": r'''
                #pragma once
                constexpr int SPI_FLASH_MMAP_DATA = 0;
                inline int spi_flash_mmap_get_free_pages(int) { return 2; }
            ''',
            "esp_heap_caps.h": r'''
                #pragma once
                #include <cstdlib>
                constexpr int MALLOC_CAP_INTERNAL = 0, MALLOC_CAP_8BIT = 1, MALLOC_CAP_SPIRAM = 2;
                inline void* heap_caps_malloc(size_t size, int) { return std::malloc(size); }
                inline void heap_caps_free(void* data) { std::free(data); }
            ''',
            "esp_system.h": r'''
                #pragma once
                #include <cstdlib>
                [[noreturn]] inline void esp_system_abort(const char*) { std::abort(); }
            ''',
            "esp_timer.h": r'''
                #pragma once
                #include <cstdint>
                using esp_timer_handle_t = void*;
                inline int64_t esp_timer_get_time() { return 0; }
            ''',
            "esp_pm.h": "",
            "esp_log.h": "\n".join(
                f"#define ESP_LOG{level}(...) ((void)0)" for level in "EWID"
            ),
            "lvgl.h": r'''
                #pragma once
                #include "host_state.h"
                using lv_color_t = int;
                struct lv_img_dsc_t {};
                struct lv_font_fmt_txt_dsc_t { uint8_t bpp = 4; };
                struct lv_font_t { const lv_font_t* fallback = nullptr; void* dsc = nullptr; };
                inline bool lv_is_initialized() { return lvgl_ready; }
                inline void lv_init() { ++lvgl_initializations; lvgl_ready = true; }
            ''',
            "cbin_font.h": r'''
                #pragma once
                #include "lvgl.h"
                inline lv_font_t* cbin_font_create(uint8_t*) {
                    assert(lvgl_ready && "font allocation before LVGL initialization");
                    ++font_creations;
                    static lv_font_fmt_txt_dsc_t descriptor;
                    return new lv_font_t{nullptr, &descriptor};
                }
                inline void cbin_font_delete(lv_font_t* font) { delete font; }
            ''',
            "noto_font_bundle.h": '#define NOTO_FONT_BUNDLE_ID "host-font-bundle"\n',
            "lvgl_theme.h": r'''
                #pragma once
                #include "display.h"
                #include "lvgl_font.h"
                class LvglTheme : public Theme {
                public:
                    using Theme::Theme;
                    static lv_color_t ParseColor(const std::string&) { return 0; }
                    void set_emoji_collection(std::shared_ptr<EmojiCollection>) { ++ui_calls; }
                    void set_text_color(lv_color_t) { ++ui_calls; }
                    void set_background_color(lv_color_t) { ++ui_calls; }
                    void set_chat_background_color(lv_color_t) { ++ui_calls; }
                    void set_background_image(std::shared_ptr<LvglImage>) { ++ui_calls; }
                };
                class LvglThemeManager {
                public:
                    static LvglThemeManager& GetInstance() {
                        static LvglThemeManager manager;
                        return manager;
                    }
                    LvglTheme* GetTheme(const std::string& name) {
                        ++ui_calls;
                        if (!gui_available) return nullptr;
                        static LvglTheme light("light"), dark("dark");
                        return name == "dark" ? &dark : (monochrome ? nullptr : &light);
                    }
                };
            ''',
            "board.h": r'''
                #pragma once
                #include "display.h"
                struct HttpError { std::string ToString() const { std::abort(); } };
                struct HttpResult {
                    explicit operator bool() const { std::abort(); }
                    int operator*() const { std::abort(); }
                    HttpError error() const { std::abort(); }
                };
                struct Http {
                    HttpResult Open(const char*, const std::string&) { std::abort(); }
                    HttpResult GetStatusCode() { std::abort(); }
                    size_t GetBodyLength() { std::abort(); }
                    HttpResult Read(void*, size_t) { std::abort(); }
                };
                struct Network { std::unique_ptr<Http> CreateHttp(int) { std::abort(); } };
                class Board {
                public:
                    Display* display = nullptr;
                    static Board& GetInstance() { static Board board; return board; }
                    Display* GetDisplay() { return display; }
                    Network* GetNetwork() { std::abort(); }
                };
            ''',
        }
        for name, contents in stubs.items():
            (build / name).write_text(textwrap.dedent(contents), encoding="utf-8")

        driver = build / "driver.cc"
        driver.write_text(textwrap.dedent(r'''
            #include "assets.h"
            #include "board.h"
            #include "lvgl_theme.h"
            #include <fstream>
            #include <iostream>
            #include <iterator>

            // Keep the actual Display/NoDisplay interfaces; replace their platform side effects.
            Display::Display() = default;
            Display::~Display() = default;
            void Display::SetStatus(const char*) {}
            void Display::ShowNotification(const char*, int) {}
            void Display::ShowNotification(const std::string&, int) {}
            bool Display::SetEmotion(const char*) { return false; }
            void Display::SetChatMessage(const char*, const char*) {}
            void Display::ClearChatMessages() {}
            void Display::SetTheme(Theme*) {}
            void Display::UpdateStatusBar(bool) {}
            void Display::SetPowerSaveMode(bool) {}
            void Display::SetPreviewImage(std::unique_ptr<LvglImage>) {}
            void EmojiCollection::AddEmoji(const std::string&, LvglImage* image) { delete image; }
            const LvglImage* EmojiCollection::GetEmojiImage(const char*) { return nullptr; }
            EmojiCollection::~EmojiCollection() = default;
            LvglRawImage::LvglRawImage(void*, size_t) {
                assert(lvgl_ready); ++image_creations;
            }
            bool LvglRawImage::IsGif() const { return false; }
            LvglCBinImage::LvglCBinImage(void*) {
                assert(lvgl_ready); ++image_creations;
            }
            LvglCBinImage::~LvglCBinImage() = default;

            class GuiDisplay : public Display {
            public:
                bool SupportsGuiOperations() const override { return true; }
                bool SetTextFont(std::shared_ptr<LvglFont> font) override {
                    assert(lvgl_ready); ++ui_calls; return font->font() != nullptr;
                }
                void SetEmojiCollection(std::shared_ptr<EmojiCollection>) override { ++ui_calls; }
                Theme* GetTheme() override {
                    return LvglThemeManager::GetInstance().GetTheme("dark");
                }
                void SetTheme(Theme*) override { ++theme_refreshes; }
                void SetHideSubtitle(bool) override { ++subtitle_updates; }
            protected:
                bool Lock(int) override { return true; }
                void Unlock() override {}
            };

            int main(int argc, char** argv) {
                assert(argc == 3);
                std::string mode = argv[1];
                lvgl_ready = mode == "headless-initialized" || mode == "oled" ||
                             mode == "lcd" || mode == "lcd-no-refresh";
                gui_available = mode == "gui-uninitialized" || mode == "oled" ||
                                mode == "lcd" || mode == "lcd-no-refresh";
                monochrome = mode == "oled";
                NoDisplay headless;
                GuiDisplay gui;
                Board::GetInstance().display = mode == "null" ? nullptr :
                                               (gui_available ? static_cast<Display*>(&gui) : &headless);
                std::ifstream input(argv[2], std::ios::binary);
                assert(input);
                partition_data.assign(std::istreambuf_iterator<char>(input), {});
                partition_data.resize(65536);
                auto& assets = Assets::GetInstance();
                assert(assets.partition_valid());
                bool applied = assets.Apply(mode != "lcd-no-refresh");
                std::cout << "{\"applied\":" << applied
                          << ",\"model_loads\":" << model_loads
                          << ",\"model_sets\":" << model_sets
                          << ",\"fonts\":" << font_creations
                          << ",\"images\":" << image_creations
                          << ",\"ui_calls\":" << ui_calls
                          << ",\"theme_refreshes\":" << theme_refreshes
                          << ",\"subtitle_updates\":" << subtitle_updates
                          << ",\"lvgl_initializations\":" << lvgl_initializations
                          << ",\"glyph_push\":" << assets.text_font_capability().glyph_push
                          << "}\n";
            }
        '''), encoding="utf-8")
        cls.executable = build / "assets_test"
        # Firmware targets use unsigned char when calculating the packed byte checksum.
        compilation = subprocess.run(
            compiler + ["-std=c++17", "-fno-exceptions", "-fno-rtti", "-funsigned-char",
                        "-DHAVE_LVGL=1",
                        "-DTEXT_FONT_SIZE=16", "-DTEXT_FONT_BPP=4", *cjson_flags,
                        f"-I{build}", f"-I{ROOT / 'main'}", f"-I{ROOT / 'main/display'}",
                        f"-I{ROOT / 'main/display/lvgl_display'}",
                        "-include", str(build / "platform.h"), str(driver),
                        str(ROOT / "main/assets.cc"),
                        str(ROOT / "main/display/lvgl_display/lvgl_font.cc"),
                        *cjson_libs, "-o", str(cls.executable)],
            capture_output=True, text=True,
        )
        if compilation.returncode:
            raise RuntimeError("Failed to compile Assets host test:\n" + compilation.stderr)

    def apply(self, mode, index=None):
        if index is None:
            index = {
                "version": 1, "srmodels": "models.bin", "text_font": "font.bin",
                "text_font_meta": {"charset": "basic", "size": 16, "bpp": 4,
                                   "bundle": "host-font-bundle"},
                "emoji_collection": [{"name": "happy", "file": "emoji.bin"}],
                "skin": {name: {"background_image": "background.bin",
                                "text_color": "#ffffff", "background_color": "#000000"}
                         for name in ("light", "dark")},
                "hide_subtitle": True,
            }
        index_data = index if isinstance(index, bytes) else json.dumps(index).encode()
        files = {"index.json": index_data, "models.bin": b"models", "font.bin": b"font",
                 "emoji.bin": b"image", "background.bin": b"image"}
        table, data = bytearray(), bytearray()
        for name, contents in files.items():
            table.extend(struct.pack("<32sIIHH", name.encode(), len(contents), len(data), 0, 0))
            data.extend(b"ZZ" + contents)
        payload = table + data
        fixture = Path(self.directory.name) / "assets.bin"
        fixture.write_bytes(struct.pack("<III", len(files), sum(payload) & 0xFFFF, len(payload))
                            + payload)
        result = subprocess.run([str(self.executable), mode, str(fixture)],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        return json.loads(result.stdout)

    def test_unavailable_ui_preserves_speech_without_rendering_or_glyph_push(self):
        for mode in ("null", "headless", "headless-initialized", "gui-uninitialized"):
            with self.subTest(mode=mode):
                self.assertEqual(self.apply(mode), {
                    "applied": 1, "model_loads": 1, "model_sets": 1, "fonts": 0, "images": 0,
                    "ui_calls": 0, "theme_refreshes": 0, "subtitle_updates": 0,
                    "lvgl_initializations": 0, "glyph_push": 0,
                })

    def test_initialized_oled_and_lcd_still_apply_ui_assets(self):
        for mode, images in (("oled", 2), ("lcd", 3), ("lcd-no-refresh", 3)):
            with self.subTest(mode=mode):
                result = self.apply(mode)
                self.assertGreater(result.pop("ui_calls"), 0)
                refresh = int(mode != "lcd-no-refresh")
                self.assertEqual(result, {
                    "applied": 1, "model_loads": 1, "model_sets": 1, "fonts": 1,
                    "images": images, "theme_refreshes": refresh, "subtitle_updates": refresh,
                    "lvgl_initializations": 0, "glyph_push": 1,
                })

    def test_unavailable_display_does_not_accept_invalid_asset_indexes(self):
        for index in (b"{", {"version": 2, "srmodels": "models.bin"}):
            with self.subTest(index=index):
                result = self.apply("headless", index)
                self.assertEqual(result["applied"], 0)
                self.assertEqual(result["model_loads"], 0)
                self.assertEqual(result["ui_calls"], 0)


if __name__ == "__main__":
    unittest.main()
