#include "application.h"
#include "features/weather/weather_service.h"
#include "features/weather/weather_ui.h"
#include "features/weather/lunar_calendar.h"
#include "board.h"
#include "display.h"
#include "system_info.h"
#include "audio_codec.h"
#include "mqtt_protocol.h"
#include "websocket_protocol.h"
#include "assets/lang_config.h"
#include "mcp_server.h"
#include "assets.h"
#include "settings.h"
#include "ota_server.h"
#include "ota.h"
#include "music/esp32_radio.h"
#include "music/esp32_sd_music.h"
#include "sd_card.h"

#ifdef CONFIG_QUIZ_ENABLE
#include "features/quiz/quiz_manager.h"
#include "features/quiz/quiz_ui.h"
#endif

#include <cstring>
#include <esp_log.h>
#include <cJSON.h>
#include <driver/gpio.h>
#include <arpa/inet.h>
#include <font_awesome.h>
#include <thread>
#include <algorithm>

#define TAG "Application"

static const char *const STATE_STRINGS[] = {
    "unknown",
    "starting",
    "configuring",
    "idle",
    "connecting",
    "listening",
    "speaking",
    "upgrading",
    "activating",
    "audio_testing",
    "streaming",
    "quiz",
    "fatal_error",
    "invalid_state"};

Application::Application()
{
    event_group_ = xEventGroupCreate();

#if CONFIG_USE_DEVICE_AEC && CONFIG_USE_SERVER_AEC
#error "CONFIG_USE_DEVICE_AEC and CONFIG_USE_SERVER_AEC cannot be enabled at the same time"
#elif CONFIG_USE_DEVICE_AEC
    aec_mode_ = kAecOnDeviceSide;
#elif CONFIG_USE_SERVER_AEC
    aec_mode_ = kAecOnServerSide;
#else
    aec_mode_ = kAecOff;
#endif

    esp_timer_create_args_t clock_timer_args = {
        .callback = [](void *arg)
        {
            Application *app = (Application *)arg;
            xEventGroupSetBits(app->event_group_, MAIN_EVENT_CLOCK_TICK);
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "clock_timer",
        .skip_unhandled_events = true};
    esp_timer_create(&clock_timer_args, &clock_timer_handle_);

    // Initialize Radio and Music
    radio_ = std::make_unique<Esp32Radio>();
    radio_->Initialize();

    sd_music_ = std::make_unique<Esp32SdMusic>();
    // SdMusic initialized later when SD card is available or in Start()
    // For now we just instantiate it.
}

Application::~Application()
{
    if (clock_timer_handle_ != nullptr)
    {
        esp_timer_stop(clock_timer_handle_);
        esp_timer_delete(clock_timer_handle_);
    }
    vEventGroupDelete(event_group_);
}

void Application::CheckAssetsVersion()
{
    auto &board = Board::GetInstance();
    auto display = board.GetDisplay();
    auto &assets = Assets::GetInstance();

    if (!assets.partition_valid())
    {
        ESP_LOGW(TAG, "Assets partition is disabled for board %s", BOARD_NAME);
        return;
    }

    Settings settings("assets", true);
    // Check if there is a new assets need to be downloaded
    std::string download_url = settings.GetString("download_url");

    if (!download_url.empty())
    {
        settings.EraseKey("download_url");

        char message[256];
        snprintf(message, sizeof(message), Lang::Strings::FOUND_NEW_ASSETS, download_url.c_str());
        Alert(Lang::Strings::LOADING_ASSETS, message, "cloud_arrow_down", Lang::Sounds::OGG_UPGRADE);

        // Wait for the audio service to be idle for 3 seconds
        vTaskDelay(pdMS_TO_TICKS(3000));
        SetDeviceState(kDeviceStateUpgrading);
        board.SetPowerSaveMode(false);
        display->SetChatMessage("system", Lang::Strings::PLEASE_WAIT);

        bool success = assets.Download(download_url, [display](int progress, size_t speed) -> void
                                       { std::thread([display, progress, speed]()
                                                     {
                char buffer[32];
                snprintf(buffer, sizeof(buffer), "%d%% %uKB/s", progress, speed / 1024);
                display->SetChatMessage("system", buffer); })
                                             .detach(); });

        board.SetPowerSaveMode(true);
        vTaskDelay(pdMS_TO_TICKS(1000));

        if (!success)
        {
            Alert(Lang::Strings::ERROR, Lang::Strings::DOWNLOAD_ASSETS_FAILED, "circle_xmark", Lang::Sounds::OGG_EXCLAMATION);
            vTaskDelay(pdMS_TO_TICKS(2000));
            return;
        }
    }

    // Apply assets
    assets.Apply();
    display->SetChatMessage("system", "");
    display->SetEmotion("microchip_ai");
}

void Application::CheckNewVersion(Ota &ota)
{
    const int MAX_RETRY = 10;
    int retry_count = 0;
    int retry_delay = 10; // 初始重试延迟为10秒

    auto &board = Board::GetInstance();
    while (true)
    {
        SetDeviceState(kDeviceStateActivating);
        auto display = board.GetDisplay();
        display->SetStatus(Lang::Strings::CHECKING_NEW_VERSION);

        std::string url;
        if (!ota.CheckVersion(url))
        {
            retry_count++;
            if (retry_count >= MAX_RETRY)
            {
                ESP_LOGE(TAG, "Too many retries, exit version check");
                return;
            }

            char buffer[256];
            snprintf(buffer, sizeof(buffer), Lang::Strings::CHECK_NEW_VERSION_FAILED, retry_delay, ota.GetCheckVersionUrl().c_str());
            Alert(Lang::Strings::ERROR, buffer, "cloud_slash", Lang::Sounds::OGG_EXCLAMATION);

            ESP_LOGW(TAG, "Check new version failed, retry in %d seconds (%d/%d)", retry_delay, retry_count, MAX_RETRY);
            for (int i = 0; i < retry_delay; i++)
            {
                vTaskDelay(pdMS_TO_TICKS(1000));
                if (device_state_ == kDeviceStateIdle)
                {
                    break;
                }
            }
            retry_delay *= 2; // 每次重试后延迟时间翻倍
            continue;
        }
        retry_count = 0;
        retry_delay = 10; // 重置重试延迟时间

        if (ota.HasNewVersion())
        {
            if (UpgradeFirmware(ota))
            {
                return; // This line will never be reached after reboot
            }
            // If upgrade failed, continue to normal operation (don't break, just fall through)
        }

        // No new version, mark the current version as valid
        ota.MarkCurrentVersionValid();
        if (!ota.HasActivationCode() && !ota.HasActivationChallenge())
        {
            xEventGroupSetBits(event_group_, MAIN_EVENT_CHECK_NEW_VERSION_DONE);
            // Exit the loop if done checking new version
            break;
        }

        display->SetStatus(Lang::Strings::ACTIVATION);
        // Activation code is shown to the user and waiting for the user to input
        if (ota.HasActivationCode())
        {
            ShowActivationCode(ota.GetActivationCode(), ota.GetActivationMessage());
        }

        // This will block the loop until the activation is done or timeout
        for (int i = 0; i < 10; ++i)
        {
            ESP_LOGI(TAG, "Activating... %d/%d", i + 1, 10);
            esp_err_t err = ota.Activate();
            if (err == ESP_OK)
            {
                xEventGroupSetBits(event_group_, MAIN_EVENT_CHECK_NEW_VERSION_DONE);
                break;
            }
            else if (err == ESP_ERR_TIMEOUT)
            {
                vTaskDelay(pdMS_TO_TICKS(3000));
            }
            else
            {
                vTaskDelay(pdMS_TO_TICKS(10000));
            }
            if (device_state_ == kDeviceStateIdle)
            {
                break;
            }
        }
    }
}

void Application::ShowActivationCode(const std::string &code, const std::string &message)
{
    struct digit_sound
    {
        char digit;
        const std::string_view &sound;
    };
    static const std::array<digit_sound, 10> digit_sounds{{digit_sound{'0', Lang::Sounds::OGG_0},
                                                           digit_sound{'1', Lang::Sounds::OGG_1},
                                                           digit_sound{'2', Lang::Sounds::OGG_2},
                                                           digit_sound{'3', Lang::Sounds::OGG_3},
                                                           digit_sound{'4', Lang::Sounds::OGG_4},
                                                           digit_sound{'5', Lang::Sounds::OGG_5},
                                                           digit_sound{'6', Lang::Sounds::OGG_6},
                                                           digit_sound{'7', Lang::Sounds::OGG_7},
                                                           digit_sound{'8', Lang::Sounds::OGG_8},
                                                           digit_sound{'9', Lang::Sounds::OGG_9}}};

    // This sentence uses 9KB of SRAM, so we need to wait for it to finish
    Alert(Lang::Strings::ACTIVATION, message.c_str(), "link", Lang::Sounds::OGG_ACTIVATION);

    for (const auto &digit : code)
    {
        auto it = std::find_if(digit_sounds.begin(), digit_sounds.end(),
                               [digit](const digit_sound &ds)
                               { return ds.digit == digit; });
        if (it != digit_sounds.end())
        {
            audio_service_.PlaySound(it->sound);
        }
    }
}

void Application::Alert(const char *status, const char *message, const char *emotion, const std::string_view &sound)
{
    ESP_LOGW(TAG, "Alert [%s] %s: %s", emotion, status, message);
    auto display = Board::GetInstance().GetDisplay();
    display->SetStatus(status);
    display->SetEmotion(emotion);
    display->SetChatMessage("system", message);
    if (!sound.empty())
    {
        audio_service_.PlaySound(sound);
    }
}

void Application::DismissAlert()
{
    if (device_state_ == kDeviceStateIdle)
    {
        auto display = Board::GetInstance().GetDisplay();
        display->SetStatus(Lang::Strings::STANDBY);
        display->SetEmotion("neutral");
        display->SetChatMessage("system", "");
    }
}

void Application::ToggleChatState()
{
    if (device_state_ == kDeviceStateActivating)
    {
        SetDeviceState(kDeviceStateIdle);
        return;
    }
    else if (device_state_ == kDeviceStateWifiConfiguring)
    {
        audio_service_.EnableAudioTesting(true);
        SetDeviceState(kDeviceStateAudioTesting);
        return;
    }
    else if (device_state_ == kDeviceStateAudioTesting)
    {
        audio_service_.EnableAudioTesting(false);
        SetDeviceState(kDeviceStateWifiConfiguring);
        return;
    }

    if (!protocol_)
    {
        ESP_LOGE(TAG, "Protocol not initialized");
        return;
    }

    if (device_state_ == kDeviceStateIdle)
    {
        Schedule([this]()
                 {
            if (!protocol_->IsAudioChannelOpened()) {
                SetDeviceState(kDeviceStateConnecting);
                if (!protocol_->OpenAudioChannel()) {
                    return;
                }
            }

            SetListeningMode(aec_mode_ == kAecOff ? kListeningModeAutoStop : kListeningModeRealtime); });
    }
    else if (device_state_ == kDeviceStateSpeaking)
    {
        Schedule([this]()
                 { AbortSpeaking(kAbortReasonNone); });
    }
    else if (device_state_ == kDeviceStateListening)
    {
        Schedule([this]()
                 { protocol_->CloseAudioChannel(); });
    }
}

void Application::StartListening()
{
    if (device_state_ == kDeviceStateActivating)
    {
        SetDeviceState(kDeviceStateIdle);
        return;
    }
    else if (device_state_ == kDeviceStateWifiConfiguring)
    {
        audio_service_.EnableAudioTesting(true);
        SetDeviceState(kDeviceStateAudioTesting);
        return;
    }

    if (!protocol_)
    {
        ESP_LOGE(TAG, "Protocol not initialized");
        return;
    }

    if (device_state_ == kDeviceStateIdle)
    {
        Schedule([this]()
                 {
            if (!protocol_->IsAudioChannelOpened()) {
                SetDeviceState(kDeviceStateConnecting);
                if (!protocol_->OpenAudioChannel()) {
                    return;
                }
            }

            SetListeningMode(kListeningModeManualStop); });
    }
    else if (device_state_ == kDeviceStateSpeaking)
    {
        Schedule([this]()
                 {
            AbortSpeaking(kAbortReasonNone);
            SetListeningMode(kListeningModeManualStop); });
    }
}

void Application::StopListening()
{
    if (device_state_ == kDeviceStateAudioTesting)
    {
        audio_service_.EnableAudioTesting(false);
        SetDeviceState(kDeviceStateWifiConfiguring);
        return;
    }

    const std::array<int, 3> valid_states = {
        kDeviceStateListening,
        kDeviceStateSpeaking,
        kDeviceStateIdle,
    };
    // If not valid, do nothing
    if (std::find(valid_states.begin(), valid_states.end(), device_state_) == valid_states.end())
    {
        return;
    }

    Schedule([this]()
             {
        if (device_state_ == kDeviceStateListening) {
            protocol_->SendStopListening();
            SetDeviceState(kDeviceStateIdle);
        } });
}

void Application::Start()
{
    auto &board = Board::GetInstance();
    SetDeviceState(kDeviceStateStarting);

    /* Setup the display */
    auto display = board.GetDisplay();

    // Print board name/version info
    display->SetChatMessage("system", SystemInfo::GetUserAgent().c_str());

    /* Setup the audio service */
    auto codec = board.GetAudioCodec();
    audio_service_.Initialize(codec);
    audio_service_.Start();

    AudioServiceCallbacks callbacks;
    callbacks.on_send_queue_available = [this]()
    {
        xEventGroupSetBits(event_group_, MAIN_EVENT_SEND_AUDIO);
    };
    callbacks.on_wake_word_detected = [this](const std::string &wake_word)
    {
        xEventGroupSetBits(event_group_, MAIN_EVENT_WAKE_WORD_DETECTED);
    };
    callbacks.on_vad_change = [this](bool speaking)
    {
        xEventGroupSetBits(event_group_, MAIN_EVENT_VAD_CHANGE);
    };
    audio_service_.SetCallbacks(callbacks);

    // Start the main event loop task with priority 3
    xTaskCreate([](void *arg)
                {
        ((Application*)arg)->MainEventLoop();
        vTaskDelete(NULL); }, "main_event_loop", 2048 * 4, this, 3, &main_event_loop_task_handle_);

    /* Start the clock timer to update the status bar */
    esp_timer_start_periodic(clock_timer_handle_, 1000000);

    /* Wait for the network to be ready */
    board.StartNetwork();

#ifdef CONFIG_SD_CARD_ENABLE
    auto sd_card = board.GetSdCard();
    if (sd_card != nullptr)
    {
        if (sd_card->Initialize() == ESP_OK)
        {
            ESP_LOGI(TAG, "SD card mounted successfully");
            if (sd_music_ != nullptr)
            {
                sd_music_->Initialize(sd_card);
                sd_music_->loadTrackList();
            }
        }
        else
        {
            ESP_LOGW(TAG, "Failed to mount SD card");
        }
    }
#endif

    // Update the status bar immediately to show the network state
    display->UpdateStatusBar(true);

    // Check for new assets version
    CheckAssetsVersion();

    // Check for new firmware version or get the MQTT broker address
    Ota ota;
    CheckNewVersion(ota);

    // Start the OTA server
    auto &ota_server = ota::OtaServer::GetInstance();
    if (ota_server.Start() == ESP_OK)
    {
        ESP_LOGI(TAG, "OTA server started successfully");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to start OTA server");
    }

    // Initialize the protocol
    display->SetStatus(Lang::Strings::LOADING_PROTOCOL);

    // Add MCP common tools before initializing the protocol
    auto &mcp_server = McpServer::GetInstance();
    mcp_server.AddCommonTools();
    mcp_server.AddUserOnlyTools();

    if (ota.HasMqttConfig())
    {
        protocol_ = std::make_unique<MqttProtocol>();
    }
    else if (ota.HasWebsocketConfig())
    {
        protocol_ = std::make_unique<WebsocketProtocol>();
    }
    else
    {
        ESP_LOGW(TAG, "No protocol specified in the OTA config, using MQTT");
        protocol_ = std::make_unique<MqttProtocol>();
    }

    protocol_->OnConnected([this]()
                           { DismissAlert(); });

    protocol_->OnNetworkError([this](const std::string &message)
                              {
        last_error_message_ = message;
        xEventGroupSetBits(event_group_, MAIN_EVENT_ERROR); });
    protocol_->OnIncomingAudio([this](std::unique_ptr<AudioStreamPacket> packet)
                               {
        if (device_state_ == kDeviceStateSpeaking) {
            audio_service_.PushPacketToDecodeQueue(std::move(packet));
        } });
    protocol_->OnAudioChannelOpened([this, codec, &board]()
                                    {
        board.SetPowerSaveMode(false);
        if (protocol_->server_sample_rate() != codec->output_sample_rate()) {
            ESP_LOGW(TAG, "Server sample rate %d does not match device output sample rate %d, resampling may cause distortion",
                protocol_->server_sample_rate(), codec->output_sample_rate());
        } });
    protocol_->OnAudioChannelClosed([this, &board]()
                                    {
        board.SetPowerSaveMode(true);
        Schedule([this]() {
            // Don't change to idle if currently streaming music or in quiz mode
            if (device_state_ == kDeviceStateStreaming) {
                ESP_LOGI(TAG, "Audio channel closed but music is streaming, keeping streaming state");
                return;
            }
            
            if (device_state_ == kDeviceStateQuiz) {
                ESP_LOGI(TAG, "Audio channel closed but in Quiz Mode, ignoring state reset");
                return;
            }

            auto display = Board::GetInstance().GetDisplay();
            display->SetChatMessage("system", "");
            SetDeviceState(kDeviceStateIdle);
        }); });
    protocol_->OnIncomingJson([this, display](const cJSON *root)
                              {
        // Parse JSON data
        auto type = cJSON_GetObjectItem(root, "type");
        if (strcmp(type->valuestring, "tts") == 0) {
            auto state = cJSON_GetObjectItem(root, "state");
            if (strcmp(state->valuestring, "start") == 0) {
                Schedule([this]() {
                    aborted_ = false;
                    if (device_state_ == kDeviceStateIdle || device_state_ == kDeviceStateListening) {
                        SetDeviceState(kDeviceStateSpeaking);
                    }
                });
            } else if (strcmp(state->valuestring, "stop") == 0) {
                Schedule([this]() {
                    if (device_state_ == kDeviceStateSpeaking) {
                        if (listening_mode_ == kListeningModeManualStop) {
                            SetDeviceState(kDeviceStateIdle);
                        } else {
                            SetDeviceState(kDeviceStateListening);
                        }
                    }
                });
            } else if (strcmp(state->valuestring, "sentence_start") == 0) {
                auto text = cJSON_GetObjectItem(root, "text");
                if (cJSON_IsString(text)) {
                    ESP_LOGI(TAG, "<< %s", text->valuestring);
                    Schedule([this, display, message = std::string(text->valuestring)]() {
                        display->SetChatMessage("assistant", message.c_str());
                    });
                }
            }
        } else if (strcmp(type->valuestring, "stt") == 0) {
            auto text = cJSON_GetObjectItem(root, "text");
            if (cJSON_IsString(text)) {
                ESP_LOGI(TAG, ">> %s", text->valuestring);
                std::string stt_text = text->valuestring;
                Schedule([this, display, stt_text]() {
                    display->SetChatMessage("user", stt_text.c_str());
                    
#ifdef CONFIG_QUIZ_ENABLE
                    // Check for quiz keywords or answers
                    if (HandleQuizVoiceInput(stt_text)) {
                        return; // Handled by quiz system
                    }
#endif
                });
            }
        } else if (strcmp(type->valuestring, "llm") == 0) {
            auto emotion = cJSON_GetObjectItem(root, "emotion");
            if (cJSON_IsString(emotion)) {
                Schedule([this, display, emotion_str = std::string(emotion->valuestring)]() {
                    display->SetEmotion(emotion_str.c_str());
                });
            }
        } else if (strcmp(type->valuestring, "mcp") == 0) {
            auto payload = cJSON_GetObjectItem(root, "payload");
            if (cJSON_IsObject(payload)) {
                McpServer::GetInstance().ParseMessage(payload);
            }
        } else if (strcmp(type->valuestring, "system") == 0) {
            auto command = cJSON_GetObjectItem(root, "command");
            if (cJSON_IsString(command)) {
                ESP_LOGI(TAG, "System command: %s", command->valuestring);
                if (strcmp(command->valuestring, "reboot") == 0) {
                    // Do a reboot if user requests a OTA update
                    Schedule([this]() {
                        Reboot();
                    });
                } else {
                    ESP_LOGW(TAG, "Unknown system command: %s", command->valuestring);
                }
            }
        } else if (strcmp(type->valuestring, "alert") == 0) {
            auto status = cJSON_GetObjectItem(root, "status");
            auto message = cJSON_GetObjectItem(root, "message");
            auto emotion = cJSON_GetObjectItem(root, "emotion");
            if (cJSON_IsString(status) && cJSON_IsString(message) && cJSON_IsString(emotion)) {
                Alert(status->valuestring, message->valuestring, emotion->valuestring, Lang::Sounds::OGG_VIBRATION);
            } else {
                ESP_LOGW(TAG, "Alert command requires status, message and emotion");
            }
        } else if (strcmp(type->valuestring, "ota_url") == 0) {
            auto url = cJSON_GetObjectItem(root, "url");
            if (cJSON_IsString(url)) {
                std::string firmware_url = url->valuestring;
                ESP_LOGI(TAG, "Received OTA URL via Protocol: %s", firmware_url.c_str());
                
                Schedule([this, display, firmware_url]() {
                    display->SetChatMessage("system", "OTA Update Started...");
                    
                    // Run OTA in a separate thread to avoid blocking the main loop
                    std::thread([this, display, firmware_url]() {
                        Ota ota;
                        bool success = ota.StartUpgradeFromUrl(firmware_url, [this, display](int progress, size_t speed) {
                            Schedule([display, progress, speed]() {
                                char msg[64];
                                snprintf(msg, sizeof(msg), "Updating: %d%% %uKB/s", progress, (unsigned int)(speed / 1024));
                                display->SetChatMessage("system", msg);
                            });
                        });

                        if (success) {
                            Schedule([display]() {
                                display->SetChatMessage("system", "Update Success! Restarting...");
                            });
                            vTaskDelay(pdMS_TO_TICKS(2000));
                            esp_restart();
                        } else {
                            Schedule([this, display]() {
                                display->SetChatMessage("system", "Update Failed!");
                                Alert(Lang::Strings::ERROR, "Update Failed", "circle_xmark", Lang::Sounds::OGG_ERR_PIN);
                            });
                        }
                    }).detach();
                });
            }
#if CONFIG_RECEIVE_CUSTOM_MESSAGE
        } else if (strcmp(type->valuestring, "custom") == 0) {
            auto payload = cJSON_GetObjectItem(root, "payload");
            ESP_LOGI(TAG, "Received custom message: %s", cJSON_PrintUnformatted(root));
            if (cJSON_IsObject(payload)) {
                Schedule([this, display, payload_str = std::string(cJSON_PrintUnformatted(payload))]() {
                    display->SetChatMessage("system", payload_str.c_str());
                });
            } else {
                ESP_LOGW(TAG, "Invalid custom message format: missing payload");
            }
#endif
        } else {
            ESP_LOGW(TAG, "Unknown message type: %s", type->valuestring);
        } });
    bool protocol_started = protocol_->Start();

    SystemInfo::PrintHeapStats();
    SetDeviceState(kDeviceStateIdle);

    has_server_time_ = ota.HasServerTime();
    if (protocol_started)
    {
        std::string message = std::string(Lang::Strings::VERSION) + ota.GetCurrentVersion();
        display->ShowNotification(message.c_str());
        display->SetChatMessage("system", "");
        // Play the success sound to indicate the device is ready
        audio_service_.PlaySound(Lang::Sounds::OGG_SUCCESS);
    }
}

// Add a async task to MainLoop
void Application::Schedule(std::function<void()> callback)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        main_tasks_.push_back(std::move(callback));
    }
    xEventGroupSetBits(event_group_, MAIN_EVENT_SCHEDULE);
}

// The Main Event Loop controls the chat state and websocket connection
// If other tasks need to access the websocket or chat state,
// they should use Schedule to call this function
void Application::MainEventLoop()
{
    while (true)
    {
        auto bits = xEventGroupWaitBits(event_group_, MAIN_EVENT_SCHEDULE | MAIN_EVENT_SEND_AUDIO | MAIN_EVENT_WAKE_WORD_DETECTED | MAIN_EVENT_VAD_CHANGE | MAIN_EVENT_CLOCK_TICK | MAIN_EVENT_ERROR, pdTRUE, pdFALSE, portMAX_DELAY);

        if (bits & MAIN_EVENT_ERROR)
        {
            if (device_state_ != kDeviceStateQuiz)
            {
                SetDeviceState(kDeviceStateIdle);
            }
            else
            {
                ESP_LOGW(TAG, "Network error occurred but keeping Quiz Mode active");
            }
            Alert(Lang::Strings::ERROR, last_error_message_.c_str(), "circle_xmark", Lang::Sounds::OGG_EXCLAMATION);
        }

        if (bits & MAIN_EVENT_SEND_AUDIO)
        {
            while (auto packet = audio_service_.PopPacketFromSendQueue())
            {
                if (protocol_ && !protocol_->SendAudio(std::move(packet)))
                {
                    break;
                }
            }
        }

        if (bits & MAIN_EVENT_WAKE_WORD_DETECTED)
        {
            OnWakeWordDetected();
        }

        if (bits & MAIN_EVENT_VAD_CHANGE)
        {
            if (device_state_ == kDeviceStateListening)
            {
                auto led = Board::GetInstance().GetLed();
                led->OnStateChanged();
            }
        }

        if (bits & MAIN_EVENT_SCHEDULE)
        {
            std::unique_lock<std::mutex> lock(mutex_);
            auto tasks = std::move(main_tasks_);
            lock.unlock();
            for (auto &task : tasks)
            {
                task();
            }
        }

        if (bits & MAIN_EVENT_CLOCK_TICK)
        {
            clock_ticks_++;

#ifdef CONFIG_STANDBY_SCREEN_ENABLE
            if (device_state_ == kDeviceStateIdle)
            {
                // Update every second for time
                UpdateIdleDisplay();

                // Fetch weather every 30 mins (1800 seconds) or 5 seconds after boot
                if (clock_ticks_ == 5 || clock_ticks_ % 1800 == 0)
                {
                    auto &ws = WeatherService::GetInstance();
                    if (!ws.IsFetching())
                    {
                        xTaskCreate([](void *arg)
                                    {
                            auto &ws = WeatherService::GetInstance();
                            ws.FetchWeatherData();
                            vTaskDelete(NULL); }, "weather_fetch", 4096, NULL, 5, NULL);
                    }
                }
            }
#endif

            auto display = Board::GetInstance().GetDisplay();
            display->UpdateStatusBar();

            // Print the debug info every 10 seconds
            if (clock_ticks_ % 10 == 0)
            {
                // SystemInfo::PrintTaskCpuUsage(pdMS_TO_TICKS(1000));
                // SystemInfo::PrintTaskList();
                SystemInfo::PrintHeapStats();
            }
        }
    }
}

void Application::OnWakeWordDetected()
{
    if (!protocol_)
    {
        return;
    }

    if (device_state_ == kDeviceStateIdle)
    {
        audio_service_.EncodeWakeWord();

        if (!protocol_->IsAudioChannelOpened())
        {
            SetDeviceState(kDeviceStateConnecting);
            if (!protocol_->OpenAudioChannel())
            {
                audio_service_.EnableWakeWordDetection(true);
                return;
            }
        }

        auto wake_word = audio_service_.GetLastWakeWord();
        ESP_LOGI(TAG, "Wake word detected: %s", wake_word.c_str());
#if 0 // CONFIG_SEND_WAKE_WORD_DATA
      // Encode and send the wake word data to the server
        while (auto packet = audio_service_.PopWakeWordPacket())
        {
            protocol_->SendAudio(std::move(packet));
        }
        // Set the chat state to wake word detected
        protocol_->SendWakeWordDetected(wake_word);
        SetListeningMode(aec_mode_ == kAecOff ? kListeningModeAutoStop : kListeningModeRealtime);
#else
        SetListeningMode(aec_mode_ == kAecOff ? kListeningModeAutoStop : kListeningModeRealtime);
        // Play the pop up sound to indicate the wake word is detected
        audio_service_.PlaySound(Lang::Sounds::OGG_POPUP);
#endif
    }
    else if (device_state_ == kDeviceStateSpeaking)
    {
        AbortSpeaking(kAbortReasonWakeWordDetected);
    }
    else if (device_state_ == kDeviceStateStreaming)
    {
        StopMusicStreaming();
        SetListeningMode(aec_mode_ == kAecOff ? kListeningModeAutoStop : kListeningModeRealtime);
    }
    else if (device_state_ == kDeviceStateActivating)
    {
        SetDeviceState(kDeviceStateIdle);
    }
}

void Application::AbortSpeaking(AbortReason reason)
{
    ESP_LOGI(TAG, "Abort speaking");
    aborted_ = true;
    if (protocol_)
    {
        protocol_->SendAbortSpeaking(reason);
    }
}

void Application::SetListeningMode(ListeningMode mode)
{
    listening_mode_ = mode;
    SetDeviceState(kDeviceStateListening);
}

void Application::SetDeviceState(DeviceState state)
{
    if (device_state_ == state)
    {
        return;
    }

    clock_ticks_ = 0;
    auto previous_state = device_state_;
    device_state_ = state;
    ESP_LOGI(TAG, "STATE: %s", STATE_STRINGS[device_state_]);

    // Send the state change event
    DeviceStateEventManager::GetInstance().PostStateChangeEvent(previous_state, state);

    // Handle transition from WiFi configuring state - re-enable audio
    if (previous_state == kDeviceStateWifiConfiguring && state != kDeviceStateWifiConfiguring)
    {
        auto &board = Board::GetInstance();
        auto codec = board.GetAudioCodec();
        if (codec && !codec->output_enabled())
        {
            ESP_LOGI(TAG, "Re-enabling audio output after WiFi configuration");
            codec->EnableOutput(true);
        }
    }

    auto &board = Board::GetInstance();
    auto display = board.GetDisplay();
    auto led = board.GetLed();
    led->OnStateChanged();

#ifdef CONFIG_STANDBY_SCREEN_ENABLE
    if (state != kDeviceStateIdle && state != kDeviceStateUnknown)
    {
        display->HideIdleCard();
    }
#endif

    switch (state)
    {
    case kDeviceStateUnknown:
    case kDeviceStateIdle:
        display->SetStatus(Lang::Strings::STANDBY);
        display->SetEmotion("neutral");
        audio_service_.EnableVoiceProcessing(false);
        audio_service_.EnableWakeWordDetection(true);
        break;
    case kDeviceStateConnecting:
        display->SetStatus(Lang::Strings::CONNECTING);
        display->SetEmotion("neutral");
        display->SetChatMessage("system", "");
        break;
    case kDeviceStateListening:
        display->SetStatus(Lang::Strings::LISTENING);
        display->SetEmotion("neutral");

        // Make sure the audio processor is running
        if (!audio_service_.IsAudioProcessorRunning())
        {
            // Send the start listening command
            protocol_->SendStartListening(listening_mode_);
            audio_service_.EnableVoiceProcessing(true);
            audio_service_.EnableWakeWordDetection(false);
        }
        break;
    case kDeviceStateSpeaking:
        display->SetStatus(Lang::Strings::SPEAKING);

        if (listening_mode_ != kListeningModeRealtime)
        {
            audio_service_.EnableVoiceProcessing(false);
            // Only AFE wake word can be detected in speaking mode
            audio_service_.EnableWakeWordDetection(audio_service_.IsAfeWakeWord());
        }
        audio_service_.ResetDecoder();
        break;
    case kDeviceStateStreaming:
        display->SetStatus("Streaming Music");
        display->SetEmotion("music");
        // Keep minimal wake word detection (AFE if available)
        audio_service_.EnableVoiceProcessing(false);
        audio_service_.EnableWakeWordDetection(audio_service_.IsAfeWakeWord());
        break;
    case kDeviceStateWifiConfiguring:
        display->SetStatus("WiFi Configuration");
        display->SetEmotion("gear");
        // Disable audio during WiFi configuration to prevent power drop
        {
            auto &board = Board::GetInstance();
            auto codec = board.GetAudioCodec();
            if (codec && codec->output_enabled())
            {
                ESP_LOGI(TAG, "Disabling audio output during WiFi configuration");
                codec->EnableOutput(false);
            }
        }
        // Disable voice processing and wake word during WiFi config
        audio_service_.EnableVoiceProcessing(false);
        audio_service_.EnableWakeWordDetection(false);
        break;
    case kDeviceStateQuiz:
        display->SetStatus("Quiz Mode");
        display->SetEmotion("neutral");
        // Keep wake word detection for quiz voice answers
        audio_service_.EnableVoiceProcessing(false);
#ifdef CONFIG_QUIZ_VOICE_ANSWER
        audio_service_.EnableWakeWordDetection(true);
#else
        audio_service_.EnableWakeWordDetection(false);
#endif
        break;
    default:
        // Do nothing
        break;
    }
}

void Application::Reboot()
{
    ESP_LOGI(TAG, "Rebooting...");
    // Disconnect the audio channel
    if (protocol_ && protocol_->IsAudioChannelOpened())
    {
        protocol_->CloseAudioChannel();
    }
    protocol_.reset();
    audio_service_.Stop();

    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
}

bool Application::UpgradeFirmware(Ota &ota, const std::string &url)
{
    auto &board = Board::GetInstance();
    auto display = board.GetDisplay();

    // Use provided URL or get from OTA object
    std::string upgrade_url = url.empty() ? ota.GetFirmwareUrl() : url;
    std::string version_info = url.empty() ? ota.GetFirmwareVersion() : "(Manual upgrade)";

    // Close audio channel if it's open
    if (protocol_ && protocol_->IsAudioChannelOpened())
    {
        ESP_LOGI(TAG, "Closing audio channel before firmware upgrade");
        protocol_->CloseAudioChannel();
    }
    ESP_LOGI(TAG, "Starting firmware upgrade from URL: %s", upgrade_url.c_str());

    Alert(Lang::Strings::OTA_UPGRADE, Lang::Strings::UPGRADING, "download", Lang::Sounds::OGG_UPGRADE);
    vTaskDelay(pdMS_TO_TICKS(3000));

    SetDeviceState(kDeviceStateUpgrading);

    std::string message = std::string(Lang::Strings::NEW_VERSION) + version_info;
    display->SetChatMessage("system", message.c_str());

    board.SetPowerSaveMode(false);
    audio_service_.Stop();
    vTaskDelay(pdMS_TO_TICKS(1000));

    bool upgrade_success = ota.StartUpgradeFromUrl(upgrade_url, [display](int progress, size_t speed)
                                                   { std::thread([display, progress, speed]()
                                                                 {
            char buffer[32];
            snprintf(buffer, sizeof(buffer), "%d%% %uKB/s", progress, speed / 1024);
            display->SetChatMessage("system", buffer); })
                                                         .detach(); });

    if (!upgrade_success)
    {
        // Upgrade failed, restart audio service and continue running
        ESP_LOGE(TAG, "Firmware upgrade failed, restarting audio service and continuing operation...");
        audio_service_.Start();       // Restart audio service
        board.SetPowerSaveMode(true); // Restore power save mode
        Alert(Lang::Strings::ERROR, Lang::Strings::UPGRADE_FAILED, "circle_xmark", Lang::Sounds::OGG_EXCLAMATION);
        vTaskDelay(pdMS_TO_TICKS(3000));
        return false;
    }
    else
    {
        // Upgrade success, reboot immediately
        ESP_LOGI(TAG, "Firmware upgrade successful, rebooting...");
        display->SetChatMessage("system", "Upgrade successful, rebooting...");
        vTaskDelay(pdMS_TO_TICKS(1000)); // Brief pause to show message
        Reboot();
        return true;
    }
}

void Application::WakeWordInvoke(const std::string &wake_word)
{
    if (!protocol_)
    {
        return;
    }

    if (device_state_ == kDeviceStateIdle)
    {
        audio_service_.EncodeWakeWord();

        if (!protocol_->IsAudioChannelOpened())
        {
            SetDeviceState(kDeviceStateConnecting);
            if (!protocol_->OpenAudioChannel())
            {
                audio_service_.EnableWakeWordDetection(true);
                return;
            }
        }

        ESP_LOGI(TAG, "Wake word detected: %s", wake_word.c_str());
#if CONFIG_USE_AFE_WAKE_WORD || CONFIG_USE_CUSTOM_WAKE_WORD
        // Encode and send the wake word data to the server
        while (auto packet = audio_service_.PopWakeWordPacket())
        {
            protocol_->SendAudio(std::move(packet));
        }
        // Set the chat state to wake word detected
        protocol_->SendWakeWordDetected(wake_word);
        SetListeningMode(aec_mode_ == kAecOff ? kListeningModeAutoStop : kListeningModeRealtime);
#else
        SetListeningMode(aec_mode_ == kAecOff ? kListeningModeAutoStop : kListeningModeRealtime);
        // Play the pop up sound to indicate the wake word is detected
        audio_service_.PlaySound(Lang::Sounds::OGG_POPUP);
#endif
    }
    else if (device_state_ == kDeviceStateSpeaking)
    {
        Schedule([this]()
                 { AbortSpeaking(kAbortReasonNone); });
    }
    else if (device_state_ == kDeviceStateListening)
    {
        Schedule([this]()
                 {
            if (protocol_) {
                protocol_->CloseAudioChannel();
            } });
    }
}

bool Application::CanEnterSleepMode()
{
    if (device_state_ != kDeviceStateIdle)
    {
        return false;
    }

    if (protocol_ && protocol_->IsAudioChannelOpened())
    {
        return false;
    }

    auto music = Board::GetInstance().GetMusic();
    if (music && music->IsPlaying())
    {
        return false;
    }

    if (!audio_service_.IsIdle())
    {
        return false;
    }

    // Now it is safe to enter sleep mode
    return true;
}

void Application::SendMcpMessage(const std::string &payload)
{
    if (protocol_ == nullptr)
    {
        return;
    }

    // Make sure you are using main thread to send MCP message
    if (xTaskGetCurrentTaskHandle() == main_event_loop_task_handle_)
    {
        protocol_->SendMcpMessage(payload);
    }
    else
    {
        Schedule([this, payload = std::move(payload)]()
                 { protocol_->SendMcpMessage(payload); });
    }
}

void Application::SetAecMode(AecMode mode)
{
    aec_mode_ = mode;
    Schedule([this]()
             {
        auto& board = Board::GetInstance();
        auto display = board.GetDisplay();
        switch (aec_mode_) {
        case kAecOff:
            audio_service_.EnableDeviceAec(false);
            display->ShowNotification(Lang::Strings::RTC_MODE_OFF);
            break;
        case kAecOnServerSide:
            audio_service_.EnableDeviceAec(false);
            display->ShowNotification(Lang::Strings::RTC_MODE_ON);
            break;
        case kAecOnDeviceSide:
            audio_service_.EnableDeviceAec(true);
            display->ShowNotification(Lang::Strings::RTC_MODE_ON);
            break;
        }

        // If the AEC mode is changed, close the audio channel
        if (protocol_ && protocol_->IsAudioChannelOpened()) {
            protocol_->CloseAudioChannel();
        } });
}

void Application::PlaySound(const std::string_view &sound)
{
    audio_service_.PlaySound(sound);
}

void Application::StartMusicStreaming(const std::string &url)
{
    Schedule([this, url]()
             {
        auto music = Board::GetInstance().GetMusic();
        if (!music) {
            Alert(Lang::Strings::ERROR, "Music not available", "circle_xmark", Lang::Sounds::OGG_EXCLAMATION);
            return;
        }
        
        if (device_state_ == kDeviceStateStreaming) {
            StopMusicStreaming();
        }
        
        if (music->StartStreaming(url)) {
            SetDeviceState(kDeviceStateStreaming);
        } else {
            Alert(Lang::Strings::ERROR, "Failed to start music streaming", "circle_xmark", Lang::Sounds::OGG_EXCLAMATION);
        } });
}

void Application::StopMusicStreaming()
{
    Schedule([this]()
             {
        auto music = Board::GetInstance().GetMusic();
        if (music) {
            music->StopStreaming();
        }
        if (device_state_ == kDeviceStateStreaming) {
            SetDeviceState(kDeviceStateIdle);
        } });
}

// 新增：接收外部音频数据（如音乐播放）
void Application::AddAudioData(AudioStreamPacket &&packet)
{
    auto codec = Board::GetInstance().GetAudioCodec();
    if ((device_state_ == kDeviceStateIdle || device_state_ == kDeviceStateStreaming) && codec->output_enabled())
    {
        // packet.payload包含的是原始PCM数据（int16_t）
        if (packet.payload.size() >= 2)
        {
            size_t num_samples = packet.payload.size() / sizeof(int16_t);
            std::vector<int16_t> pcm_data(num_samples);

            // Ensure alignment: copy sample by sample to avoid alignment issues
            const int16_t *src = reinterpret_cast<const int16_t *>(packet.payload.data());
            for (size_t i = 0; i < num_samples; ++i)
            {
                pcm_data[i] = src[i];
            }

            // Check if sample rates match, if not, perform simple resampling
            if (packet.sample_rate != codec->output_sample_rate())
            {
                // ESP_LOGI(TAG, "Resampling music audio from %d to %d Hz",
                //         packet.sample_rate, codec->output_sample_rate());

                // Validate sample rate parameters
                if (packet.sample_rate <= 0 || codec->output_sample_rate() <= 0)
                {
                    ESP_LOGE(TAG, "Invalid sample rates: %d -> %d",
                             packet.sample_rate, codec->output_sample_rate());
                    return;
                }

                std::vector<int16_t> resampled;

                if (packet.sample_rate > codec->output_sample_rate())
                {
                    ESP_LOGI(TAG, "Music playback: switching sample rate from %d Hz to %d Hz",
                             codec->output_sample_rate(), packet.sample_rate);

                    // Try to dynamically switch sample rate
                    if (codec->SetOutputSampleRate(packet.sample_rate))
                    {
                        ESP_LOGI(TAG, "Successfully switched to music playback sample rate: %d Hz", packet.sample_rate);
                    }
                    else
                    {
                        ESP_LOGW(TAG, "Unable to switch sample rate, continuing with current sample rate: %d Hz", codec->output_sample_rate());
                    }
                }
                else
                {
                    // Upsampling: linear interpolation
                    float upsample_ratio = codec->output_sample_rate() / static_cast<float>(packet.sample_rate);
                    size_t expected_size = static_cast<size_t>(pcm_data.size() * upsample_ratio + 0.5f);
                    resampled.reserve(expected_size);

                    for (size_t i = 0; i < pcm_data.size(); ++i)
                    {
                        // Add original sample
                        resampled.push_back(pcm_data[i]);

                        // Calculate the number of samples that need interpolation
                        int interpolation_count = static_cast<int>(upsample_ratio) - 1;
                        if (interpolation_count > 0 && i + 1 < pcm_data.size())
                        {
                            int16_t current = pcm_data[i];
                            int16_t next = pcm_data[i + 1];
                            for (int j = 1; j <= interpolation_count; ++j)
                            {
                                float t = static_cast<float>(j) / (interpolation_count + 1);
                                int16_t interpolated = static_cast<int16_t>(current + (next - current) * t);
                                resampled.push_back(interpolated);
                            }
                        }
                        else if (interpolation_count > 0)
                        {
                            // Last sample, repeat directly
                            for (int j = 1; j <= interpolation_count; ++j)
                            {
                                resampled.push_back(pcm_data[i]);
                            }
                        }
                    }

                    ESP_LOGI(TAG, "Upsampled %d -> %d samples (ratio: %.2f)",
                             pcm_data.size(), resampled.size(), upsample_ratio);
                }

                pcm_data = std::move(resampled);
            }

            // Ensure audio output is enabled
            if (!codec->output_enabled())
            {
                codec->EnableOutput(true);
            }

            // Send PCM data to audio codec
            codec->OutputData(pcm_data);

            audio_service_.UpdateOutputTimestamp();
        }
    }
}

void Application::UpdateIdleDisplay()
{
#ifdef CONFIG_STANDBY_SCREEN_ENABLE
    auto &weather_service = WeatherService::GetInstance();
    // Get copy of weather info to avoid race conditions
    WeatherInfo weather_info = weather_service.GetWeatherInfo();

    auto display = Board::GetInstance().GetDisplay();
    if (display)
    {
        IdleCardInfo card;

        // Time & Date
        time_t now = time(nullptr);
        struct tm tm_buf;
        localtime_r(&now, &tm_buf);

        char buf[32];
        strftime(buf, sizeof(buf), "%H:%M", &tm_buf);
        card.time_text = buf;

        strftime(buf, sizeof(buf), "%d/%m/%Y", &tm_buf);
        card.date_text = buf;

        // Lunar
        card.lunar_date_text = LunarCalendar::GetLunarDateString(tm_buf.tm_mday, tm_buf.tm_mon + 1, tm_buf.tm_year + 1900);
        card.can_chi_year = LunarCalendar::GetCanChiYear(tm_buf.tm_year + 1900);

        // Weather
        if (weather_info.valid)
        {
            card.city = weather_info.city;
            snprintf(buf, sizeof(buf), "%.1f C", weather_info.temp);
            card.temperature_text = buf;
            card.humidity_text = std::to_string(weather_info.humidity) + "%";
            card.description_text = weather_info.description;
            card.icon_src = WeatherUI::GetWeatherIcon(weather_info.icon_code);

            snprintf(buf, sizeof(buf), "%.1f", weather_info.uv_index);
            card.uv_text = buf;

            snprintf(buf, sizeof(buf), "%.1f", weather_info.pm2_5);
            card.pm25_text = buf;
        }
        else
        {
            card.city = "Updating...";
            card.temperature_text = "--";
            card.icon = "\uf0c2"; // FA_CLOUD
        }

        display->ShowIdleCard(card);
    }
#endif
}

// ==================== Quiz Mode Implementation ====================
#ifdef CONFIG_QUIZ_ENABLE

void Application::StartQuizMode(const std::string& quiz_file)
{
    auto& board = Board::GetInstance();
    auto display = board.GetDisplay();
    
    // Create quiz manager if not exists
    if (!quiz_manager_) {
        quiz_manager_ = std::make_unique<QuizManager>();
        
        // Create UI if not exists
        if (!quiz_ui_) {
            quiz_ui_ = std::make_unique<QuizUI>();
        }
        
        // Setup UI
        // Note: We need to cast display to lv_obj_t* parent. 
        // Assuming GetDisplay() returns a Display* which wraps the LVGL object.
        // If display->GetLvglScreen() or similar exists, use that.
        // Looking at display implementation, usually we can get the active screen from LVGL directly
        // or the Display class handles it.
        // For now, let's assume we can get the active screen using lv_scr_act() if display doesn't provide it
        // Or if SetupQuizUI takes the raw display object.
        // Checking quiz_ui.cc: void SetupQuizUI(lv_obj_t* parent, int width, int height);
        
        quiz_ui_->SetupQuizUI(lv_scr_act(), display->width(), display->height(), display);
        
        // Connect UI callbacks
        quiz_ui_->SetOnAnswerPress([this](char answer) {
            ESP_LOGI(TAG, "UI Answer pressed: %c", answer);
            if (quiz_manager_) {
                quiz_manager_->SubmitAnswer(answer);
            }
        });

        // Set up callbacks
        quiz_manager_->SetOnQuestionReady([this, display](const QuizQuestion& question) {
            Schedule([this, display, question]() {
                // Hide emoji during quiz mode
                display->SetEmotion("");
                
                // Show question on UI
                if (quiz_ui_) {
                    int total = quiz_manager_->GetTotalQuestions();
                    quiz_ui_->ShowQuestion(question, question.question_number - 1, total);
                }
                
                display->SetChatMessage("system", ""); // Clear chat message as we use custom UI
                
                // Build TTS text: question + options
                std::string tts_text = "Câu " + std::to_string(question.question_number) + ". ";
                tts_text += question.question_text + ". ";
                tts_text += "A: " + question.options[0] + ". ";
                tts_text += "B: " + question.options[1] + ". ";
                tts_text += "C: " + question.options[2] + ". ";
                tts_text += "D: " + question.options[3] + ".";
                
                // Use SendText to trigger server-side TTS (treated as user prompt to read aloud)
                if (protocol_ && protocol_->IsAudioChannelOpened()) {
                    // Send a natural language "Read this" command
                    std::string prompt = "Hãy đọc to và chậm rãi nội dung sau đây để người dùng làm trắc nghiệm: " + tts_text;
                    protocol_->SendText(prompt);
                }
                
                ESP_LOGI(TAG, "Quiz Q%d displayed", question.question_number);
            });
        });
        
        quiz_manager_->SetOnAnswerChecked([this, display](const UserAnswer& answer, bool is_last) {
            Schedule([this, display, answer, is_last]() {
                
                // Show feedback on UI
                if (quiz_ui_) {
                    quiz_ui_->ShowAnswerFeedback(answer.selected_answer, answer.correct_answer, answer.is_correct);
                }
                
                ESP_LOGI(TAG, "Answer: %c, Correct: %c, Result: %s", 
                         answer.selected_answer, answer.correct_answer,
                         answer.is_correct ? "CORRECT" : "WRONG");
                
                if (!is_last) {
                    // Wait then move to next question
                    // Use a timer or delay in a separate task to avoid blocking main loop
                     xTaskCreate([](void* arg) {
                        vTaskDelay(pdMS_TO_TICKS(2000));
                        Application::GetInstance().Schedule([]() {
                             auto manager = Application::GetInstance().GetQuizManager();
                             if (manager) manager->NextQuestion();
                        });
                        vTaskDelete(NULL);
                    }, "quiz_delay", 2048, NULL, 5, NULL);
                }
            });
        });
        
        quiz_manager_->SetOnQuizComplete([this, display](const QuizSession& session) {
            Schedule([this, display, &session]() {
                
                std::string summary = quiz_manager_->GenerateResultSummary();
                
                // Show results on UI
                if (quiz_ui_) {
                    std::string details = ""; // You can format details here if needed
                    auto wrong_answers = quiz_manager_->GetWrongAnswers();
                    for (const auto& wa : wrong_answers) {
                        details += "Câu " + std::to_string(wa.question_number) + ": " + 
                                   std::string(1, wa.correct_answer) + "\n";
                    }
                    if (details.empty()) details = "Xuất sắc!";
                    
                    quiz_ui_->ShowResults(session.GetCorrectCount(), session.questions.size(), details);
                }
                
                // Speak results
                if (protocol_ && protocol_->IsAudioChannelOpened()) {
                    std::string prompt = "Hãy đọc thông báo kết quả sau: " + summary;
                    protocol_->SendText(prompt);
                }
                
                ESP_LOGI(TAG, "Quiz complete! Score: %d/%d", 
                         session.GetCorrectCount(), static_cast<int>(session.questions.size()));
                
                // Return to idle after showing results
                 xTaskCreate([](void* arg) {
                    vTaskDelay(pdMS_TO_TICKS(10000));
                    Application::GetInstance().Schedule([]() {
                         Application::GetInstance().StopQuizMode();
                    });
                    vTaskDelete(NULL);
                }, "quiz_finish", 2048, NULL, 5, NULL);
            });
        });
        
        quiz_manager_->SetOnError([this, display](const std::string& error) {
            Schedule([this, display, error]() {
                display->SetChatMessage("system", error.c_str());
                Alert(Lang::Strings::ERROR, error.c_str(), "circle_xmark", Lang::Sounds::OGG_EXCLAMATION);
                StopQuizMode();
            });
        });
    }
    
    // Start Quiz Mode (Server Based)
    // No more SD card file searching needed
    
    ESP_LOGI(TAG, "Starting Quiz Mode (Connecting to Server...)");
    
    SetDeviceState(kDeviceStateQuiz);
    audio_service_.PlaySound(Lang::Sounds::OGG_POPUP);
    
    if (quiz_ui_) quiz_ui_->Show();

    // Call StartQuiz without arguments (uses configured URL)
    if (!quiz_manager_->StartQuiz()) {
        Alert(Lang::Strings::ERROR, "Không thể kết nối Server Quiz!", "circle_xmark", Lang::Sounds::OGG_EXCLAMATION);
        SetDeviceState(kDeviceStateIdle);
        return;
    }
}

void Application::StopQuizMode()
{
    if (quiz_manager_) {
        quiz_manager_->StopQuiz();
        // Do not reset quiz_manager_ here. A background thread might still be running 
        // and accessing members. Ideally we should use shared_ptr/weak_ptr or join threads.
        // For now, keeping the instance alive is a safer quick fix.
        // quiz_manager_.reset(); 
    }
    
    if (quiz_ui_) {
        quiz_ui_->Hide();
        // quiz_ui_.reset(); // Keep UI object too, or reset if sure no callbacks pending
    }
    
    SetDeviceState(kDeviceStateIdle);
    
    auto display = Board::GetInstance().GetDisplay();
    display->SetStatus(Lang::Strings::STANDBY);
    display->SetChatMessage("system", "");
}

bool Application::HandleQuizVoiceInput(const std::string& text)
{
    // Check for quiz trigger keywords (Vietnamese)
    static const std::vector<std::string> quiz_keywords = {
        "tài liệu", "tai lieu",
        "kiểm tra", "kiem tra",
        "làm bài tập", "lam bai tap",
        "bài tập", "bai tap",
        "làm quiz", "lam quiz",
        "quiz", "test"
    };
    
    // Convert text to lowercase for comparison
    std::string lower_text = text;
    std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(), ::tolower);
    
    // If in idle state, check for quiz trigger
    if (device_state_ == kDeviceStateIdle) {
        for (const auto& keyword : quiz_keywords) {
            if (lower_text.find(keyword) != std::string::npos) {
                ESP_LOGI(TAG, "Quiz trigger keyword detected: %s", keyword.c_str());
                StartQuizMode();
                return true;
            }
        }
    }
    
    // If in quiz mode, check for answer input
    if (device_state_ == kDeviceStateQuiz && quiz_manager_ && quiz_manager_->IsActive()) {
        // Check for answer patterns
        static const std::vector<std::pair<std::string, char>> answer_patterns = {
            {"đáp án a", 'A'}, {"dap an a", 'A'}, {"chọn a", 'A'}, {"chon a", 'A'}, {" a ", 'A'}, {"câu a", 'A'},
            {"đáp án b", 'B'}, {"dap an b", 'B'}, {"chọn b", 'B'}, {"chon b", 'B'}, {" b ", 'B'}, {"câu b", 'B'},
            {"đáp án c", 'C'}, {"dap an c", 'C'}, {"chọn c", 'C'}, {"chon c", 'C'}, {" c ", 'C'}, {"câu c", 'C'},
            {"đáp án d", 'D'}, {"dap an d", 'D'}, {"chọn d", 'D'}, {"chon d", 'D'}, {" d ", 'D'}, {"câu d", 'D'},
        };
        
        // Pad with spaces for single letter detection
        std::string padded_text = " " + lower_text + " ";
        
        for (const auto& pattern : answer_patterns) {
            if (padded_text.find(pattern.first) != std::string::npos) {
                ESP_LOGI(TAG, "Quiz answer detected: %c", pattern.second);
                quiz_manager_->SubmitAnswer(pattern.second);
                
                // Schedule next question
                Schedule([this]() {
                    vTaskDelay(pdMS_TO_TICKS(1500));
                    if (quiz_manager_ && quiz_manager_->IsActive()) {
                        quiz_manager_->NextQuestion();
                    }
                });
                return true;
            }
        }
        
        // Also check for single letters at start or end
        if (!lower_text.empty()) {
            char first = lower_text.front();
            if (first >= 'a' && first <= 'd' && lower_text.length() <= 3) {
                char answer = first - 'a' + 'A';
                ESP_LOGI(TAG, "Quiz answer (single letter): %c", answer);
                quiz_manager_->SubmitAnswer(answer);
                Schedule([this]() {
                    vTaskDelay(pdMS_TO_TICKS(1500));
                    if (quiz_manager_ && quiz_manager_->IsActive()) {
                        quiz_manager_->NextQuestion();
                    }
                });
                return true;
            }
        }
    }
    
    return false;
}

#endif // CONFIG_QUIZ_ENABLE
