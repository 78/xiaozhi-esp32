#include "dual_network_board.h"
#include "codecs/box_audio_codec.h"
#include "display/oled_display.h"
#include "application.h"
#include "button.h"
#include "led/single_led.h"
#include "config.h"
#include "power_save_timer.h"
#include "axp2101.h"
#include "assets/lang_config.h"
#include "settings.h"
#include "local_control_panel.h"
#include "secure_maintenance_ap.h"
#include "system_info.h"
#include "device_state_machine.h"

#include <atomic>
#include <memory>
#include <cJSON.h>
#include <esp_log.h>
#include <esp_app_desc.h>
#include <esp_timer.h>
#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <ssid_manager.h>
#include <wifi_manager.h>

#define TAG "KevinBoxBoard"

class Pmic : public Axp2101 {
public:
    Pmic(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : Axp2101(i2c_bus, addr) {
        // ** EFUSE defaults **
        WriteReg(0x22, 0b110); // PWRON > OFFLEVEL as POWEROFF Source enable
        WriteReg(0x27, 0x10);  // hold 4s to power off
    
        WriteReg(0x93, 0x1C); // 配置 aldo2 输出为 3.3V
    
        uint8_t value = ReadReg(0x90); // XPOWERS_AXP2101_LDO_ONOFF_CTRL0
        value = value | 0x02; // set bit 1 (ALDO2)
        WriteReg(0x90, value);  // and power channels now enabled
    
        WriteReg(0x64, 0x03); // CV charger voltage setting to 4.2V
        
        WriteReg(0x61, 0x05); // set Main battery precharge current to 125mA
        WriteReg(0x62, 0x0A); // set Main battery charger current to 400mA ( 0x08-200mA, 0x09-300mA, 0x0A-400mA )
        WriteReg(0x63, 0x15); // set Main battery term charge current to 125mA
    
        WriteReg(0x14, 0x00); // set minimum system voltage to 4.1V (default 4.7V), for poor USB cables
        WriteReg(0x15, 0x00); // set input voltage limit to 3.88v, for poor USB cables
        WriteReg(0x16, 0x05); // set input current limit to 2000mA
    
        WriteReg(0x24, 0x01); // set Vsys for PWROFF threshold to 3.2V (default - 2.6V and kill battery)
        WriteReg(0x50, 0x14); // set TS pin to EXTERNAL input (not temperature)
    }
};

class KevinBoxBoard : public DualNetworkBoard {
private:
    i2c_master_bus_handle_t display_i2c_bus_;
    i2c_master_bus_handle_t codec_i2c_bus_;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    Display* display_ = nullptr;
    Pmic* pmic_ = nullptr;
    Button boot_button_;
    Button volume_up_button_;
    Button volume_down_button_;
    PowerSaveTimer* power_save_timer_ = nullptr;
    std::unique_ptr<LocalControlPanel> local_panel_;
    std::unique_ptr<SecureMaintenanceAccessPoint> maintenance_ap_;
    esp_timer_handle_t panel_monitor_timer_ = nullptr;
    std::atomic<bool> panel_reconcile_pending_{false};
    std::atomic<bool> maintenance_pending_{false};

    cJSON* BuildLocalPanelStatus() {
        auto* root = cJSON_CreateObject();
        const auto* app_desc = esp_app_get_description();
        auto* firmware = cJSON_CreateObject();
        cJSON_AddStringToObject(firmware, "version", app_desc->version);
        cJSON_AddStringToObject(firmware, "idf_version", app_desc->idf_ver);
        char sha256[65] = {};
        for (size_t i = 0; i < sizeof(app_desc->app_elf_sha256); ++i) {
            snprintf(sha256 + i * 2, sizeof(sha256) - i * 2, "%02x",
                     app_desc->app_elf_sha256[i]);
        }
        cJSON_AddStringToObject(firmware, "commit", sha256);
        cJSON_AddItemToObject(root, "firmware", firmware);

        auto& app = Application::GetInstance();
        cJSON_AddNumberToObject(root, "uptime_seconds", esp_timer_get_time() / 1000000);
        cJSON_AddStringToObject(root, "device_state",
                                DeviceStateMachine::GetStateName(app.GetDeviceState()));
        cJSON_AddNumberToObject(root, "wifi_rssi", GetWifiRssi());
        cJSON_AddNumberToObject(root, "cellular_csq", GetCellularSignalQuality());
        cJSON_AddNumberToObject(root, "free_heap", SystemInfo::GetFreeHeapSize());
        cJSON_AddNumberToObject(root, "minimum_free_heap",
                                SystemInfo::GetMinimumFreeHeapSize());
#if CONFIG_USE_AFE_WAKE_WORD
#if CONFIG_ENABLE_LOCAL_COMMANDS
        cJSON_AddStringToObject(root, "wake_engine", "WakeNet + MultiNet");
#else
        cJSON_AddStringToObject(root, "wake_engine", "WakeNet");
#endif
#else
        cJSON_AddStringToObject(root, "wake_engine", "disabled");
#endif
        cJSON_AddStringToObject(root, "recent_error",
                                app.GetLastErrorMessage().empty() ? "" : "network_or_protocol_error");

        int battery_level = 0;
        bool charging = false;
        bool discharging = false;
        auto* battery = cJSON_CreateObject();
        if (GetBatteryLevel(battery_level, charging, discharging)) {
            cJSON_AddNumberToObject(battery, "level", battery_level);
            cJSON_AddBoolToObject(battery, "charging", charging);
            cJSON_AddBoolToObject(battery, "discharging", discharging);
        }
        cJSON_AddItemToObject(root, "battery", battery);

        const auto status = GetNetworkController()->GetStatus();
        auto* network = cJSON_CreateObject();
        cJSON_AddStringToObject(network, "mode", ToString(status.mode));
        cJSON_AddStringToObject(network, "active", ToString(status.active));
        cJSON_AddStringToObject(network, "candidate", ToString(status.candidate));
        cJSON_AddStringToObject(network, "wifi_health", ToString(status.wifi_health));
        cJSON_AddStringToObject(network, "cellular_health", ToString(status.cellular_health));
        cJSON_AddStringToObject(network, "last_switch_reason",
                                ToString(status.last_switch_reason));
        cJSON_AddNumberToObject(network, "cellular_start_failures",
                                status.cellular_start_failures);
        cJSON_AddBoolToObject(network, "cellular_retry_limited",
                              status.cellular_retry_limited);
        cJSON_AddBoolToObject(network, "cellular_sim_missing",
                              status.cellular_sim_missing);
        cJSON_AddBoolToObject(network, "offline", status.offline);
        cJSON_AddItemToObject(root, "network", network);
        return root;
    }

    void ReconcileLocalPanel() {
        if (!local_panel_ || (maintenance_ap_ && maintenance_ap_->IsRunning())) {
            return;
        }
        const auto status = GetNetworkController()->GetStatus();
        const bool wifi_active = status.active == NetworkTransport::Wifi &&
                                 (status.wifi_health == NetworkHealth::LinkUp ||
                                  status.wifi_health == NetworkHealth::InternetReady) &&
                                 WifiManager::GetInstance().IsConnected();
        const bool should_run = wifi_active && local_panel_->IsLanEnabled() &&
                                local_panel_->HasAdminPassword();
        if (should_run) {
            local_panel_->Start(LocalControlPanel::Exposure::Lan);
        } else {
            // In particular, stop the listening socket before cellular becomes active so the
            // management surface is never bound while 4G is the selected WAN transport.
            local_panel_->Stop();
        }
    }

    void StartPanelMonitor() {
        if (panel_monitor_timer_ == nullptr) {
            esp_timer_create_args_t timer_args = {
                .callback = [](void* arg) {
                    auto* board = static_cast<KevinBoxBoard*>(arg);
                    bool expected = false;
                    if (!board->panel_reconcile_pending_.compare_exchange_strong(expected, true)) {
                        return;
                    }
                    Application::GetInstance().Schedule([board]() {
                        board->panel_reconcile_pending_ = false;
                        board->ReconcileLocalPanel();
                    });
                },
                .arg = this,
                .dispatch_method = ESP_TIMER_TASK,
                .name = "local_panel",
                .skip_unhandled_events = true,
            };
            ESP_ERROR_CHECK(esp_timer_create(&timer_args, &panel_monitor_timer_));
        }
        if (!esp_timer_is_active(panel_monitor_timer_)) {
            ESP_ERROR_CHECK(esp_timer_start_periodic(panel_monitor_timer_, 2'000'000));
        }
    }

    void StopPanelMonitor() {
        panel_reconcile_pending_ = false;
        if (panel_monitor_timer_ != nullptr && esp_timer_is_active(panel_monitor_timer_)) {
            esp_timer_stop(panel_monitor_timer_);
        }
    }

    void InitializeLocalPanel() {
        local_panel_ = std::make_unique<LocalControlPanel>(
            [this]() { return BuildLocalPanelStatus(); },
            [this](NetworkMode mode) { return SetNetworkMode(mode); },
            [this](const LocalPanelSettingsUpdate& update, std::string& error) {
                (void)error;
                if (update.has_lan_enabled) {
                    Settings settings("local_panel", true);
                    settings.SetBool("lan_enabled", update.lan_enabled);
                }
                if (update.has_speaker_volume) {
                    GetAudioCodec()->SetOutputVolume(update.speaker_volume);
                }
                if (update.has_wifi_credentials) {
                    SsidManager::GetInstance().AddSsid(update.wifi_ssid,
                                                       update.wifi_password);
                }
                if (update.restart_requested) {
                    Application::GetInstance().Schedule(
                        []() { Application::GetInstance().Reboot(); });
                }
                return true;
            });
        maintenance_ap_ = std::make_unique<SecureMaintenanceAccessPoint>(*local_panel_);
        SetWifiConfigModeHandler([this]() {
            bool expected = false;
            if (!maintenance_pending_.compare_exchange_strong(expected, true)) {
                return;
            }
            Application::GetInstance().Schedule([this]() { EnterMaintenanceMode(); });
        });
    }

    void InitializePowerSaveTimer() {
        power_save_timer_ = new PowerSaveTimer(-1, -1, 600);
        power_save_timer_->OnShutdownRequest([this]() {
            pmic_->PowerOff();
        });
        power_save_timer_->SetEnabled(true);
    }

    void Initialize4GPowerControl() {
        gpio_config_t ml307_enable_config = {
            .pin_bit_mask = (1ULL << 4),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&ml307_enable_config);
        gpio_set_level(GPIO_NUM_4, 0);
        SetCellularPowerControl([](bool enabled) {
            ESP_LOGI(TAG, "Turning cellular module %s", enabled ? "on" : "off");
            return gpio_set_level(GPIO_NUM_4, enabled ? 1 : 0) == ESP_OK;
        });
        SetExternalPowerProvider([this]() { return !pmic_->IsDischarging(); });
    }

    void InitializeDisplayI2c() {
        i2c_master_bus_config_t bus_config = {
            .i2c_port = (i2c_port_t)0,
            .sda_io_num = DISPLAY_SDA_PIN,
            .scl_io_num = DISPLAY_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &display_i2c_bus_));
    }

    void InitializeSsd1306Display() {
        // SSD1306 config
        esp_lcd_panel_io_i2c_config_t io_config = {
            .dev_addr = 0x3C,
            .scl_speed_hz = 400 * 1000,
            .control_phase_bytes = 1,
            .dc_bit_offset = 6,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .on_color_trans_done = nullptr,
            .user_ctx = nullptr,
            .flags = {
                .dc_low_on_data = 0,
                .disable_control_phase = 0,
            },
        };

        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(display_i2c_bus_, &io_config, &panel_io_));

        ESP_LOGI(TAG, "Install SSD1306 driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.bits_per_pixel = 1;

        esp_lcd_panel_ssd1306_config_t ssd1306_config = {
            .height = static_cast<uint8_t>(DISPLAY_HEIGHT),
        };
        panel_config.vendor_config = &ssd1306_config;

        ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(panel_io_, &panel_config, &panel_));
        ESP_LOGI(TAG, "SSD1306 driver installed");

        // Reset the display
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        if (esp_lcd_panel_init(panel_) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize display");
            display_ = new NoDisplay();
            return;
        }

        // Set the display to on
        ESP_LOGI(TAG, "Turning display on");
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

        display_ = new OledDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
    }

    void InitializeCodecI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)1,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
    }

    void InitializeButtons() {
        boot_button_.OnPressDown([this]() {
            power_save_timer_->WakeUp();
            auto& app = Application::GetInstance();
            app.StartListening();
        });
        boot_button_.OnPressUp([this]() {
            auto& app = Application::GetInstance();
            app.StopListening();
        });
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting ||
                app.GetDeviceState() == kDeviceStateWifiConfiguring) {
                EnterMaintenanceMode();
            }
        });
        boot_button_.OnDoubleClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting || app.GetDeviceState() == kDeviceStateWifiConfiguring) {
                SetNetworkMode(NetworkMode::Auto);
                GetDisplay()->ShowNotification("Network mode: AUTO");
            }
        });
        boot_button_.OnMultipleClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting ||
                app.GetDeviceState() == kDeviceStateWifiConfiguring) {
                local_panel_->ResetAdminPassword();
                EnterMaintenanceMode();
            }
        }, 3);

        volume_up_button_.OnClick([this]() {
            power_save_timer_->WakeUp();
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;
            if (volume > 100) {
                volume = 100;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        volume_up_button_.OnLongPress([this]() {
            power_save_timer_->WakeUp();
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting ||
                app.GetDeviceState() == kDeviceStateWifiConfiguring) {
                SetNetworkMode(NetworkMode::Wifi);
                GetDisplay()->ShowNotification("Network mode: WiFi");
                return;
            }
            GetAudioCodec()->SetOutputVolume(100);
            GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);
        });

        volume_down_button_.OnClick([this]() {
            power_save_timer_->WakeUp();
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        volume_down_button_.OnLongPress([this]() {
            power_save_timer_->WakeUp();
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting ||
                app.GetDeviceState() == kDeviceStateWifiConfiguring) {
                SetNetworkMode(NetworkMode::Cellular);
                GetDisplay()->ShowNotification("Network mode: 4G");
                return;
            }
            GetAudioCodec()->SetOutputVolume(0);
            GetDisplay()->ShowNotification(Lang::Strings::MUTED);
        });
    }

public:
    KevinBoxBoard() : DualNetworkBoard(ML307_TX_PIN, ML307_RX_PIN),
        boot_button_(BOOT_BUTTON_GPIO),
        volume_up_button_(VOLUME_UP_BUTTON_GPIO),
        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO) {
        InitializeDisplayI2c();
        InitializeSsd1306Display();
        InitializeCodecI2c();
        pmic_ = new Pmic(codec_i2c_bus_, AXP2101_I2C_ADDR);

        Initialize4GPowerControl();

        InitializePowerSaveTimer();
        InitializeLocalPanel();
        InitializeButtons();
    }

    void EnterMaintenanceMode() override {
        maintenance_pending_ = false;
        if (maintenance_ap_ && maintenance_ap_->IsRunning()) {
            const std::string message = "SSID: " + maintenance_ap_->GetSsid() +
                                        "\n密码: " + maintenance_ap_->GetPassword() +
                                        "\nhttp://192.168.4.1";
            GetDisplay()->SetStatus("维护热点");
            GetDisplay()->SetChatMessage("system", message.c_str());
            return;
        }

        auto& app = Application::GetInstance();
        app.ResetProtocol();
        StopPanelMonitor();
        if (local_panel_) {
            local_panel_->Stop();
        }
        DualNetworkBoard::StopNetwork();
        app.SetDeviceState(kDeviceStateWifiConfiguring);
        if (!maintenance_ap_ || !maintenance_ap_->Start()) {
            GetDisplay()->ShowNotification("维护热点启动失败");
            DualNetworkBoard::StartNetwork();
            StartPanelMonitor();
            return;
        }

        const std::string message = "SSID: " + maintenance_ap_->GetSsid() +
                                    "\n密码: " + maintenance_ap_->GetPassword() +
                                    "\nhttp://192.168.4.1";
        GetDisplay()->SetStatus("维护热点");
        GetDisplay()->SetChatMessage("system", message.c_str());
    }

    void StartNetwork() override {
        if (maintenance_ap_ && maintenance_ap_->IsRunning()) {
            maintenance_ap_->Stop();
        }
        DualNetworkBoard::StartNetwork();
        StartPanelMonitor();
    }

    bool StopNetwork() override {
        StopPanelMonitor();
        if (local_panel_ && (!maintenance_ap_ || !maintenance_ap_->IsRunning())) {
            local_panel_->Stop();
        }
        return DualNetworkBoard::StopNetwork();
    }

    void OnNetworkSwitching(NetworkTransport target, NetworkSwitchReason reason) override {
        if (local_panel_ && (!maintenance_ap_ || !maintenance_ap_->IsRunning())) {
            local_panel_->Stop();
        }
        const auto status = GetNetworkController()->GetStatus();
        const std::string message = std::string(ToString(status.active)) + " → " +
                                    ToString(target) + "\n原因: " + ToString(reason);
        GetDisplay()->SetChatMessage("system", message.c_str());
    }

    std::string GetIdleStatusText() override {
        if (Application::GetInstance().GetDeviceState() != kDeviceStateIdle) {
            return {};
        }
        const auto status = GetNetworkController()->GetStatus();
        if (status.offline || status.active == NetworkTransport::None) {
            return "离线 · 唤醒就绪";
        }
        return std::string(status.active == NetworkTransport::Wifi ? "Wi-Fi" : "4G") +
               " · 唤醒就绪";
    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static BoxAudioCodec audio_codec(codec_i2c_bus_, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR, AUDIO_CODEC_ES7210_ADDR, AUDIO_INPUT_REFERENCE);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual bool GetBatteryLevel(int &level, bool& charging, bool& discharging) override {
        static bool last_discharging = false;
        charging = pmic_->IsCharging();
        discharging = pmic_->IsDischarging();
        if (discharging != last_discharging) {
            power_save_timer_->SetEnabled(discharging);
            RefreshNetworkPowerPolicy();
            last_discharging = discharging;
        }

        level = pmic_->GetBatteryLevel();
        return true;
    }
};

DECLARE_BOARD(KevinBoxBoard);
