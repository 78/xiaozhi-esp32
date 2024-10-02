#include <BuiltinLed.h>
#include <TlsTransport.h>
#include <Ml307SslTransport.h>
#include <WifiConfigurationAp.h>
#include <WifiStation.h>
#include <SystemInfo.h>

#include <cstring>
#include <esp_log.h>
#include <cJSON.h>
#include <driver/gpio.h>

#include "Application.h"

#define TAG "Application"


Application::Application()
    : button_((gpio_num_t)CONFIG_BOOT_BUTTON_GPIO)
#ifdef CONFIG_USE_ML307
    , ml307_at_modem_(CONFIG_ML307_TX_PIN, CONFIG_ML307_RX_PIN, 4096),
      http_(ml307_at_modem_),
      firmware_upgrade_(http_)
#else
    , http_(),
    firmware_upgrade_(http_)
#endif
#ifdef CONFIG_USE_DISPLAY
    , display_(CONFIG_DISPLAY_SDA_PIN, CONFIG_DISPLAY_SCL_PIN)
#endif
{
    event_group_ = xEventGroupCreate();
    
    opus_encoder_.Configure(CONFIG_AUDIO_INPUT_SAMPLE_RATE, 1);
    opus_decoder_ = opus_decoder_create(opus_decode_sample_rate_, 1, NULL);
    if (opus_decode_sample_rate_ != CONFIG_AUDIO_OUTPUT_SAMPLE_RATE) {
        opus_resampler_.Configure(opus_decode_sample_rate_, CONFIG_AUDIO_OUTPUT_SAMPLE_RATE);
    }

    firmware_upgrade_.SetCheckVersionUrl(CONFIG_OTA_VERSION_URL);
    firmware_upgrade_.SetHeader("Device-Id", SystemInfo::GetMacAddress().c_str());
    firmware_upgrade_.SetPostData(SystemInfo::GetJsonString());
}

Application::~Application() {
    if (opus_decoder_ != nullptr) {
        opus_decoder_destroy(opus_decoder_);
    }
    if (audio_encode_task_stack_ != nullptr) {
        free(audio_encode_task_stack_);
    }

    vEventGroupDelete(event_group_);
}

void Application::CheckNewVersion() {
    // Check if there is a new firmware version available
    firmware_upgrade_.CheckVersion();
    if (firmware_upgrade_.HasNewVersion()) {
        // Wait for the chat state to be idle
        while (chat_state_ != kChatStateIdle) {
            vTaskDelay(100);
        }
        SetChatState(kChatStateUpgrading);
        firmware_upgrade_.StartUpgrade([this](int progress, size_t speed) {
#ifdef CONFIG_USE_DISPLAY
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "Upgrading...\n %d%% %zuKB/s", progress, speed / 1024);
            display_.SetText(buffer);
#endif
        });
        // If upgrade success, the device will reboot and never reach here
        ESP_LOGI(TAG, "Firmware upgrade failed...");
        SetChatState(kChatStateIdle);
    } else {
        firmware_upgrade_.MarkCurrentVersionValid();
    }
}

#ifdef CONFIG_USE_DISPLAY

#ifdef CONFIG_USE_ML307
static std::string csq_to_string(int csq) {
    if (csq == -1) {
        return "No network";
    } else if (csq >= 0 && csq <= 9) {
        return "Very bad";
    } else if (csq >= 10 && csq <= 14) {
        return "Bad";
    } else if (csq >= 15 && csq <= 19) {
        return "Fair";
    } else if (csq >= 20 && csq <= 24) {
        return "Good";
    } else if (csq >= 25 && csq <= 31) {
        return "Very good";
    }
    return "Invalid";
}
#else
static std::string rssi_to_string(int rssi) {
    if (rssi >= -55) {
        return "Very good";
    } else if (rssi >= -65) {
        return "Good";
    } else if (rssi >= -75) {
        return "Fair";
    } else if (rssi >= -85) {
        return "Poor";
    } else {
        return "No network";
    }
}
#endif

void Application::UpdateDisplay() {
    while (true) {
        if (chat_state_ == kChatStateIdle) {
#ifdef CONFIG_USE_ML307
            std::string network_name = ml307_at_modem_.GetCarrierName();
            int signal_quality = ml307_at_modem_.GetCsq();
            if (signal_quality == -1) {
                network_name = "No network";
            } else {
                ESP_LOGI(TAG, "%s CSQ: %d", network_name.c_str(), signal_quality);
                display_.SetText(network_name + "\n" + csq_to_string(signal_quality) + " (" + std::to_string(signal_quality) + ")");
            }
#else
            auto& wifi_station = WifiStation::GetInstance();
            int8_t rssi = wifi_station.GetRssi();
            display_.SetText(wifi_station.GetSsid() + "\n" + rssi_to_string(rssi) + " (" + std::to_string(rssi) + ")");
#endif
        }
        vTaskDelay(pdMS_TO_TICKS(10 * 1000));
    }
}
#endif

void Application::Start() {
    auto& builtin_led = BuiltinLed::GetInstance();
#ifdef CONFIG_USE_ML307
    builtin_led.SetBlue();
    builtin_led.StartContinuousBlink(100);
    ml307_at_modem_.SetDebug(false);
    ml307_at_modem_.SetBaudRate(921600);
    // Print the ML307 modem information
    std::string module_name = ml307_at_modem_.GetModuleName();
    ESP_LOGI(TAG, "ML307 Module: %s", module_name.c_str());
#ifdef CONFIG_USE_DISPLAY
    display_.SetText(std::string("Wait for network\n") + module_name);
#endif
    ml307_at_modem_.ResetConnections();
    ml307_at_modem_.WaitForNetworkReady();

    ESP_LOGI(TAG, "ML307 IMEI: %s", ml307_at_modem_.GetImei().c_str());
    ESP_LOGI(TAG, "ML307 ICCID: %s", ml307_at_modem_.GetIccid().c_str());
#else
    // Try to connect to WiFi, if failed, launch the WiFi configuration AP
    auto& wifi_station = WifiStation::GetInstance();    
#ifdef CONFIG_USE_DISPLAY
    display_.SetText(std::string("Connect to WiFi\n") + wifi_station.GetSsid());
#endif
    builtin_led.SetBlue();
    builtin_led.StartContinuousBlink(100);
    wifi_station.Start();
    if (!wifi_station.IsConnected()) {
        builtin_led.SetBlue();
        builtin_led.Blink(1000, 500);
        auto& wifi_ap = WifiConfigurationAp::GetInstance();
        wifi_ap.SetSsidPrefix("Xiaozhi");
#ifdef CONFIG_USE_DISPLAY
        display_.SetText(wifi_ap.GetSsid() + "\n" + wifi_ap.GetWebServerUrl());
#endif
        wifi_ap.Start();
        return;
    }
#endif

    audio_device_.OnInputData([this](const int16_t* data, int size) {
#ifdef CONFIG_USE_AFE_SR
        if (audio_processor_.IsRunning()) {
            audio_processor_.Input(data, size);
        }
        if (wake_word_detect_.IsDetectionRunning()) {
            wake_word_detect_.Feed(data, size);
        }
#else
        std::vector<int16_t> pcm(data, data + size);
        Schedule([this, pcm = std::move(pcm)]() {
            if (chat_state_ == kChatStateListening) {
                std::lock_guard<std::mutex> lock(mutex_);
                audio_encode_queue_.emplace_back(std::move(pcm));
                cv_.notify_all();
            }
        });
#endif
    });

    // Initialize the audio device
    audio_device_.Start(CONFIG_AUDIO_INPUT_SAMPLE_RATE, CONFIG_AUDIO_OUTPUT_SAMPLE_RATE);

    // OPUS encoder / decoder use a lot of stack memory
    const size_t opus_stack_size = 4096 * 8;
    audio_encode_task_stack_ = (StackType_t*)malloc(opus_stack_size);
    audio_encode_task_ = xTaskCreateStatic([](void* arg) {
        Application* app = (Application*)arg;
        app->AudioEncodeTask();
        vTaskDelete(NULL);
    }, "opus_encode", opus_stack_size, this, 1, audio_encode_task_stack_, &audio_encode_task_buffer_);

    xTaskCreate([](void* arg) {
        Application* app = (Application*)arg;
        app->AudioPlayTask();
        vTaskDelete(NULL);
    }, "play_audio", 4096 * 2, this, 5, NULL);

#ifdef CONFIG_USE_AFE_SR
    wake_word_detect_.OnVadStateChange([this](bool speaking) {
        Schedule([this, speaking]() {
            auto& builtin_led = BuiltinLed::GetInstance();
            if (chat_state_ == kChatStateListening) {
                if (speaking) {
                    builtin_led.SetRed(32);
                } else {
                    builtin_led.SetRed(8);
                }
                builtin_led.TurnOn();
            }
        });
    });

    wake_word_detect_.OnWakeWordDetected([this]() {
        Schedule([this]() {
            if (chat_state_ == kChatStateIdle) {
                // Encode the wake word data and start websocket client at the same time
                // They both consume a lot of time (700ms), so we can do them in parallel
                wake_word_detect_.EncodeWakeWordData();

                SetChatState(kChatStateConnecting);
                if (ws_client_ == nullptr) {
                    StartWebSocketClient();
                }
                if (ws_client_ && ws_client_->IsConnected()) {
                    auto encoded = wake_word_detect_.GetWakeWordStream();
                    // Send the wake word data to the server
                    ws_client_->Send(encoded.data(), encoded.size(), true);
                    opus_encoder_.ResetState();
                    // Send a ready message to indicate the server that the wake word data is sent
                    SetChatState(kChatStateWakeWordDetected);
                    // If connected, the hello message is already sent, so we can start communication
                    audio_processor_.Start();
                    ESP_LOGI(TAG, "Audio processor started");
                } else {
                    SetChatState(kChatStateIdle);
                }
            } else if (chat_state_ == kChatStateSpeaking) {
                break_speaking_ = true;
            }

            // Resume detection
            wake_word_detect_.StartDetection();
        });
    });
    wake_word_detect_.StartDetection();

    audio_processor_.OnOutput([this](std::vector<int16_t>&& data) {
        Schedule([this, data = std::move(data)]() {
            if (chat_state_ == kChatStateListening) {
                std::lock_guard<std::mutex> lock(mutex_);
                audio_encode_queue_.emplace_back(std::move(data));
                cv_.notify_all();
            }
        });
    });
#endif

    // Blink the LED to indicate the device is running
    builtin_led.SetGreen();
    builtin_led.BlinkOnce();

    button_.OnClick([this]() {
        Schedule([this]() {
            if (chat_state_ == kChatStateIdle) {
                SetChatState(kChatStateConnecting);
                StartWebSocketClient();

                if (ws_client_ && ws_client_->IsConnected()) {
                    opus_encoder_.ResetState();
#ifdef CONFIG_USE_AFE_SR
                    audio_processor_.Start();
#endif
                    SetChatState(kChatStateListening);
                    ESP_LOGI(TAG, "Communication started");
                } else {
                    SetChatState(kChatStateIdle);
                }
            } else if (chat_state_ == kChatStateSpeaking) {
                break_speaking_ = true;
            } else if (chat_state_ == kChatStateListening) {
                if (ws_client_ && ws_client_->IsConnected()) {
                    ws_client_->Close();
                }
            }
        });
    });

    xTaskCreate([](void* arg) {
        Application* app = (Application*)arg;
        app->MainLoop();
        vTaskDelete(NULL);
    }, "main_loop", 4096 * 2, this, 5, NULL);

    // Launch a task to check for new firmware version
    xTaskCreate([](void* arg) {
        Application* app = (Application*)arg;
        app->CheckNewVersion();
        vTaskDelete(NULL);
    }, "check_new_version", 4096 * 2, this, 1, NULL);

#ifdef CONFIG_USE_DISPLAY
    // Launch a task to update the display
    xTaskCreate([](void* arg) {
        Application* app = (Application*)arg;
        app->UpdateDisplay();
        vTaskDelete(NULL);
    }, "update_display", 4096, this, 1, NULL);
#endif
}

void Application::Schedule(std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    main_tasks_.push_back(callback);
    cv_.notify_all();
}

// The Main Loop controls the chat state and websocket connection
// If other tasks need to access the websocket or chat state,
// they should use Schedule to call this function
void Application::MainLoop() {
    while (true) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this]() {
            return !main_tasks_.empty();
        });
        auto task = std::move(main_tasks_.front());
        main_tasks_.pop_front();
        lock.unlock();
        task();
    }
}

void Application::SetChatState(ChatState state) {
    const char* state_str[] = {
        "idle",
        "connecting",
        "listening",
        "speaking",
        "wake_word_detected",
        "testing",
        "upgrading",
        "unknown"
    };
    chat_state_ = state;
    ESP_LOGI(TAG, "STATE: %s", state_str[chat_state_]);

    auto& builtin_led = BuiltinLed::GetInstance();
    switch (chat_state_) {
        case kChatStateIdle:
            builtin_led.TurnOff();
            break;
        case kChatStateConnecting:
            builtin_led.SetBlue();
            builtin_led.TurnOn();
            break;
        case kChatStateListening:
            builtin_led.SetRed();
            builtin_led.TurnOn();
            break;
        case kChatStateSpeaking:
            builtin_led.SetGreen();
            builtin_led.TurnOn();
            break;
        case kChatStateWakeWordDetected:
            builtin_led.SetBlue();
            builtin_led.TurnOn();
            break;
        case kChatStateUpgrading:
            builtin_led.SetGreen();
            builtin_led.StartContinuousBlink(100);
            break;
    }

    if (ws_client_ && ws_client_->IsConnected()) {
        cJSON* root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "type", "state");
        cJSON_AddStringToObject(root, "state", state_str[chat_state_]);
        char* json = cJSON_PrintUnformatted(root);

        std::lock_guard<std::mutex> lock(mutex_);
        ws_client_->Send(json);
        cJSON_Delete(root);
        free(json);
    }
}

BinaryProtocol* Application::AllocateBinaryProtocol(const uint8_t* payload, size_t payload_size) {
    auto last_timestamp = 0;
    auto protocol = (BinaryProtocol*)heap_caps_malloc(sizeof(BinaryProtocol) + payload_size, MALLOC_CAP_SPIRAM);
    protocol->version = htons(PROTOCOL_VERSION);
    protocol->type = htons(0);
    protocol->reserved = 0;
    protocol->timestamp = htonl(last_timestamp);
    protocol->payload_size = htonl(payload_size);
    assert(sizeof(BinaryProtocol) == 16);
    memcpy(protocol->payload, payload, payload_size);
    return protocol;
}

void Application::AudioEncodeTask() {
    ESP_LOGI(TAG, "Audio encode task started");
    while (true) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this]() {
            return !audio_encode_queue_.empty() || !audio_decode_queue_.empty();
        });

        if (!audio_encode_queue_.empty()) {
            auto pcm = std::move(audio_encode_queue_.front());
            audio_encode_queue_.pop_front();
            lock.unlock();

            // Encode audio data
            opus_encoder_.Encode(pcm, [this](const uint8_t* opus, size_t opus_size) {
                auto protocol = AllocateBinaryProtocol(opus, opus_size);
                Schedule([this, protocol, opus_size]() {
                    if (ws_client_ && ws_client_->IsConnected()) {
                        ws_client_->Send(protocol, sizeof(BinaryProtocol) + opus_size, true);
                    }
                    heap_caps_free(protocol);
                });
            });
        } else if (!audio_decode_queue_.empty()) {
            auto packet = std::move(audio_decode_queue_.front());
            audio_decode_queue_.pop_front();
            lock.unlock();

            int frame_size = opus_decode_sample_rate_ / 1000 * opus_duration_ms_;
            packet->pcm.resize(frame_size);

            int ret = opus_decode(opus_decoder_, packet->opus.data(), packet->opus.size(), packet->pcm.data(), frame_size, 0);
            if (ret < 0) {
                ESP_LOGE(TAG, "Failed to decode audio, error code: %d", ret);
                delete packet;
                continue;
            }

            if (opus_decode_sample_rate_ != CONFIG_AUDIO_OUTPUT_SAMPLE_RATE) {
                int target_size = opus_resampler_.GetOutputSamples(frame_size);
                std::vector<int16_t> resampled(target_size);
                opus_resampler_.Process(packet->pcm.data(), frame_size, resampled.data());
                packet->pcm = std::move(resampled);
            }

            std::lock_guard<std::mutex> lock(mutex_);
            audio_play_queue_.push_back(packet);
            cv_.notify_all();
        }
    }
}

void Application::HandleAudioPacket(AudioPacket* packet) {
    switch (packet->type)
    {
    case kAudioPacketTypeData: {
        if (skip_to_end_) {
            break;
        }

        // This will block until the audio device has finished playing the audio
        audio_device_.OutputData(packet->pcm);

        if (break_speaking_) {
            break_speaking_ = false;
            skip_to_end_ = true;
            
            // Play a silence and skip to the end
            int frame_size = opus_decode_sample_rate_ / 1000 * opus_duration_ms_;
            std::vector<int16_t> silence(frame_size);
            bzero(silence.data(), silence.size() * sizeof(int16_t));
            audio_device_.OutputData(silence);
        }
        break;
    }
    case kAudioPacketTypeStart:
        Schedule([this]() {
            SetChatState(kChatStateSpeaking);
        });
        break;
    case kAudioPacketTypeStop:
        skip_to_end_ = false;
        Schedule([this]() {
            SetChatState(kChatStateListening);
        });
        break;
    case kAudioPacketTypeSentenceStart:
        ESP_LOGI(TAG, "<< %s", packet->text.c_str());
        break;
    case kAudioPacketTypeSentenceEnd:
        break;
    default:
        ESP_LOGI(TAG, "Unknown packet type: %d", packet->type);
        break;
    }

    delete packet;
}

void Application::AudioPlayTask() {
    ESP_LOGI(TAG, "Audio play task started");

    while (true) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this]() {
            return !audio_play_queue_.empty();
        });
        auto packet = std::move(audio_play_queue_.front());
        audio_play_queue_.pop_front();
        lock.unlock();

        HandleAudioPacket(packet);
    }
}

void Application::SetDecodeSampleRate(int sample_rate) {
    if (opus_decode_sample_rate_ == sample_rate) {
        return;
    }

    opus_decoder_destroy(opus_decoder_);
    opus_decode_sample_rate_ = sample_rate;
    opus_decoder_ = opus_decoder_create(opus_decode_sample_rate_, 1, NULL);
    if (opus_decode_sample_rate_ != CONFIG_AUDIO_OUTPUT_SAMPLE_RATE) {
        opus_resampler_.Configure(opus_decode_sample_rate_, CONFIG_AUDIO_OUTPUT_SAMPLE_RATE);
    }
}

void Application::StartWebSocketClient() {
    if (ws_client_ != nullptr) {
        ESP_LOGW(TAG, "WebSocket client already exists");
        delete ws_client_;
    }

    std::string token = "Bearer " + std::string(CONFIG_WEBSOCKET_ACCESS_TOKEN);
#ifdef CONFIG_USE_ML307
    ws_client_ = new WebSocket(new Ml307SslTransport(ml307_at_modem_, 0));
#else
    ws_client_ = new WebSocket(new TlsTransport());
#endif
    ws_client_->SetHeader("Authorization", token.c_str());
    ws_client_->SetHeader("Device-Id", SystemInfo::GetMacAddress().c_str());
    ws_client_->SetHeader("Protocol-Version", std::to_string(PROTOCOL_VERSION).c_str());

    ws_client_->OnConnected([this]() {
        ESP_LOGI(TAG, "Websocket connected");
        
        // Send hello message to describe the client
        // keys: message type, version, wakeup_model, audio_params (format, sample_rate, channels)
        std::string message = "{";
        message += "\"type\":\"hello\",";
        message += "\"audio_params\":{";
        message += "\"format\":\"opus\", \"sample_rate\":" + std::to_string(CONFIG_AUDIO_INPUT_SAMPLE_RATE) + ", \"channels\":1";
        message += "}}";
        ws_client_->Send(message);
    });

    ws_client_->OnData([this](const char* data, size_t len, bool binary) {
        if (binary) {
            auto protocol = (BinaryProtocol*)data;

            auto packet = new AudioPacket();
            packet->type = kAudioPacketTypeData;
            packet->timestamp = ntohl(protocol->timestamp);
            auto payload_size = ntohl(protocol->payload_size);
            packet->opus.resize(payload_size);
            memcpy(packet->opus.data(), protocol->payload, payload_size);

            std::lock_guard<std::mutex> lock(mutex_);
            audio_decode_queue_.push_back(packet);
            cv_.notify_all();
        } else {
            // Parse JSON data
            auto root = cJSON_Parse(data);
            auto type = cJSON_GetObjectItem(root, "type");
            if (type != NULL) {
                if (strcmp(type->valuestring, "tts") == 0) {
                    auto packet = new AudioPacket();
                    auto state = cJSON_GetObjectItem(root, "state");
                    if (strcmp(state->valuestring, "start") == 0) {
                        packet->type = kAudioPacketTypeStart;
                        auto sample_rate = cJSON_GetObjectItem(root, "sample_rate");
                        if (sample_rate != NULL) {
                            SetDecodeSampleRate(sample_rate->valueint);
                        }
                    } else if (strcmp(state->valuestring, "stop") == 0) {
                        packet->type = kAudioPacketTypeStop;
                    } else if (strcmp(state->valuestring, "sentence_end") == 0) {
                        packet->type = kAudioPacketTypeSentenceEnd;
                    } else if (strcmp(state->valuestring, "sentence_start") == 0) {
                        packet->type = kAudioPacketTypeSentenceStart;
                        packet->text = cJSON_GetObjectItem(root, "text")->valuestring;
                    }

                    std::lock_guard<std::mutex> lock(mutex_);
                    audio_decode_queue_.push_back(packet);
                    cv_.notify_all();
                } else if (strcmp(type->valuestring, "stt") == 0) {
                    auto text = cJSON_GetObjectItem(root, "text");
                    if (text != NULL) {
                        ESP_LOGI(TAG, ">> %s", text->valuestring);
                    }
                }
            }
            cJSON_Delete(root);
        }
    });

    ws_client_->OnError([this](int error) {
        ESP_LOGE(TAG, "Websocket error: %d", error);
    });

    ws_client_->OnDisconnected([this]() {
        ESP_LOGI(TAG, "Websocket disconnected");
        Schedule([this]() {
#ifdef CONFIG_USE_AFE_SR
            audio_processor_.Stop();
#endif
            delete ws_client_;
            ws_client_ = nullptr;
            SetChatState(kChatStateIdle);
        });
    });

    if (!ws_client_->Connect(CONFIG_WEBSOCKET_URL)) {
        ESP_LOGE(TAG, "Failed to connect to websocket server");
        return;
    }
}