"""OLED power-save must honour a flag that NVS can actually store.

ESP-IDF limits an NVS key to NVS_KEY_NAME_MAX_SIZE - 1 characters (15). The
OLED power-save override reads its switch from the "wifi" namespace, so a key
longer than that is rejected by both nvs_set_* and nvs_get_*: the option can
never be enabled and the panel is never switched off. This runs the real
SetPowerSaveMode body against an NVS double that enforces that limit.
"""
import os
import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
OLED = ROOT / "main/display/oled_display.cc"

NVS_KEY_NAME_MAX_SIZE = 16  # components/nvs_flash/include/nvs.h


def function(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


HARNESS = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>

// ESP-IDF rejects a key whose length reaches NVS_KEY_NAME_MAX_SIZE (16), so the
// longest storable key is 15 characters plus the terminator.
constexpr int ESP_OK = 0;
constexpr int ESP_ERR_NVS_KEY_TOO_LONG = 0x1109;
constexpr int ESP_ERR_NVS_NOT_FOUND = 0x1102;

struct NvsStore {
    std::map<std::string, std::map<std::string, uint8_t>> namespaces;
    int rejected = 0;
};
static NvsStore g_nvs;

static bool KeyStorable(const char* key) {
    const size_t length = std::strlen(key);
    return length > 0 && length < NVS_KEY_NAME_MAX_SIZE;
}

static int SetU8(const char* key, uint8_t value) {
    if (!KeyStorable(key)) { ++g_nvs.rejected; return ESP_ERR_NVS_KEY_TOO_LONG; }
    g_nvs.namespaces["wifi"][key] = value;
    return ESP_OK;
}

static int GetU8(const char* key, uint8_t* value) {
    if (!KeyStorable(key)) { ++g_nvs.rejected; return ESP_ERR_NVS_KEY_TOO_LONG; }
    auto it = g_nvs.namespaces["wifi"].find(key);
    if (it == g_nvs.namespaces["wifi"].end()) return ESP_ERR_NVS_NOT_FOUND;
    *value = it->second;
    return ESP_OK;
}

// Mirrors main/settings.cc: a rejected read falls back to the caller's default.
struct Settings {
    Settings(const char* ns, bool read_write) : writable_(read_write) { (void)ns; }
    bool GetBool(const char* key, bool fallback) {
        uint8_t value = 0;
        if (GetU8(key, &value) != ESP_OK) return fallback;
        return value != 0;
    }
    int SetBool(const char* key, bool value) {
        return writable_ ? SetU8(key, value ? 1 : 0) : ESP_ERR_NVS_NOT_FOUND;
    }
    bool writable_;
};

struct Panel { bool powered = true; };
static Panel g_panel;
static int g_panel_switches = 0;

int esp_lcd_panel_disp_on_off(Panel* panel, bool on) {
    ++g_panel_switches;
    panel->powered = on;
    return ESP_OK;
}

struct LvglDisplay {
    virtual ~LvglDisplay() = default;
    virtual void SetPowerSaveMode(bool) {}
};
struct OledDisplay : LvglDisplay {
    Panel* panel_ = &g_panel;
    void SetPowerSaveMode(bool on) override;
    void SetPowerSaveModeBase() { ++base_calls; }
    int base_calls = 0;
};
'''

MAIN = r'''
int main() {
    OledDisplay display;

    // The provisioning page stores the flag with this key.
    Settings writer("wifi", true);
    assert(writer.SetBool(KEY, true) == ESP_OK && "power-save flag must be storable");

    Settings reader("wifi", false);
    assert(reader.GetBool(KEY, false) && "power-save flag must read back enabled");

    display.SetPowerSaveMode(true);
    assert(!g_panel.powered && "entering power save must switch the panel off");
    display.SetPowerSaveMode(false);
    assert(g_panel.powered && "leaving power save must switch the panel on");
    assert(g_panel_switches == 2);
    assert(display.base_calls == 2 && "LVGL power-save state must still be applied");
}
'''


class OledPowerSaveKeyTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which(os.environ.get("CXX", "c++").split()[0]),
                         "requires a host C++ compiler")
    def test_power_save_flag_survives_an_nvs_round_trip(self):
        body = function(OLED.read_text(encoding="utf-8"),
                        "void OledDisplay::SetPowerSaveMode(")
        key = body.split('GetBool("', 1)[1].split('"', 1)[0]
        self.assertLess(len(key), NVS_KEY_NAME_MAX_SIZE,
                        f"NVS key {key!r} is longer than {NVS_KEY_NAME_MAX_SIZE - 1} characters")

        source = (HARNESS.replace("NVS_KEY_NAME_MAX_SIZE", str(NVS_KEY_NAME_MAX_SIZE))
                  + f'\nstatic const char* KEY = "{key}";\n\n'
                  + body.replace("LvglDisplay::SetPowerSaveMode(on);", "SetPowerSaveModeBase();")
                  + MAIN)
        with tempfile.TemporaryDirectory() as directory:
            binary = pathlib.Path(directory) / "oled_power_save"
            compiler = os.environ.get("CXX", "c++").split()
            subprocess.run(compiler + ["-std=c++17", "-Wall", "-Wextra", "-Werror",
                                       "-x", "c++", "-", "-o", str(binary)],
                           input=source, text=True, check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
