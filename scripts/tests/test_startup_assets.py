"""Exercise application startup and deferred asset updates with host dependencies."""

import os
import shlex
import shutil
import subprocess
import tempfile
import textwrap
import unittest
from pathlib import Path

from test_startup_wake_word_hint import definition


ROOT = Path(__file__).resolve().parents[2]


class StartupAssetsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shlex.split(os.environ.get("CXX", "c++"))
        if not compiler or shutil.which(compiler[0]) is None:
            raise unittest.SkipTest("requires a host C++17 compiler")
        cls.directory = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.directory.cleanup)
        path = Path(cls.directory.name)
        application = (ROOT / "main/application.cc").read_text()
        methods = "\n".join(definition(application, signature) for signature in (
            "void Application::Initialize()",
            "void Application::CheckAssetsVersion()",
        ))
        platform = r'''
            #include <cassert>
            #include <cstdint>
            #include <cstdio>
            #include <functional>
            #include <string>
            #include <vector>
            #define TAG "HostApplication"
            #define BOARD_NAME "host-board"
            #define ESP_LOGI(...) ((void)0)
            #define ESP_LOGW(...) ((void)0)
            #define pdMS_TO_TICKS(ms) (ms)
            void vTaskDelay(int) {}
            void xEventGroupSetBits(int, int) {}
            void esp_timer_start_periodic(int, int) {}
            enum {
                MAIN_EVENT_SEND_AUDIO, MAIN_EVENT_WAKE_WORD_DETECTED,
                MAIN_EVENT_VAD_CHANGE, MAIN_EVENT_PLAYBACK_DRAINED,
                MAIN_EVENT_STATE_CHANGED, MAIN_EVENT_NETWORK_DISCONNECTED,
                MAIN_EVENT_NETWORK_CONNECTED,
            };
            enum DeviceState { kDeviceStateStarting, kDeviceStateUpgrading,
                               kDeviceStateActivating };
            enum class PowerSaveLevel { PERFORMANCE, LOW_POWER };
            enum class NetworkEvent {
                Scanning, Connecting, Connected, Disconnected,
                WifiConfigModeEnter, WifiConfigModeExit, ModemDetecting,
                ModemErrorNoSim, ModemErrorRegDenied, ModemErrorInitFailed, ModemErrorTimeout,
            };
            namespace Lang {
                namespace Strings {
                    const char* SCANNING_WIFI = "Scanning";
                    const char* REGISTERING_NETWORK = "Registering";
                    const char* CONNECT_TO = "Connecting to ";
                    const char* CONNECTED_TO = "Connected to ";
                    const char* DETECTING_MODULE = "Detecting";
                    const char* ERROR = "Error";
                    const char* PIN_ERROR = "SIM error";
                    const char* REG_ERROR = "Registration error";
                    const char* MODEM_INIT_ERROR = "Modem error";
                    const char* FOUND_NEW_ASSETS = "New assets: %s";
                    const char* LOADING_ASSETS = "Loading assets";
                    const char* PLEASE_WAIT = "Please wait";
                    const char* DOWNLOAD_ASSETS_FAILED = "Download failed";
                }
                namespace Sounds {
                    const char* OGG_ERR_PIN = "", *OGG_ERR_REG = "";
                    const char* OGG_EXCLAMATION = "", *OGG_UPGRADE = "";
                }
            }
            std::vector<std::string> operations;
            bool ui_ready = false, audio_ready = false, network_started = false;
            bool font_loaded = false, pending_update = false, valid_partition = true;
            bool download_success = true;
            std::string download_url;
            class Settings {
            public:
                Settings(const std::string& name, bool) { assert(name == "assets"); }
                std::string GetString(const std::string&) { return download_url; }
                void EraseKey(const std::string&) { download_url.clear(); }
            };
            class Display {
            public:
                void SetupUI() { ui_ready = true; }
                void SetChatMessage(const char*, const char*) {}
                void SetStatus(const char*) {}
                bool SetEmotion(const char*) { return true; }
                void UpdateStatusBar(bool) {}
                void ShowNotification(const char*, int) {
                    assert(font_loaded == (valid_partition && !pending_update));
                }
            };
            class Board {
                Display display_;
                std::function<void(NetworkEvent, const std::string&)> callback_;
            public:
                static Board& GetInstance() { static Board board; return board; }
                Display* GetDisplay() { return &display_; }
                int GetAudioCodec() { return 0; }
                void SetPowerSaveLevel(PowerSaveLevel) {}
                void SetNetworkEventCallback(decltype(callback_) callback) {
                    callback_ = callback;
                }
                void StartNetwork() {
                    operations.push_back("network");
                    network_started = true;
                    callback_(NetworkEvent::Connecting, "虾哥 iPhone");
                }
            };
            struct AudioServiceCallbacks {
                std::function<void()> on_send_queue_available, on_playback_drained;
                std::function<void(const std::string&)> on_wake_word_detected;
                std::function<void(bool)> on_vad_change;
                std::function<void(unsigned, unsigned)> on_playback_progress;
            };
            class AudioService {
            public:
                void Initialize(int) { audio_ready = true; }
                void Start() {}
                void SetCallbacks(const AudioServiceCallbacks&) {}
            };
            struct NotifyPlayer { void OnPlaybackProgress(unsigned, unsigned) {} };
            struct StateMachine {
                template <class T> void AddStateChangeListener(T) {}
            };
            struct SystemInfo {
                static void PrintHeapStats() {}
                static std::string GetUserAgent() { return "host"; }
            };
            class McpServer {
            public:
                static McpServer& GetInstance() { static McpServer server; return server; }
                void AddCommonTools() {}
                void AddUserOnlyTools() {}
                void LogToolInventory() {}
            };
            class Assets {
            public:
                static Assets& GetInstance() { static Assets assets; return assets; }
                bool partition_valid() { return valid_partition; }
                bool Apply() {
                    assert(ui_ready && audio_ready);
                    operations.push_back("apply");
                    font_loaded = true;
                    return true;
                }
                template <class T> bool Download(const std::string&, T progress) {
                    assert(network_started);
                    // Rendering from the partition while it is erased would be unsafe.
                    assert(!font_loaded);
                    operations.push_back("download");
                    progress(100, 1024);
                    return download_success;
                }
            };
            class Application {
            public:
                AudioService audio_service_;
                NotifyPlayer notify_player_;
                StateMachine state_machine_;
                int event_group_ = 0, clock_timer_handle_ = 0;
                bool assets_version_checked_ = false;
                void Initialize();
                void CheckAssetsVersion();
                void SetDeviceState(DeviceState) {}
                template <class... T> void Alert(T&&...) {}
                template <class T> void Schedule(T callback) { callback(); }
            };
        '''
        checks = r'''
            int main(int argc, char** argv) {
                assert(argc == 2);
                std::string mode = argv[1];
                pending_update = mode == "update" || mode == "failed-update";
                download_url = pending_update ? "https://example.test/assets.bin" : "";
                valid_partition = mode != "missing-assets";
                download_success = mode != "failed-update";
                Application app;
                app.Initialize();
                if (pending_update || !valid_partition) {
                    assert(operations == std::vector<std::string>{"network"});
                } else {
                    assert((operations == std::vector<std::string>{"apply", "network"}));
                }

                // Activation after connecting must not reload ordinary startup assets.
                app.CheckAssetsVersion();
                if (pending_update) {
                    auto expected = std::vector<std::string>{"network", "download"};
                    if (download_success) expected.push_back("apply");
                    assert(operations == expected);
                    assert(download_url.empty());
                    assert(font_loaded == download_success);
                }
                auto completed = operations;
                app.CheckAssetsVersion();
                assert(operations == completed);
            }
        '''
        source = path / "startup_assets.cc"
        source.write_text(textwrap.dedent(platform) + methods + textwrap.dedent(checks))
        cls.executable = path / "startup_assets"
        compiled = subprocess.run(
            compiler + ["-std=c++17", str(source), "-o", str(cls.executable)],
            capture_output=True, text=True,
        )
        if compiled.returncode:
            raise AssertionError(compiled.stdout + compiled.stderr)

    def test_fonts_are_available_to_the_first_connection_notification_without_reloading(self):
        self.run_scenario("startup")

    def test_pending_update_defers_loading_until_download_completes(self):
        self.run_scenario("update")

    def test_failed_update_keeps_the_builtin_font_available(self):
        self.run_scenario("failed-update")

    def test_missing_assets_do_not_prevent_network_startup(self):
        self.run_scenario("missing-assets")

    def run_scenario(self, name):
        result = subprocess.run([str(self.executable), name], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
