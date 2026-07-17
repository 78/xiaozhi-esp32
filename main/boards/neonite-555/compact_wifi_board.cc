#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "choreo_audio_codec.h"
#include "display/oled_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "lamp_controller.h"
#include "led/single_led.h"
#include "assets/lang_config.h"
#include "dog_control.h"
#include "choreo.h"
#include "websocket_control_server.h"
#include "ble_control_server.h"
#include <wifi_manager.h>

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_netif.h>
#include <esp_event.h>

#ifdef SH1106
#include <esp_lcd_panel_sh1106.h>
#endif

#define TAG "CompactWifiBoard"

/* choreo 音频回调前向声明（实现在文件末尾 extern "C" 块中）*/
extern "C" {
    void choreo_audio_ctrl_bridge(bool enable);
    int  choreo_audio_write_bridge(const int16_t* data, int samples);
}

class CompactWifiBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t display_i2c_bus_;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    Display* display_ = nullptr;
    Button boot_button_;
    Button touch_button_;
    Button volume_up_button_;
    Button volume_down_button_;
    WebSocketControlServer* ws_server_ = nullptr;  // 本地 WebSocket 服务器
    BleControlServer* ble_server_ = nullptr;        // 本地 BLE MCP 服务器
    NetworkEventCallback external_network_callback_ = nullptr;  // Application 等外部回调

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
            .on_color_trans_done = nullptr,
            .user_ctx = nullptr,
            .control_phase_bytes = 1,
            .dc_bit_offset = 6,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .flags = {
                .dc_low_on_data = 0,
                .disable_control_phase = 0,
            },
            .scl_speed_hz = 400 * 1000,
        };

        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c_v2(display_i2c_bus_, &io_config, &panel_io_));

        ESP_LOGI(TAG, "Install SSD1306 driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = -1;
        panel_config.bits_per_pixel = 1;

        esp_lcd_panel_ssd1306_config_t ssd1306_config = {
            .height = static_cast<uint8_t>(DISPLAY_HEIGHT),
        };
        panel_config.vendor_config = &ssd1306_config;

#ifdef SH1106
        ESP_ERROR_CHECK(esp_lcd_new_panel_sh1106(panel_io_, &panel_config, &panel_));
#else
        ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(panel_io_, &panel_config, &panel_));
#endif
        ESP_LOGI(TAG, "SSD1306 driver installed");

        // Reset the display
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        if (esp_lcd_panel_init(panel_) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize display");
            display_ = new NoDisplay();
            return;
        }
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_, false));

        // Set the display to on
        ESP_LOGI(TAG, "Turning display on");
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

        display_ = new OledDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
        touch_button_.OnPressDown([this]() {
            Application::GetInstance().StartListening();
        });
        touch_button_.OnPressUp([this]() {
            Application::GetInstance().StopListening();
        });

        volume_up_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;
            if (volume > 100) {
                volume = 100;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        volume_up_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(100);
            GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);
        });

        volume_down_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        volume_down_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(0);
            GetDisplay()->ShowNotification(Lang::Strings::MUTED);
        });
    }

    // 物联网初始化，逐步迁移到 MCP 协议
    void InitializeTools() {
        static LampController lamp(LAMP_GPIO);
    }

public:
    // 串联 BLE 逻辑和外部回调（防止 Application 覆盖我们的回调）
    void SetNetworkEventCallback(NetworkEventCallback callback) override {
        external_network_callback_ = callback;
        WifiBoard::SetNetworkEventCallback([this](NetworkEvent event, const std::string& data) {
            // BLE 内存管理：配网前释放，配网后重建
            if (event == NetworkEvent::WifiConfigModeEnter) {
                ESP_LOGI(TAG, "WifiConfigModeEnter: freeing BLE memory");
                if (ble_server_) {
                    ble_server_->Deinit();
                }
            } else if (event == NetworkEvent::WifiConfigModeExit) {
                ESP_LOGI(TAG, "WifiConfigModeExit: re-initializing BLE");
                if (ble_server_) {
                    ble_server_->Reinit();
                }
            }
            // 调用外部回调（Application 状态机）
            if (external_network_callback_) {
                external_network_callback_(event, data);
            }
        });
    }

    CompactWifiBoard() :
        boot_button_(BOOT_BUTTON_GPIO),
        touch_button_(TOUCH_BUTTON_GPIO),
        volume_up_button_(VOLUME_UP_BUTTON_GPIO),
        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO) {
        // 网络栈初始化（esp_netif_init 只能调用一次，放在 Board 构造函数中）
        static bool net_initialized = false;
        if (!net_initialized) {
            ESP_ERROR_CHECK(esp_event_loop_create_default());
            esp_netif_init();
            net_initialized = true;
        }

        InitializeDisplayI2c();
        InitializeSsd1306Display();
        InitializeButtons();
        InitializeTools();

        // 初始化舵机（来自 weixintest2）
        Dog_ServoInit();

        // 初始化舞蹈编排模块（mmap assets 分区）
        if (choreo_init() != ESP_OK) {
            ESP_LOGW(TAG, "舞蹈编排模块初始化失败，自定义舞步功能不可用");
        }

        // 注册音频回调：choreo 不再自管 I2S，直接走 codec 输出
        // （避免和 NoAudioCodecSimplex 的 I2S_NUM_1 mic 通道冲突）
        choreo_set_audio_callbacks(choreo_audio_ctrl_bridge, choreo_audio_write_bridge);

        // 注册 MCP 工具
        InitMachineDog();

        // 启动本地 WebSocket 服务器（端口 8765）
        ws_server_ = new WebSocketControlServer();
        if (!ws_server_->Start(8765)) {
            ESP_LOGE(TAG, "WebSocket 服务器启动失败");
            delete ws_server_;
            ws_server_ = nullptr;
        } else {
            ESP_LOGI(TAG, "WebSocket 服务器已启动，端口 8765");
        }

        // 启动本地 BLE 服务器（NimBLE GATT）
        ble_server_ = new BleControlServer();
        if (!ble_server_->Start()) {
            ESP_LOGW(TAG, "BLE 服务器启动失败（可能 BLE 未在 sdkconfig 中启用）");
            delete ble_server_;
            ble_server_ = nullptr;
        } else {
            ESP_LOGI(TAG, "BLE 服务器已启动，广播名 Xiaozhi-Dog");
        }

        // ── 互斥逻辑：WS 和 BLE 连接后自动释放另一方内存 ──────────────
        // 注意：Deinit/Reinit/Stop/Start 均涉及 FreeRTOS 任务删除/创建，
        //       不能在 httpd / NimBLE 回调上下文里同步调用，
        //       必须 offload 到独立一次性任务执行，避免 vTaskDelete 崩溃。
        if (ws_server_ && ble_server_) {
            ws_server_->SetOnClientConnectedCallback([this]() {
                ESP_LOGI(TAG, "WS connected: scheduling BLE deinit");
                BleControlServer* ble = ble_server_;
                xTaskCreate([](void* arg) {
                    auto* ble = static_cast<BleControlServer*>(arg);
                    ESP_LOGI("Mutex", "BLE Deinit task running");
                    ble->Deinit();
                    vTaskDelete(nullptr);
                }, "ble_deinit", 4096, ble, 5, nullptr);
            });
            // WebSocket 最后一个客户端断开 → Reinit BLE
            ws_server_->SetOnAllClientsDisconnectedCallback([this]() {
                ESP_LOGI(TAG, "WS disconnected: scheduling BLE reinit");
                BleControlServer* ble = ble_server_;
                xTaskCreate([](void* arg) {
                    auto* ble = static_cast<BleControlServer*>(arg);
                    ESP_LOGI("Mutex", "BLE Reinit task running");
                    ble->Reinit();
                    vTaskDelete(nullptr);
                }, "ble_reinit", 4096, ble, 5, nullptr);
            });
            // BLE 客户端连接 → 停止 WebSocket server
            ble_server_->SetOnConnectedCallback([this]() {
                ESP_LOGI(TAG, "BLE connected: scheduling WS stop");
                WebSocketControlServer* ws = ws_server_;
                xTaskCreate([](void* arg) {
                    auto* ws = static_cast<WebSocketControlServer*>(arg);
                    ESP_LOGI("Mutex", "WS Stop task running");
                    ws->Stop();
                    vTaskDelete(nullptr);
                }, "ws_stop", 4096, ws, 5, nullptr);
            });
            // BLE 客户端断开 → 停止 WS 后延时再启动
            ble_server_->SetOnDisconnectedCallback([this]() {
                ESP_LOGI(TAG, "BLE disconnected: scheduling WS stop then start");
                WebSocketControlServer* ws = ws_server_;
                xTaskCreate([](void* arg) {
                    auto* ws = static_cast<WebSocketControlServer*>(arg);
                    ws->Stop();
                    vTaskDelay(pdMS_TO_TICKS(2000));  // 等 httpd_stop 释放端口 + TIME_WAIT
                    ESP_LOGI("Mutex", "WS Start after stop+delay");
                    ws->Start(8765);
                    vTaskDelete(nullptr);
                }, "ws_restart", 4096, ws, 5, nullptr);
            });
        }
        // ────────────────────────────────────────────────────────────────

        // 注意：BLE Deinit/Reinit 逻辑已在 SetNetworkEventCallback override 中，
        // 由 Application 初始化时触发，无需在这里重复注册
    }

    void StartNetwork() override {
        WifiBoard::StartNetwork();
        // 轮询获取 IP 后在第二行滚动显示（避开第一行 WiFi/音量/电池图标）
        xTaskCreate([](void* arg) {
            Display* display = static_cast<Display*>(arg);
            auto& wifi = WifiManager::GetInstance();
            std::string ip;
            while (ip.empty()) {
                vTaskDelay(pdMS_TO_TICKS(1000));
                ip = wifi.GetIpAddress();
            }
            // 等小智初始化完毕再显示
            vTaskDelay(pdMS_TO_TICKS(10000));
            ESP_LOGI(TAG, "WiFi IP: %s", ip.c_str());
            std::string text = "IP: " + ip;
            display->SetChatMessage("system", text.c_str());
            vTaskDelay(pdMS_TO_TICKS(8000));
            display->SetChatMessage("system", "");
            vTaskDelete(NULL);
        }, "show_ip", 4096, display_, 1, NULL);
    }

    ~CompactWifiBoard() {
        if (ws_server_) {
            ws_server_->Stop();
            delete ws_server_;
            ws_server_ = nullptr;
        }
        if (ble_server_) {
            ble_server_->Stop();
            delete ble_server_;
            ble_server_ = nullptr;
        }
    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
#ifdef AUDIO_I2S_METHOD_SIMPLEX
        static ChoreoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
#else
        static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
#endif
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    // ── 供 dog_control.cc 调用的 friend 函数 ──
    // 绕过 WifiBoard::EnterWifiConfigMode() 的设备状态检查，
    // 直接从 MCP 工具 handler 强制进入配网模式。
    friend void compact_board_force_wifi_config(Board* board);
};

// ── choreo 音频回调（C 桥接，定义在 DECLARE_BOARD 之前，确保构造函数可见）──
// 不使用 static，避免构造函数所在翻译单元找不到符号。
extern "C" {

/* 持久化 PCM 缓冲区（heap 分配，避免在 opus_task 的栈上构造/析构 vector）*/
static std::vector<int16_t> s_choreo_pcm_buf;
static int s_choreo_write_count = 0;  /* 诊断计数器 */

void choreo_audio_ctrl_bridge(bool enable) {
    auto* codec = Board::GetInstance().GetAudioCodec();
    if (!codec) return;

    /* choreo 活动标志必须先于 EnableOutput 设置：
     * - enable=true:  先置 active，再 EnableOutput(true)，放行开启
     * - enable=false: 先清 active，再 EnableOutput(false)，放行 choreo 自身的合法关闭
     *   （先清后调期间若 power timer 恰好触发，也只是关闭 I2S，choreo 即将结束，幂等无影响）*/
#ifdef AUDIO_I2S_METHOD_SIMPLEX
    static_cast<ChoreoAudioCodecSimplex*>(codec)->SetChoreoActive(enable);
#endif
    codec->EnableOutput(enable);
    if (!enable) {
        s_choreo_write_count = 0;  /* 重置诊断计数器 */
    }
}

}  // extern "C"

/* choreo_audio_write_bridge 定义在 extern "C" 外部，以便调用 C++ Board API。
 * choreo.h 中已有 extern "C" 声明，链接不受影响。
 * 不再调用 AudioService::TouchOutputTime()（该方法将从官方移除），
 * power timer 误判改由 ChoreoAudioCodecSimplex::EnableOutput 在板级拦截。 */
int choreo_audio_write_bridge(const int16_t* data, int samples) {
    auto* codec = Board::GetInstance().GetAudioCodec();
    if (!codec) return 0;

    if (s_choreo_pcm_buf.size() != (size_t)samples) {
        s_choreo_pcm_buf.resize(samples);
    }
    memcpy(s_choreo_pcm_buf.data(), data, samples * sizeof(int16_t));

    codec->OutputData(s_choreo_pcm_buf);

    s_choreo_write_count++;
    if (s_choreo_write_count <= 3 || s_choreo_write_count % 50 == 0) {
        ESP_LOGI("choreo_audio", "write #%d: %d samples", s_choreo_write_count, samples);
    }

    return samples;
}

DECLARE_BOARD(CompactWifiBoard);

// ── 强制进入配网模式（绕过状态检查）────────────────────────────
// 语音/远程 MCP 调用时设备状态可能不在 WifiBoard::EnterWifiConfigMode()
// 允许的 {Speaking, Listening, Idle} 集合内，导致静默失败。
// 此函数直接执行配网逻辑，不做状态检查。
void compact_board_force_wifi_config(Board* board_ptr) {
    auto* board = static_cast<CompactWifiBoard*>(board_ptr);
    ESP_LOGI(TAG, "ForceEnterWifiConfigMode: stopping WS/BLE before wifi config");

    auto& app = Application::GetInstance();
    app.ResetProtocol();

    // 停止 WebSocket HTTPD，释放 HTTPD 资源，避免 StartWifiConfigMode
    // 里的第二个 httpd_start 报 ESP_ERR_HTTPD_TASK 崩溃。
    if (board->ws_server_) {
        board->ws_server_->Stop();
    }

    xTaskCreate([](void* arg) {
        auto* b = static_cast<CompactWifiBoard*>(arg);
        vTaskDelay(pdMS_TO_TICKS(1000));

        esp_timer_stop(b->connect_timer_);
        WifiManager::GetInstance().StopStation();
        b->StartWifiConfigMode();

        vTaskDelete(NULL);
    }, "wifi_cfg_force", 4096, board, 2, NULL);
}
