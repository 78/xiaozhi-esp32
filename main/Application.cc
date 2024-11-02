#include <BuiltinLed.h>
#include <Ml307SslTransport.h>
#include <WifiConfigurationAp.h>
#include <WifiStation.h>
#include <SystemInfo.h>

#include <cstring>
#include <esp_log.h>
#include <cJSON.h>
#include <driver/gpio.h>
#include <arpa/inet.h>

#include "Application.h"

#define TAG "Application"

extern const char p3_err_reg_start[] asm("_binary_err_reg_p3_start");
extern const char p3_err_reg_end[] asm("_binary_err_reg_p3_end");
extern const char p3_err_pin_start[] asm("_binary_err_pin_p3_start");
extern const char p3_err_pin_end[] asm("_binary_err_pin_p3_end");
extern const char p3_err_wificonfig_start[] asm("_binary_err_wificonfig_p3_start");
extern const char p3_err_wificonfig_end[] asm("_binary_err_wificonfig_p3_end");


Application::Application()
    : boot_button_((gpio_num_t)BOOT_BUTTON_GPIO),
      volume_up_button_((gpio_num_t)VOLUME_UP_BUTTON_GPIO),
      volume_down_button_((gpio_num_t)VOLUME_DOWN_BUTTON_GPIO),
      display_(DISPLAY_SDA_PIN, DISPLAY_SCL_PIN)
{
    event_group_ = xEventGroupCreate();

    opus_encoder_.Configure(16000, 1);
    opus_decoder_ = opus_decoder_create(opus_decode_sample_rate_, 1, NULL);
    if (opus_decode_sample_rate_ != AUDIO_OUTPUT_SAMPLE_RATE) {
        output_resampler_.Configure(AUDIO_OUTPUT_SAMPLE_RATE, opus_decode_sample_rate_);
    }
    if (16000 != AUDIO_INPUT_SAMPLE_RATE) {
        input_resampler_.Configure(AUDIO_INPUT_SAMPLE_RATE, 16000);
        reference_resampler_.Configure(AUDIO_INPUT_SAMPLE_RATE, 16000);
    }

    firmware_upgrade_.SetCheckVersionUrl(CONFIG_OTA_VERSION_URL);
    firmware_upgrade_.SetHeader("Device-Id", SystemInfo::GetMacAddress().c_str());
}

Application::~Application() {
    if (update_display_timer_ != nullptr) {
        esp_timer_stop(update_display_timer_);
        esp_timer_delete(update_display_timer_);
    }
    if (ws_client_ != nullptr) {
        delete ws_client_;
    }
    if (opus_decoder_ != nullptr) {
        opus_decoder_destroy(opus_decoder_);
    }
    if (audio_encode_task_stack_ != nullptr) {
        heap_caps_free(audio_encode_task_stack_);
    }
    if (main_loop_task_stack_ != nullptr) {
        heap_caps_free(main_loop_task_stack_);
    }
    if (audio_device_ != nullptr) {
        delete audio_device_;
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
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "Upgrading...\n %d%% %zuKB/s", progress, speed / 1024);
            display_.SetText(buffer);
        });
        // If upgrade success, the device will reboot and never reach here
        ESP_LOGI(TAG, "Firmware upgrade failed...");
        SetChatState(kChatStateIdle);
    } else {
        firmware_upgrade_.MarkCurrentVersionValid();
    }
}

void Application::Alert(const std::string&& title, const std::string&& message) {
    ESP_LOGE(TAG, "Alert: %s, %s", title.c_str(), message.c_str());
    display_.ShowNotification(std::string(title + "\n" + message));

    if (message == "PIN is not ready") {
        PlayLocalFile(p3_err_pin_start, p3_err_pin_end - p3_err_pin_start);
    } else if (message == "Configuring WiFi") {
        PlayLocalFile(p3_err_wificonfig_start, p3_err_wificonfig_end - p3_err_wificonfig_start);
    } else if (message == "Registration denied") {
        PlayLocalFile(p3_err_reg_start, p3_err_reg_end - p3_err_reg_start);
    }
}

void Application::PlayLocalFile(const char* data, size_t size) {
    ESP_LOGI(TAG, "PlayLocalFile: %zu bytes", size);
    SetDecodeSampleRate(16000);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto packet = new AudioPacket();
        packet->type = kAudioPacketTypeStart;
        audio_decode_queue_.push_back(packet);
    }

    ParseBinaryProtocol3(data, size);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto packet = new AudioPacket();
        packet->type = kAudioPacketTypeStop;
        audio_decode_queue_.push_back(packet);
        cv_.notify_all();
    }
}

void Application::Start() {
    auto& builtin_led = BuiltinLed::GetInstance();
    builtin_led.SetBlue();
    builtin_led.StartContinuousBlink(100);

    auto& board = Board::GetInstance();
    board.Initialize();

    audio_device_ = board.CreateAudioDevice();
    audio_device_->Initialize();
    audio_device_->EnableOutput(true);
    audio_device_->EnableInput(true);
    audio_device_->OnInputData([this](std::vector<int16_t>&& data) {
        if (16000 != AUDIO_INPUT_SAMPLE_RATE) {
            if (audio_device_->input_channels() == 2) {
                auto mic_channel = std::vector<int16_t>(data.size() / 2);
                auto reference_channel = std::vector<int16_t>(data.size() / 2);
                for (size_t i = 0, j = 0; i < mic_channel.size(); ++i, j += 2) {
                    mic_channel[i] = data[j];
                    reference_channel[i] = data[j + 1];
                }
                auto resampled_mic = std::vector<int16_t>(input_resampler_.GetOutputSamples(mic_channel.size()));
                auto resampled_reference = std::vector<int16_t>(reference_resampler_.GetOutputSamples(reference_channel.size()));
                input_resampler_.Process(mic_channel.data(), mic_channel.size(), resampled_mic.data());
                reference_resampler_.Process(reference_channel.data(), reference_channel.size(), resampled_reference.data());
                data.resize(resampled_mic.size() + resampled_reference.size());
                for (size_t i = 0, j = 0; i < resampled_mic.size(); ++i, j += 2) {
                    data[j] = resampled_mic[i];
                    data[j + 1] = resampled_reference[i];
                }
            } else {
                auto resampled = std::vector<int16_t>(input_resampler_.GetOutputSamples(data.size()));
                input_resampler_.Process(data.data(), data.size(), resampled.data());
                data = std::move(resampled);
            }
        }
#ifdef CONFIG_USE_AFE_SR
        if (audio_processor_.IsRunning()) {
            audio_processor_.Input(data);
        }
        if (wake_word_detect_.IsDetectionRunning()) {
            wake_word_detect_.Feed(data);
        }
#else
        Schedule([this, data = std::move(data)]() {
            if (chat_state_ == kChatStateListening) {
                std::lock_guard<std::mutex> lock(mutex_);
                audio_encode_queue_.emplace_back(std::move(data));
                cv_.notify_all();
            }
        });
#endif
    });

    // OPUS encoder / decoder use a lot of stack memory
    const size_t opus_stack_size = 4096 * 8;
    audio_encode_task_stack_ = (StackType_t*)heap_caps_malloc(opus_stack_size, MALLOC_CAP_SPIRAM);
    audio_encode_task_ = xTaskCreateStatic([](void* arg) {
        Application* app = (Application*)arg;
        app->AudioEncodeTask();
        vTaskDelete(NULL);
    }, "opus_encode", opus_stack_size, this, 1, audio_encode_task_stack_, &audio_encode_task_buffer_);

    xTaskCreate([](void* arg) {
        Application* app = (Application*)arg;
        app->AudioPlayTask();
        vTaskDelete(NULL);
    }, "play_audio", 4096 * 4, this, 4, NULL);

    board.StartNetwork();
    firmware_upgrade_.SetPostData(board.GetJson());
    // Blink the LED to indicate the device is running
    builtin_led.SetGreen();
    builtin_led.BlinkOnce();

    boot_button_.OnClick([this]() {
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
                AbortSpeaking();
            } else if (chat_state_ == kChatStateListening) {
                if (ws_client_ && ws_client_->IsConnected()) {
                    ws_client_->Close();
                }
            }
        });
    });

    volume_up_button_.OnClick([this]() {
        Schedule([this]() {
            auto volume = audio_device_->output_volume() + 10;
            if (volume > 100) {
                volume = 100;
            }
            audio_device_->SetOutputVolume(volume);
            display_.ShowNotification("Volume\n" + std::to_string(volume));
        });
    });

    volume_up_button_.OnLongPress([this]() {
        Schedule([this]() {
            audio_device_->SetOutputVolume(100);
            display_.ShowNotification("Volume\n100");
        });
    });

    volume_down_button_.OnClick([this]() {
        Schedule([this]() {
            auto volume = audio_device_->output_volume() - 10;
            if (volume < 0) {
                volume = 0;
            }
            audio_device_->SetOutputVolume(volume);
            display_.ShowNotification("Volume\n" + std::to_string(volume));
        });
    });

    volume_down_button_.OnLongPress([this]() {
        Schedule([this]() {
            audio_device_->SetOutputVolume(0);
            display_.ShowNotification("Volume\n0");
        });
    });

    const size_t main_loop_stack_size = 4096 * 2;
    main_loop_task_stack_ = (StackType_t*)heap_caps_malloc(main_loop_stack_size, MALLOC_CAP_SPIRAM);
    xTaskCreateStatic([](void* arg) {
        Application* app = (Application*)arg;
        app->MainLoop();
        vTaskDelete(NULL);
    }, "main_loop", main_loop_stack_size, this, 1, main_loop_task_stack_, &main_loop_task_buffer_);

    // Launch a task to check for new firmware version
    xTaskCreate([](void* arg) {
        Application* app = (Application*)arg;
        app->CheckNewVersion();
        vTaskDelete(NULL);
    }, "check_new_version", 4096 * 2, this, 1, NULL);

#ifdef CONFIG_USE_AFE_SR
    wake_word_detect_.Initialize(audio_device_->input_channels(), audio_device_->input_reference());
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
                AbortSpeaking();
            }

            // Resume detection
            wake_word_detect_.StartDetection();
        });
    });
    wake_word_detect_.StartDetection();

    audio_processor_.Initialize(audio_device_->input_channels(), audio_device_->input_reference());
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

    SetChatState(kChatStateIdle);
    display_.UpdateDisplay();
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

void Application::AbortSpeaking() {
    ESP_LOGI(TAG, "Abort speaking");
    skip_to_end_ = true;

    if (ws_client_ && ws_client_->IsConnected()) {
        cJSON* root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "type", "abort");
        char* json = cJSON_PrintUnformatted(root);

        std::lock_guard<std::mutex> lock(mutex_);
        ws_client_->Send(json);
        cJSON_Delete(root);
        free(json);
    }
}

void Application::SetChatState(ChatState state) {
    const char* state_str[] = {
        "unknown",
        "idle",
        "connecting",
        "listening",
        "speaking",
        "wake_word_detected",
        "testing",
        "upgrading",
        "invalid_state"
    };
    if (chat_state_ == state) {
        // No need to update the state
        return;
    }
    chat_state_ = state;
    ESP_LOGI(TAG, "STATE: %s", state_str[chat_state_]);

    auto& builtin_led = BuiltinLed::GetInstance();
    switch (chat_state_) {
        case kChatStateUnknown:
        case kChatStateIdle:
            builtin_led.TurnOff();
            audio_device_->EnableOutput(false);
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
            audio_device_->EnableOutput(true);
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

BinaryProtocol3* Application::AllocateBinaryProtocol3(const uint8_t* payload, size_t payload_size) {
    auto protocol = (BinaryProtocol3*)heap_caps_malloc(sizeof(BinaryProtocol3) + payload_size, MALLOC_CAP_SPIRAM);
    assert(protocol != nullptr);
    protocol->type = 0;
    protocol->reserved = 0;
    protocol->payload_size = htons(payload_size);
    assert(sizeof(BinaryProtocol3) == 4UL);
    memcpy(protocol->payload, payload, payload_size);
    return protocol;
}

void Application::AudioEncodeTask() {
    ESP_LOGI(TAG, "Audio encode task started");
    const int max_audio_play_queue_size_ = 2; // avoid decoding too fast

    while (true) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this]() {
            return !audio_encode_queue_.empty() || (!audio_decode_queue_.empty() && audio_play_queue_.size() < max_audio_play_queue_size_);
        });

        if (!audio_encode_queue_.empty()) {
            auto pcm = std::move(audio_encode_queue_.front());
            audio_encode_queue_.pop_front();
            lock.unlock();

            // Encode audio data
            opus_encoder_.Encode(pcm, [this](const uint8_t* opus, size_t opus_size) {
                auto protocol = AllocateBinaryProtocol3(opus, opus_size);
                Schedule([this, protocol, opus_size]() {
                    if (ws_client_ && ws_client_->IsConnected()) {
                        if (!ws_client_->Send(protocol, sizeof(BinaryProtocol3) + opus_size, true)) {
                            ESP_LOGE(TAG, "Failed to send audio data");
                        }
                    }
                    heap_caps_free(protocol);
                });
            });
        } else if (!audio_decode_queue_.empty()) {
            auto packet = std::move(audio_decode_queue_.front());
            audio_decode_queue_.pop_front();
            lock.unlock();

            if (packet->type == kAudioPacketTypeData && !skip_to_end_) {
                int frame_size = opus_decode_sample_rate_ * opus_duration_ms_ / 1000;
                packet->pcm.resize(frame_size);

                int ret = opus_decode(opus_decoder_, packet->opus.data(), packet->opus.size(), packet->pcm.data(), frame_size, 0);
                if (ret < 0) {
                    ESP_LOGE(TAG, "Failed to decode audio, error code: %d", ret);
                    delete packet;
                    continue;
                }

                if (opus_decode_sample_rate_ != AUDIO_OUTPUT_SAMPLE_RATE) {
                    int target_size = output_resampler_.GetOutputSamples(frame_size);
                    std::vector<int16_t> resampled(target_size);
                    output_resampler_.Process(packet->pcm.data(), frame_size, resampled.data());
                    packet->pcm = std::move(resampled);
                }
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
        audio_device_->OutputData(packet->pcm);
        break;
    }
    case kAudioPacketTypeStart:
        break_speaking_ = false;
        skip_to_end_ = false;
        Schedule([this]() {
            SetChatState(kChatStateSpeaking);
        });
        break;
    case kAudioPacketTypeStop:
        Schedule([this]() {
            if (ws_client_ && ws_client_->IsConnected()) {
                SetChatState(kChatStateListening);
            } else {
                SetChatState(kChatStateIdle);
            }
        });
        break;
    case kAudioPacketTypeSentenceStart:
        ESP_LOGI(TAG, "<< %s", packet->text.c_str());
        break;
    case kAudioPacketTypeSentenceEnd:
        if (break_speaking_) {
            skip_to_end_ = true;
        }
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
        cv_.notify_all();
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
    if (opus_decode_sample_rate_ != AUDIO_OUTPUT_SAMPLE_RATE) {
        ESP_LOGI(TAG, "Resampling audio from %d to %d", opus_decode_sample_rate_, AUDIO_OUTPUT_SAMPLE_RATE);
        output_resampler_.Configure(opus_decode_sample_rate_, AUDIO_OUTPUT_SAMPLE_RATE);
    }
}

void Application::ParseBinaryProtocol3(const char* data, size_t size) {
    for (const char* p = data; p < data + size; ) {
        auto protocol = (BinaryProtocol3*)p;
        p += sizeof(BinaryProtocol3);

        auto packet = new AudioPacket();
        packet->type = kAudioPacketTypeData;
        auto payload_size = ntohs(protocol->payload_size);
        packet->opus.resize(payload_size);
        memcpy(packet->opus.data(), protocol->payload, payload_size);
        p += payload_size;

        std::lock_guard<std::mutex> lock(mutex_);
        audio_decode_queue_.push_back(packet);
    }
}

void Application::StartWebSocketClient() {
    if (ws_client_ != nullptr) {
        ESP_LOGW(TAG, "WebSocket client already exists");
        delete ws_client_;
    }

    std::string url = CONFIG_WEBSOCKET_URL;
    std::string token = "Bearer " + std::string(CONFIG_WEBSOCKET_ACCESS_TOKEN);
    ws_client_ = Board::GetInstance().CreateWebSocket();
    ws_client_->SetHeader("Authorization", token.c_str());
    ws_client_->SetHeader("Protocol-Version", std::to_string(PROTOCOL_VERSION).c_str());
    ws_client_->SetHeader("Device-Id", SystemInfo::GetMacAddress().c_str());

    ws_client_->OnConnected([this]() {
        ESP_LOGI(TAG, "Websocket connected");
        
        // Send hello message to describe the client
        // keys: message type, version, wakeup_model, audio_params (format, sample_rate, channels)
        std::string message = "{";
        message += "\"type\":\"hello\",";
        message += "\"audio_params\":{";
        message += "\"format\":\"opus\", \"sample_rate\":16000, \"channels\":1";
        message += "}}";
        ws_client_->Send(message);
    });

    ws_client_->OnData([this](const char* data, size_t len, bool binary) {
        if (binary) {
            ParseBinaryProtocol3(data, len);
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

                        // If the device is speaking, we need to skip the last session
                        skip_to_end_ = true;
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
                } else if (strcmp(type->valuestring, "llm") == 0) {
                    auto emotion = cJSON_GetObjectItem(root, "emotion");
                    if (emotion != NULL) {
                        ESP_LOGD(TAG, "EMOTION: %s", emotion->valuestring);
                    }
                } else {
                    ESP_LOGW(TAG, "Unknown message type: %s", type->valuestring);
                }
            } else {
                ESP_LOGE(TAG, "Missing message type, data: %s", data);
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

    if (!ws_client_->Connect(url.c_str())) {
        ESP_LOGE(TAG, "Failed to connect to websocket server");
        return;
    }
}