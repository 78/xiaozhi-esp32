"""Exercise the real idle timer and SenseCAP power snapshots on the host."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
COMMON = ROOT / "main/boards/common"
WATCHER = ROOT / "main/boards/sensecap-watcher/sensecap_watcher.cc"


def function(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def without_includes(source):
    return "\n".join(line for line in source.splitlines()
                     if not line.startswith(("#include", "#pragma once")))


@unittest.skipUnless(shutil.which("c++"), "requires a host C++ compiler")
class PowerSaveTimerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.tmp.cleanup)
        cls.binary = str(pathlib.Path(cls.tmp.name) / "power-save-test")
        source = r'''
#include <cassert>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

using esp_err_t = int;
constexpr int ESP_OK = 0, ESP_TIMER_TASK = 0;
#define ESP_LOGI(...)
#define ESP_ERROR_CHECK(result) assert((result) == ESP_OK)
#define pdMS_TO_TICKS(ms) (ms)
void vTaskDelay(int) {}
struct esp_timer_create_args_t {
    void (*callback)(void*);
    void* arg;
    int dispatch_method;
    const char* name;
    bool skip_unhandled_events;
};
struct Timer {
    esp_timer_create_args_t args;
    bool active = false;
};
using esp_timer_handle_t = Timer*;
std::vector<Timer*> timers;
int esp_timer_create(const esp_timer_create_args_t* args, Timer** timer) {
    *timer = new Timer{*args};
    timers.push_back(*timer);
    return ESP_OK;
}
int esp_timer_start_periodic(Timer* timer, int64_t period) {
    assert(!timer->active && period == 1000000);
    timer->active = true;
    return ESP_OK;
}
int esp_timer_stop(Timer* timer) { timer->active = false; return ESP_OK; }
int esp_timer_delete(Timer* timer) { delete timer; return ESP_OK; }
struct esp_pm_config_t {
    int max_freq_mhz;
    int min_freq_mhz;
    bool light_sleep_enable;
};
bool light_sleep_enabled = false;
int pm_min_freq = 0;
int esp_pm_configure(const esp_pm_config_t* config) {
    light_sleep_enabled = config->light_sleep_enable;
    pm_min_freq = config->min_freq_mhz;
    return ESP_OK;
}
bool sleep_allowed = true;
int dim_minutes = -1, off_minutes = -1;
struct Settings {
    Settings(const char*, bool) {}
    bool GetBool(const char* key, bool) {
        assert(std::string(key).size() <= 15);  // NVS key limit
        return sleep_allowed;
    }
    int GetInt(const char* key, int fallback) {
        std::string k(key);
        assert(k.size() <= 15);  // NVS key limit
        if (k == "sleep_dim_min") return dim_minutes;
        if (k == "sleep_off_min") return off_minutes;
        return fallback;
    }
};
bool wake_word_running = false;
struct AudioService {
    bool IsWakeWordRunning() { return wake_word_running; }
    void EnableWakeWordDetection(bool enable) { wake_word_running = enable; }
};
struct Codec { void EnableInput(bool) {} };
struct Board {
    static Board& GetInstance() { static Board board; return board; }
    Codec* GetAudioCodec() { return nullptr; }
};
struct Application {
    bool idle = true;
    AudioService audio;
    std::vector<std::function<void()>> pending;
    static Application& GetInstance() { static Application app; return app; }
    bool CanEnterSleepMode() { return idle; }
    AudioService& GetAudioService() { return audio; }
    void Schedule(std::function<void()> callback) { pending.push_back(callback); }
    void Flush() {
        auto callbacks = std::move(pending);
        pending.clear();
        for (auto& callback : callbacks) callback();
    }
};
void Tick(int seconds, bool run_main = true) {
    for (int second = 0; second < seconds; ++second) {
        for (auto* timer : timers) {
            if (timer->active) timer->args.callback(timer->args.arg);
        }
        if (run_main) Application::GetInstance().Flush();
    }
}
'''
        source += without_includes((COMMON / "power_save_timer.h").read_text())
        source += without_includes((COMMON / "power_save_timer.cc").read_text())
        source += r'''
bool usb_connected = false;
int power_offs = 0;
constexpr int BSP_PWR_VBUS_IN_DET = 1, BSP_PWR_SYSTEM = 2;
int IoExpanderGetLevel(int pin) {
    assert(pin == BSP_PWR_VBUS_IN_DET);
    return usb_connected ? 0 : 1;
}
void IoExpanderSetLevel(int pin, int level) {
    assert(pin == BSP_PWR_SYSTEM && level == 0);
    ++power_offs;
}
float BatterygetPercent(bool) { return 80; }
struct Display { void SetPowerSaveMode(bool) {} };
struct Backlight {
    void SetBrightness(int) {}
    void RestoreBrightness() {}
};
struct BatteryProvider {
    virtual bool GetBatteryLevel(int&, bool&, bool&) = 0;
};
class Watcher : public BatteryProvider {
public:
    PowerSaveTimer* power_save_timer_;
    Display display;
    Backlight backlight;
    Display* GetDisplay() { return &display; }
    Backlight* GetBacklight() { return &backlight; }
    ~Watcher() { delete power_save_timer_; }
    void Snapshot() {
        int level = -1;
        bool charging = false, discharging = false;
        assert(GetBatteryLevel(level, charging, discharging));
        assert(charging == usb_connected && discharging != charging && level == 80);
    }
'''
        board = WATCHER.read_text()
        source += function(board, "void InitializePowerSaveTimer()")
        source += function(board, "virtual bool GetBatteryLevel(")
        source += r'''
};
int main(int argc, char** argv) {
    assert(argc == 2);
    std::string test = argv[1];

    if (test == "disable_active_sleep" || test == "disable_dim_active_sleep") {
        wake_word_running = true;
        PowerSaveTimer sleeping(160, 1, 60);
        int entered = 0, exited = 0, requests = 0;
        sleeping.OnEnterSleepMode([&]() { ++entered; });
        sleeping.OnExitSleepMode([&]() { ++exited; });
        sleeping.OnShutdownRequest([&]() { ++requests; });
        sleeping.SetEnabled(true);
        Tick(1);
        assert(entered == 1 && light_sleep_enabled && !wake_word_running);
        if (test == "disable_active_sleep") sleep_allowed = false;
        else dim_minutes = 0;
        Tick(1);
        assert(exited == 1 && !light_sleep_enabled && wake_word_running && pm_min_freq == 160);
        Tick(57);
        assert(requests == 0 && exited == 1);
        Tick(1);
        assert(requests == (test == "disable_active_sleep" ? 0 : 1));
        return 0;
    }
    if (test == "reenable_fresh_cycle") {
        sleep_allowed = false;
        PowerSaveTimer polling(-1, -1, 2);
        int requests = 0;
        polling.OnShutdownRequest([&]() { ++requests; });
        polling.SetEnabled(true);
        Tick(10);
        assert(requests == 0);
        sleep_allowed = true;
        Tick(1);
        assert(requests == 0);
        Tick(1);
        assert(requests == 1);
        return 0;
    }
    if (test == "all_steps_disabled") {
        dim_minutes = 0;
        off_minutes = 0;
        PowerSaveTimer polling(-1, 1, 2);
        int requests = 0;
        polling.OnShutdownRequest([&]() { ++requests; });
        polling.SetEnabled(true);
        Tick(100);
        assert(requests == 0);
        off_minutes = 1;
        Tick(59);
        assert(requests == 0);
        Tick(1);
        assert(requests == 1);
        return 0;
    }
    if (test == "invalid_dim_minutes" || test == "invalid_off_minutes" ||
        test == "minutes_out_of_range" || test == "negative_minutes") {
        int value = test == "minutes_out_of_range" ? 1441 :
                    test == "negative_minutes" ? -2 : 2147483647;
        if (test == "invalid_off_minutes") off_minutes = value;
        else dim_minutes = value;
        PowerSaveTimer guarded(-1, 20, 30);
        int entered = 0, requests = 0;
        guarded.OnEnterSleepMode([&]() { ++entered; });
        guarded.OnShutdownRequest([&]() { ++requests; });
        guarded.SetEnabled(true);
        Tick(19);
        assert(entered == 0 && requests == 0);
        Tick(1);
        assert(entered == 1 && requests == 0);
        Tick(9);
        assert(requests == 0);
        Tick(1);
        assert(requests == 1);
        return 0;
    }
    if (test == "maximum_minutes") {
        dim_minutes = 1440;
        off_minutes = 0;
        PowerSaveTimer bounded(-1, 1, 2);
        int entered = 0, requests = 0;
        bounded.OnEnterSleepMode([&]() { ++entered; });
        bounded.OnShutdownRequest([&]() { ++requests; });
        bounded.SetEnabled(true);
        Tick(86399);
        assert(entered == 0 && requests == 0);
        Tick(1);
        assert(entered == 1 && requests == 0);
        return 0;
    }

    if (test == "usb_boot" || test == "battery_boot" || test == "plug_unplug" ||
        test == "short_usb" || test == "queued_usb") {
        usb_connected = test == "usb_boot";
        Watcher watcher;
        watcher.InitializePowerSaveTimer();
        watcher.Snapshot();
        if (test == "usb_boot") {
            Tick(600);
            assert(power_offs == 0);
            usb_connected = false;
            watcher.Snapshot();
        } else if (test == "plug_unplug") {
            Tick(299);
            usb_connected = true;
            watcher.Snapshot();
            Tick(600);
            assert(power_offs == 0);
            usb_connected = false;
            watcher.Snapshot();
        } else if (test == "short_usb" || test == "queued_usb") {
            Tick(299);
            usb_connected = true;
            Tick(1, test != "queued_usb");  // Battery polling misses this connection.
            assert(power_offs == 0);
            usb_connected = false;
            watcher.Snapshot();
            Application::GetInstance().Flush();
        }
        Tick(299);
        assert(power_offs == 0);
        Tick(1);
        assert(power_offs == 1);
        Tick(10);
        assert(power_offs == 1);
        return 0;
    }
    // No sleep stage: activity must rearm a declined shutdown even when no
    // caller explicitly invokes WakeUp().
    PowerSaveTimer timer(-1, -1, 2);
    int requests = 0;
    timer.OnShutdownRequest([&]() { ++requests; });
    if (test == "settings_disabled") sleep_allowed = false;
    if (test == "nvs_override") { dim_minutes = 0; off_minutes = 1; }
    if (test == "late_change") { dim_minutes = 0; off_minutes = 10; }
    timer.SetEnabled(true);
    Tick(3);
    if (test == "settings_disabled") {
        assert(requests == 0);
        return 0;
    }
    if (test == "nvs_override") {
        // NVS minutes (0 = dim disabled, 1 = power off after 60 s) must win
        // over the constructor's shutdown-at-2-s fallback.
        assert(requests == 0);
        Tick(57);
        assert(requests == 1);
        return 0;
    }
    if (test == "late_change") {
        // Changing the values mid-run applies them to the running clock
        // without restarting it: 60 s in, switching to power off at 60 s
        // fires on the next tick instead of counting 60 s again.
        Tick(57);
        assert(requests == 0);
        off_minutes = 1;
        Tick(1);
        assert(requests == 1);
        return 0;
    }
    assert(requests == 1);
    Tick(20);
    assert(requests == 1);
    if (test == "once") return 0;
    if (test == "wake") {
        timer.WakeUp();
    } else if (test == "activity") {
        Application::GetInstance().idle = false;
        Tick(1);
        Application::GetInstance().idle = true;
    } else if (test == "enable_transition") {
        timer.SetEnabled(true);
        Tick(3);
        assert(requests == 1);
        timer.SetEnabled(false);
        Tick(3);
        assert(requests == 1);
        timer.SetEnabled(true);
    } else if (test == "callback_rearm") {
        timer.OnShutdownRequest([&]() { ++requests; timer.WakeUp(); });
        timer.WakeUp();
        Tick(4);
        assert(requests == 3);
        return 0;
    } else {
        assert(false);
    }
    Tick(1);
    assert(requests == 1);
    Tick(1);
    assert(requests == 2);
    Tick(10);
    assert(requests == 2);
}
'''
        subprocess.run(["c++", "-std=c++20", "-fsanitize=undefined", "-fno-sanitize-recover=undefined", "-x", "c++", "-",
                        "-o", cls.binary], input=source, text=True, check=True)

    def run_case(self, name):
        subprocess.run([self.binary, name], check=True)

    def test_request_once_per_idle_cycle(self):
        self.run_case("once")

    def test_wakeup_rearms_request(self):
        self.run_case("wake")

    def test_busy_application_rearms_before_sleep(self):
        self.run_case("activity")

    def test_reenable_is_noop_until_disabled(self):
        self.run_case("enable_transition")

    def test_callback_can_rearm_request(self):
        self.run_case("callback_rearm")

    def test_settings_disable_timer(self):
        self.run_case("settings_disabled")

    def test_nvs_minutes_override_constructor(self):
        self.run_case("nvs_override")

    def test_settings_apply_without_clock_restart(self):
        self.run_case("late_change")

    def test_watcher_usb_boot_then_unplug(self):
        self.run_case("usb_boot")

    def test_watcher_battery_boot(self):
        self.run_case("battery_boot")

    def test_watcher_plug_and_unplug_restart_countdown(self):
        self.run_case("plug_unplug")

    def test_watcher_retries_after_usb_connection_missed_by_polling(self):
        self.run_case("short_usb")

    def test_watcher_unplugs_before_queued_charging_request_runs(self):
        self.run_case("queued_usb")

    def test_disabling_settings_exits_sleep_and_restores_power_state(self):
        self.run_case("disable_active_sleep")

    def test_disabling_dim_preserves_shutdown_deadline(self):
        self.run_case("disable_dim_active_sleep")

    def test_reenabling_settings_starts_fresh_idle_cycle(self):
        self.run_case("reenable_fresh_cycle")

    def test_enabling_step_after_all_disabled_starts_fresh_cycle(self):
        self.run_case("all_steps_disabled")

    def test_invalid_dim_minutes_preserve_board_default(self):
        self.run_case("invalid_dim_minutes")

    def test_invalid_shutdown_minutes_preserve_board_default(self):
        self.run_case("invalid_off_minutes")

    def test_minutes_above_portal_limit_preserve_board_default(self):
        self.run_case("minutes_out_of_range")

    def test_negative_minutes_preserve_board_default(self):
        self.run_case("negative_minutes")

    def test_maximum_supported_minutes(self):
        self.run_case("maximum_minutes")
