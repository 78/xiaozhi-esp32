#include "application.h"
#include "system_info.h"
#include "ml307_ssl_transport.h"
#include "audio_codec.h"
#include "mqtt_protocol.h"
#include "websocket_protocol.h"
#include "font_awesome_symbols.h"

#include <cstring>
#include <esp_log.h>
#include <cJSON.h>
#include <driver/gpio.h>
#include <arpa/inet.h>

#define TAG "Application"

extern const char p3_err_reg_start[] asm("_binary_err_reg_p3_start");
extern const char p3_err_reg_end[] asm("_binary_err_reg_p3_end");
extern const char p3_err_pin_start[] asm("_binary_err_pin_p3_start");
extern const char p3_err_pin_end[] asm("_binary_err_pin_p3_end");
extern const char p3_err_wificonfig_start[] asm("_binary_err_wificonfig_p3_start");
extern const char p3_err_wificonfig_end[] asm("_binary_err_wificonfig_p3_end");


Application::Application() : background_task_(4096 * 8) {
    event_group_ = xEventGroupCreate();

    ota_.SetCheckVersionUrl(CONFIG_OTA_VERSION_URL);
    ota_.SetHeader("Device-Id", SystemInfo::GetMacAddress().c_str());
}

Application::~Application() {
    if (protocol_ != nullptr) {
        delete protocol_;
    }
    if (opus_decoder_ != nullptr) {
        opus_decoder_destroy(opus_decoder_);
    }

    vEventGroupDelete(event_group_);
}

void Application::CheckNewVersion() {
    auto& board = Board::GetInstance();
    auto display = board.GetDisplay();
    // Check if there is a new firmware version available
    ota_.SetPostData(board.GetJson());

    while (true) {
        if (ota_.CheckVersion()) {
            if (ota_.HasNewVersion()) {
                // Wait for the chat state to be idle
                do {
                    vTaskDelay(pdMS_TO_TICKS(3000));
                } while (GetChatState() != kChatStateIdle);

                SetChatState(kChatStateUpgrading);
                
                display->SetIcon(FONT_AWESOME_DOWNLOAD);
                display->SetStatus("新版本 " + ota_.GetFirmwareVersion());

                // 预先关闭音频输出，避免升级过程有音频操作
                board.GetAudioCodec()->EnableOutput(false);

                ota_.StartUpgrade([display](int progress, size_t speed) {
                    char buffer[64];
                    snprintf(buffer, sizeof(buffer), "%d%% %zuKB/s", progress, speed / 1024);
                    display->SetStatus(buffer);
                });

                // If upgrade success, the device will reboot and never reach here
                ESP_LOGI(TAG, "Firmware upgrade failed...");
                SetChatState(kChatStateIdle);
            } else {
                ota_.MarkCurrentVersionValid();
                display->ShowNotification("版本 " + ota_.GetCurrentVersion());
            }
            return;
        }

        // Check again in 60 seconds
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}

void Application::Alert(const std::string&& title, const std::string&& message) {
    ESP_LOGW(TAG, "Alert: %s, %s", title.c_str(), message.c_str());
    auto display = Board::GetInstance().GetDisplay();
    display->ShowNotification(message);

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
    for (const char* p = data; p < data + size; ) {
        auto p3 = (BinaryProtocol3*)p;
        p += sizeof(BinaryProtocol3);

        auto payload_size = ntohs(p3->payload_size);
        std::string opus;
        opus.resize(payload_size);
        memcpy(opus.data(), p3->payload, payload_size);
        p += payload_size;

        std::lock_guard<std::mutex> lock(mutex_);
        audio_decode_queue_.emplace_back(std::move(opus));
    }
}

void Application::ToggleChatState() {
    Schedule([this]() {
        if (chat_state_ == kChatStateIdle) {
            SetChatState(kChatStateConnecting);
            if (!protocol_->OpenAudioChannel()) {
                ESP_LOGE(TAG, "Failed to open audio channel");
                SetChatState(kChatStateIdle);
                return;
            }

            keep_listening_ = true;
            protocol_->SendStartListening(kListeningModeAutoStop);
            SetChatState(kChatStateListening);
        } else if (chat_state_ == kChatStateSpeaking) {
            AbortSpeaking(kAbortReasonNone);
        } else if (chat_state_ == kChatStateListening) {
            protocol_->CloseAudioChannel();
        }
    });
}

void Application::StartListening() {
    Schedule([this]() {
        keep_listening_ = false;
        if (chat_state_ == kChatStateIdle) {
            if (!protocol_->IsAudioChannelOpened()) {
                SetChatState(kChatStateConnecting);
                if (!protocol_->OpenAudioChannel()) {
                    SetChatState(kChatStateIdle);
                    ESP_LOGE(TAG, "Failed to open audio channel");
                    return;
                }
            }
            protocol_->SendStartListening(kListeningModeManualStop);
            SetChatState(kChatStateListening);
        } else if (chat_state_ == kChatStateSpeaking) {
            AbortSpeaking(kAbortReasonNone);
            protocol_->SendStartListening(kListeningModeManualStop);
            // FIXME: Wait for the speaker to empty the buffer
            vTaskDelay(pdMS_TO_TICKS(120));
            SetChatState(kChatStateListening);
        }
    });
}

void Application::StopListening() {
    Schedule([this]() {
        if (chat_state_ == kChatStateListening) {
            protocol_->SendStopListening();
            SetChatState(kChatStateIdle);
        }
    });
}

void Application::Start() {
    auto& board = Board::GetInstance();
    board.Initialize();

    auto builtin_led = board.GetBuiltinLed();
    builtin_led->SetBlue();
    builtin_led->StartContinuousBlink(100);

    /* Setup the display */
    auto display = board.GetDisplay();

    /* Setup the audio codec */
    auto codec = board.GetAudioCodec();
    opus_decode_sample_rate_ = codec->output_sample_rate();
    opus_decoder_ = opus_decoder_create(opus_decode_sample_rate_, 1, NULL);
    opus_encoder_.Configure(16000, 1, OPUS_FRAME_DURATION_MS);
    if (codec->input_sample_rate() != 16000) {
        input_resampler_.Configure(codec->input_sample_rate(), 16000);
        reference_resampler_.Configure(codec->input_sample_rate(), 16000);
    }
    codec->OnInputReady([this, codec]() {
        BaseType_t higher_priority_task_woken = pdFALSE;
        xEventGroupSetBitsFromISR(event_group_, AUDIO_INPUT_READY_EVENT, &higher_priority_task_woken);
        return higher_priority_task_woken == pdTRUE;
    });
    codec->OnOutputReady([this]() {
        BaseType_t higher_priority_task_woken = pdFALSE;
        xEventGroupSetBitsFromISR(event_group_, AUDIO_OUTPUT_READY_EVENT, &higher_priority_task_woken);
        return higher_priority_task_woken == pdTRUE;
    });
    codec->Start();

    /* Start the main loop */
    xTaskCreate([](void* arg) {
        Application* app = (Application*)arg;
        app->MainLoop();
        vTaskDelete(NULL);
    }, "main_loop", 4096 * 2, this, 2, nullptr);

    /* Wait for the network to be ready */
    board.StartNetwork();

    // Check for new firmware version or get the MQTT broker address
    xTaskCreate([](void* arg) {
        Application* app = (Application*)arg;
        app->CheckNewVersion();
        vTaskDelete(NULL);
    }, "check_new_version", 4096 * 2, this, 1, nullptr);

#if CONFIG_IDF_TARGET_ESP32S3
    audio_processor_.Initialize(codec->input_channels(), codec->input_reference());
    audio_processor_.OnOutput([this](std::vector<int16_t>&& data) {
        background_task_.Schedule([this, data = std::move(data)]() {
            opus_encoder_.Encode(data, [this](const uint8_t* opus, size_t opus_size) {
                Schedule([this, opus = std::string(reinterpret_cast<const char*>(opus), opus_size)]() {
                    protocol_->SendAudio(opus);
                });
            });
        });
    });

    wake_word_detect_.Initialize(codec->input_channels(), codec->input_reference());
    wake_word_detect_.OnVadStateChange([this](bool speaking) {
        Schedule([this, speaking]() {
            auto builtin_led = Board::GetInstance().GetBuiltinLed();
            if (chat_state_ == kChatStateListening) {
                if (speaking) {
                    builtin_led->SetRed(HIGH_BRIGHTNESS);
                } else {
                    builtin_led->SetRed(LOW_BRIGHTNESS);
                }
                builtin_led->TurnOn();
            }
        });
    });

    wake_word_detect_.OnWakeWordDetected([this](const std::string& wake_word) {
        Schedule([this, &wake_word]() {
            if (chat_state_ == kChatStateIdle) {
                SetChatState(kChatStateConnecting);
                wake_word_detect_.EncodeWakeWordData();

                if (!protocol_->OpenAudioChannel()) {
                    ESP_LOGE(TAG, "Failed to open audio channel");
                    SetChatState(kChatStateIdle);
                    wake_word_detect_.StartDetection();
                    return;
                }
                
                std::string opus;
                // Encode and send the wake word data to the server
                while (wake_word_detect_.GetWakeWordOpus(opus)) {
                    protocol_->SendAudio(opus);
                }
                // Set the chat state to wake word detected
                protocol_->SendWakeWordDetected(wake_word);
                ESP_LOGI(TAG, "Wake word detected: %s", wake_word.c_str());
                keep_listening_ = true;
                SetChatState(kChatStateListening);
            } else if (chat_state_ == kChatStateSpeaking) {
                AbortSpeaking(kAbortReasonWakeWordDetected);
            }

            // Resume detection
            wake_word_detect_.StartDetection();
        });
    });
    wake_word_detect_.StartDetection();
#endif

    // Initialize the protocol
    display->SetStatus("初始化协议");
#ifdef CONFIG_CONNECTION_TYPE_WEBSOCKET
    protocol_ = new WebsocketProtocol();
#else
    protocol_ = new MqttProtocol();
#endif
    protocol_->OnNetworkError([this](const std::string& message) {
        Alert("Error", std::move(message));
    });
    protocol_->OnIncomingAudio([this](const std::string& data) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (chat_state_ == kChatStateSpeaking) {
            audio_decode_queue_.emplace_back(std::move(data));
        }
    });
    protocol_->OnAudioChannelOpened([this, codec, &board]() {
        if (protocol_->server_sample_rate() != codec->output_sample_rate()) {
            ESP_LOGW(TAG, "服务器的音频采样率 %d 与设备输出的采样率 %d 不一致，重采样后可能会失真",
                protocol_->server_sample_rate(), codec->output_sample_rate());
        }
        SetDecodeSampleRate(protocol_->server_sample_rate());
        board.SetPowerSaveMode(false);
    });
    protocol_->OnAudioChannelClosed([this, &board]() {
        Schedule([this]() {
            SetChatState(kChatStateIdle);
        });
        board.SetPowerSaveMode(true);
    });
    protocol_->OnIncomingJson([this, display](const cJSON* root) {
        // Parse JSON data
        auto type = cJSON_GetObjectItem(root, "type");
        if (strcmp(type->valuestring, "tts") == 0) {
            auto state = cJSON_GetObjectItem(root, "state");
            if (strcmp(state->valuestring, "start") == 0) {
                Schedule([this]() {
                    aborted_ = false;
                    if (chat_state_ == kChatStateIdle || chat_state_ == kChatStateListening) {
                        SetChatState(kChatStateSpeaking);
                    }
                });
            } else if (strcmp(state->valuestring, "stop") == 0) {
                Schedule([this]() {
                    if (chat_state_ == kChatStateSpeaking) {
                        background_task_.WaitForCompletion();
                        if (keep_listening_) {
                            protocol_->SendStartListening(kListeningModeAutoStop);
                            SetChatState(kChatStateListening);
                        } else {
                            SetChatState(kChatStateIdle);
                        }
                    }
                });
            } else if (strcmp(state->valuestring, "sentence_start") == 0) {
                auto text = cJSON_GetObjectItem(root, "text");
                if (text != NULL) {
                    ESP_LOGI(TAG, "<< %s", text->valuestring);
                    display->SetChatMessage("assistant", text->valuestring);
                }
            }
        } else if (strcmp(type->valuestring, "stt") == 0) {
            auto text = cJSON_GetObjectItem(root, "text");
            if (text != NULL) {
                ESP_LOGI(TAG, ">> %s", text->valuestring);
                display->SetChatMessage("user", text->valuestring);
            }
        } else if (strcmp(type->valuestring, "llm") == 0) {
            auto emotion = cJSON_GetObjectItem(root, "emotion");
            if (emotion != NULL) {
                display->SetEmotion(emotion->valuestring);
            }
        }
    });

    // Blink the LED to indicate the device is running
    display->SetStatus("待命");
    builtin_led->SetGreen();
    builtin_led->BlinkOnce();

    SetChatState(kChatStateIdle);
}

void Application::Schedule(std::function<void()> callback) {
    mutex_.lock();
    main_tasks_.push_back(callback);
    mutex_.unlock();
    xEventGroupSetBits(event_group_, SCHEDULE_EVENT);
}

// The Main Loop controls the chat state and websocket connection
// If other tasks need to access the websocket or chat state,
// they should use Schedule to call this function
void Application::MainLoop() {
    while (true) {
        auto bits = xEventGroupWaitBits(event_group_,
            SCHEDULE_EVENT | AUDIO_INPUT_READY_EVENT | AUDIO_OUTPUT_READY_EVENT,
            pdTRUE, pdFALSE, portMAX_DELAY);

        if (bits & AUDIO_INPUT_READY_EVENT) {
            InputAudio();
        }
        if (bits & AUDIO_OUTPUT_READY_EVENT) {
            OutputAudio();
        }
        if (bits & SCHEDULE_EVENT) {
            mutex_.lock();
            std::list<std::function<void()>> tasks = std::move(main_tasks_);
            mutex_.unlock();
            for (auto& task : tasks) {
                task();
            }
        }
    }
}

void Application::ResetDecoder() {
    std::lock_guard<std::mutex> lock(mutex_);
    opus_decoder_ctl(opus_decoder_, OPUS_RESET_STATE);
    audio_decode_queue_.clear();
    last_output_time_ = std::chrono::steady_clock::now();
    Board::GetInstance().GetAudioCodec()->EnableOutput(true);
}

void Application::OutputAudio() {
    auto now = std::chrono::steady_clock::now();
    auto codec = Board::GetInstance().GetAudioCodec();
    const int max_silence_seconds = 10;

    std::unique_lock<std::mutex> lock(mutex_);
    if (audio_decode_queue_.empty()) {
        // Disable the output if there is no audio data for a long time
        if (chat_state_ == kChatStateIdle) {
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - last_output_time_).count();
            if (duration > max_silence_seconds) {
                codec->EnableOutput(false);
            }
        }
        return;
    }

    if (chat_state_ == kChatStateListening) {
        audio_decode_queue_.clear();
        return;
    }

    last_output_time_ = now;
    auto opus = std::move(audio_decode_queue_.front());
    audio_decode_queue_.pop_front();
    lock.unlock();

    background_task_.Schedule([this, codec, opus = std::move(opus)]() {
        if (aborted_) {
            return;
        }
        int frame_size = opus_decode_sample_rate_ * OPUS_FRAME_DURATION_MS / 1000;
        std::vector<int16_t> pcm(frame_size);

        int ret = opus_decode(opus_decoder_, (const unsigned char*)opus.data(), opus.size(), pcm.data(), frame_size, 0);
        if (ret < 0) {
            ESP_LOGE(TAG, "Failed to decode audio, error code: %d", ret);
            return;
        }

        // Resample if the sample rate is different
        if (opus_decode_sample_rate_ != codec->output_sample_rate()) {
            int target_size = output_resampler_.GetOutputSamples(frame_size);
            std::vector<int16_t> resampled(target_size);
            output_resampler_.Process(pcm.data(), frame_size, resampled.data());
            pcm = std::move(resampled);
        }
        
        codec->OutputData(pcm);
    });
}

void Application::InputAudio() {
    auto codec = Board::GetInstance().GetAudioCodec();
    std::vector<int16_t> data;
    if (!codec->InputData(data)) {
        return;
    }

    if (codec->input_sample_rate() != 16000) {
        if (codec->input_channels() == 2) {
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
    
#if CONFIG_IDF_TARGET_ESP32S3
    if (audio_processor_.IsRunning()) {
        audio_processor_.Input(data);
    }
    if (wake_word_detect_.IsDetectionRunning()) {
        wake_word_detect_.Feed(data);
    }
#else
    if (chat_state_ == kChatStateListening) {
        background_task_.Schedule([this, data = std::move(data)]() {
            opus_encoder_.Encode(data, [this](const uint8_t* opus, size_t opus_size) {
                Schedule([this, opus = std::string(reinterpret_cast<const char*>(opus), opus_size)]() {
                    protocol_->SendAudio(opus);
                });
            });
        });
    }
#endif
}

void Application::AbortSpeaking(AbortReason reason) {
    ESP_LOGI(TAG, "Abort speaking");
    aborted_ = true;
    protocol_->SendAbortSpeaking(reason);
}

void Application::SetChatState(ChatState state) {
    const char* state_str[] = {
        "unknown",
        "idle",
        "connecting",
        "listening",
        "speaking",
        "upgrading",
        "invalid_state"
    };
    if (chat_state_ == state) {
        // No need to update the state
        return;
    }

    chat_state_ = state;
    ESP_LOGI(TAG, "STATE: %s", state_str[chat_state_]);
    // The state is changed, wait for all background tasks to finish
    background_task_.WaitForCompletion();

    auto display = Board::GetInstance().GetDisplay();
    auto builtin_led = Board::GetInstance().GetBuiltinLed();
    switch (state) {
        case kChatStateUnknown:
        case kChatStateIdle:
            builtin_led->TurnOff();
            display->SetStatus("待命");
            display->SetEmotion("neutral");
#ifdef CONFIG_IDF_TARGET_ESP32S3
            audio_processor_.Stop();
#endif
            break;
        case kChatStateConnecting:
            builtin_led->SetBlue();
            builtin_led->TurnOn();
            display->SetStatus("连接中...");
            break;
        case kChatStateListening:
            builtin_led->SetRed();
            builtin_led->TurnOn();
            display->SetStatus("聆听中...");
            display->SetEmotion("neutral");
            ResetDecoder();
            opus_encoder_.ResetState();
#if CONFIG_IDF_TARGET_ESP32S3
            audio_processor_.Start();
#endif
            break;
        case kChatStateSpeaking:
            builtin_led->SetGreen();
            builtin_led->TurnOn();
            display->SetStatus("说话中...");
            ResetDecoder();
#if CONFIG_IDF_TARGET_ESP32S3
            audio_processor_.Stop();
#endif
            break;
        case kChatStateUpgrading:
            builtin_led->SetGreen();
            builtin_led->StartContinuousBlink(100);
            break;
        default:
            ESP_LOGE(TAG, "Invalid chat state: %d", chat_state_);
            return;
    }
}

void Application::SetDecodeSampleRate(int sample_rate) {
    if (opus_decode_sample_rate_ == sample_rate) {
        return;
    }

    opus_decoder_destroy(opus_decoder_);
    opus_decode_sample_rate_ = sample_rate;
    opus_decoder_ = opus_decoder_create(opus_decode_sample_rate_, 1, NULL);

    auto codec = Board::GetInstance().GetAudioCodec();
    if (opus_decode_sample_rate_ != codec->output_sample_rate()) {
        ESP_LOGI(TAG, "Resampling audio from %d to %d", opus_decode_sample_rate_, codec->output_sample_rate());
        output_resampler_.Configure(opus_decode_sample_rate_, codec->output_sample_rate());
    }
}
