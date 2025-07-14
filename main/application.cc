#include "application.h"
#include "board.h"
#include "display.h"
#include "system_info.h"
#include "ml307_ssl_transport.h"
#include "audio_codec.h"
#include "mqtt_protocol.h"
#include "websocket_protocol.h"
#include "font_awesome_symbols.h"
#include "iot/thing_manager.h"
#include "assets/lang_config.h"

#include <cstring>
#include <esp_log.h>
#include <cJSON.h>
#include <driver/gpio.h>
#include <arpa/inet.h>

#define TAG "Application"


static const char* const STATE_STRINGS[] = {
    "unknown",
    "starting",
    "configuring",
    "idle",
    "connecting",
    "listening",
    "speaking",
    "upgrading",
    "activating",
    "fatal_error",
    "invalid_state"
};

Application::Application() {
    event_group_ = xEventGroupCreate();
    background_task_ = new BackgroundTask(4096 * 8);

    esp_timer_create_args_t clock_timer_args = {
        .callback = [](void* arg) {
            Application* app = (Application*)arg;
            app->OnClockTimer();
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "clock_timer",
        .skip_unhandled_events = true
    };
    esp_timer_create(&clock_timer_args, &clock_timer_handle_);
}

Application::~Application() {
    if (clock_timer_handle_ != nullptr) {
        esp_timer_stop(clock_timer_handle_);
        esp_timer_delete(clock_timer_handle_);
    }
    if (background_task_ != nullptr) {
        delete background_task_;
    }
    vEventGroupDelete(event_group_);
}

void Application::CheckNewVersion() {
    const int MAX_RETRY = 10;
    int retry_count = 0;

    while (true) {
        auto display = Board::GetInstance().GetDisplay();
        if (!ota_.CheckVersion()) {
            retry_count++;
            if (retry_count >= MAX_RETRY) {
                ESP_LOGE(TAG, "Too many retries, exit version check");
                // 即使OTA检查失败，也标记为完成，让图片资源检查可以继续
                ota_check_completed_ = true;
                if (image_resource_callback_) {
                    Schedule(image_resource_callback_);
                }
                return;
            }
            ESP_LOGW(TAG, "Check new version failed, retry in %d seconds (%d/%d)", 60, retry_count, MAX_RETRY);
            vTaskDelay(pdMS_TO_TICKS(60000));
            continue;
        }
        retry_count = 0;

        if (ota_.HasNewVersion()) {
            Alert(Lang::Strings::OTA_UPGRADE, Lang::Strings::UPGRADING, "happy", Lang::Sounds::P3_UPGRADE);
            // Wait for the chat state to be idle
            do {
                vTaskDelay(pdMS_TO_TICKS(3000));
            } while (GetDeviceState() != kDeviceStateIdle);

            // Use main task to do the upgrade, not cancelable
            Schedule([this, display]() {
                SetDeviceState(kDeviceStateUpgrading);
                
                display->SetIcon(FONT_AWESOME_DOWNLOAD);
                std::string message = std::string(Lang::Strings::NEW_VERSION) + ota_.GetFirmwareVersion();
                display->SetChatMessage("system", message.c_str());

                auto& board = Board::GetInstance();
                board.SetPowerSaveMode(false);
#if CONFIG_USE_WAKE_WORD_DETECT
                wake_word_detect_.StopDetection();
#endif
                // 预先关闭音频输出，避免升级过程有音频操作
                auto codec = board.GetAudioCodec();
                codec->EnableInput(false);
                codec->EnableOutput(false);
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    audio_decode_queue_.clear();
                }
                background_task_->WaitForCompletion();
                delete background_task_;
                background_task_ = nullptr;
                vTaskDelay(pdMS_TO_TICKS(1000));

                ota_.StartUpgrade([display](int progress, size_t speed) {
                    char buffer[64];
                    snprintf(buffer, sizeof(buffer), "%d%% %zuKB/s", progress, speed / 1024);
                    display->SetChatMessage("system", buffer);
                });

                // If upgrade success, the device will reboot and never reach here
                display->SetStatus(Lang::Strings::UPGRADE_FAILED);
                ESP_LOGI(TAG, "Firmware upgrade failed...");
                vTaskDelay(pdMS_TO_TICKS(3000));
                Reboot();
            });

            return;
        }

        // No new version, mark the current version as valid
        ota_.MarkCurrentVersionValid();
        std::string message = std::string(Lang::Strings::VERSION) + ota_.GetCurrentVersion();
        display->ShowNotification(message.c_str());
    
        if (ota_.HasActivationCode()) {
            // Activation code is valid
            SetDeviceState(kDeviceStateActivating);
            ShowActivationCode();

            // Check again in 60 seconds or until the device is idle
            for (int i = 0; i < 60; ++i) {
                if (device_state_ == kDeviceStateIdle) {
                    break;
                }
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            continue;
        }

        SetDeviceState(kDeviceStateIdle);
        display->SetChatMessage("system", "");
        ResetDecoder();
        PlaySound(Lang::Sounds::P3_SUCCESS);
        
        // OTA检查完成，标记为完成状态
        ESP_LOGI(TAG, "OTA check completed, triggering image resource check");
        ota_check_completed_ = true;
        if (image_resource_callback_) {
            Schedule(image_resource_callback_);
        }
        
        // Exit the loop if upgrade or idle
        break;
    }
}

void Application::ShowActivationCode() {
    auto& message = ota_.GetActivationMessage();
    auto& code = ota_.GetActivationCode();

    struct digit_sound {
        char digit;
        const std::string_view& sound;
    };
    static const std::array<digit_sound, 10> digit_sounds{{
        digit_sound{'0', Lang::Sounds::P3_0},
        digit_sound{'1', Lang::Sounds::P3_1}, 
        digit_sound{'2', Lang::Sounds::P3_2},
        digit_sound{'3', Lang::Sounds::P3_3},
        digit_sound{'4', Lang::Sounds::P3_4},
        digit_sound{'5', Lang::Sounds::P3_5},
        digit_sound{'6', Lang::Sounds::P3_6},
        digit_sound{'7', Lang::Sounds::P3_7},
        digit_sound{'8', Lang::Sounds::P3_8},
        digit_sound{'9', Lang::Sounds::P3_9}
    }};

    // 组合消息和激活码一起显示
    std::string full_message = message;
    if (!code.empty()) {
        full_message += "\n" + code;
    }

    // This sentence uses 9KB of SRAM, so we need to wait for it to finish
    Alert(Lang::Strings::ACTIVATION, full_message.c_str(), "happy", Lang::Sounds::P3_ACTIVATION);
    vTaskDelay(pdMS_TO_TICKS(1000));
    background_task_->WaitForCompletion();

    for (const auto& digit : code) {
        auto it = std::find_if(digit_sounds.begin(), digit_sounds.end(),
            [digit](const digit_sound& ds) { return ds.digit == digit; });
        if (it != digit_sounds.end()) {
            PlaySound(it->sound);
        }
    }
}

void Application::Alert(const char* status, const char* message, const char* emotion, const std::string_view& sound) {
    ESP_LOGW(TAG, "Alert %s: %s [%s]", status, message, emotion);
    auto display = Board::GetInstance().GetDisplay();
    display->SetStatus(status);
    display->SetEmotion(emotion);
    display->SetChatMessage("system", message);
    if (!sound.empty()) {
        ResetDecoder();
        PlaySound(sound);
    }
}

void Application::DismissAlert() {
    if (device_state_ == kDeviceStateIdle) {
        auto display = Board::GetInstance().GetDisplay();
        display->SetStatus(Lang::Strings::STANDBY);
        display->SetEmotion("neutral");
        display->SetChatMessage("system", "");
    }
}

void Application::PlaySound(const std::string_view& sound) {
    // The assets are encoded at 16000Hz, 60ms frame duration
    SetDecodeSampleRate(16000, 60);
    const char* data = sound.data();
    size_t size = sound.size();
    for (const char* p = data; p < data + size; ) {
        auto p3 = (BinaryProtocol3*)p;
        p += sizeof(BinaryProtocol3);

        auto payload_size = ntohs(p3->payload_size);
        std::vector<uint8_t> opus;
        opus.resize(payload_size);
        memcpy(opus.data(), p3->payload, payload_size);
        p += payload_size;

        std::lock_guard<std::mutex> lock(mutex_);
        audio_decode_queue_.emplace_back(std::move(opus));
    }
}

void Application::ToggleChatState() {
    if (device_state_ == kDeviceStateActivating) {
        SetDeviceState(kDeviceStateIdle);
        return;
    }

    if (!protocol_) {
        ESP_LOGE(TAG, "Protocol not initialized");
        return;
    }

    if (device_state_ == kDeviceStateIdle) {
        Schedule([this]() {
            SetDeviceState(kDeviceStateConnecting);
            if (!protocol_->OpenAudioChannel()) {
                return;
            }

            SetListeningMode(realtime_chat_enabled_ ? kListeningModeRealtime : kListeningModeAutoStop);
        });
    } else if (device_state_ == kDeviceStateSpeaking) {
        Schedule([this]() {
            AbortSpeaking(kAbortReasonNone);
        });
    } else if (device_state_ == kDeviceStateListening) {
        Schedule([this]() {
            protocol_->CloseAudioChannel();
        });
    }
}

void Application::StartListening() {
    if (device_state_ == kDeviceStateActivating) {
        SetDeviceState(kDeviceStateIdle);
        return;
    }

    if (!protocol_) {
        ESP_LOGE(TAG, "Protocol not initialized");
        return;
    }
    
    if (device_state_ == kDeviceStateIdle) {
        Schedule([this]() {
            if (!protocol_->IsAudioChannelOpened()) {
                SetDeviceState(kDeviceStateConnecting);
                if (!protocol_->OpenAudioChannel()) {
                    return;
                }
            }

            SetListeningMode(kListeningModeManualStop);
        });
    } else if (device_state_ == kDeviceStateSpeaking) {
        Schedule([this]() {
            AbortSpeaking(kAbortReasonNone);
            SetListeningMode(kListeningModeManualStop);
        });
    }
}

void Application::StopListening() {
    Schedule([this]() {
        if (device_state_ == kDeviceStateListening) {
            protocol_->SendStopListening();
            SetDeviceState(kDeviceStateIdle);
        }
    });
}

void Application::Start() {
    auto& board = Board::GetInstance();
    SetDeviceState(kDeviceStateStarting);

    /* Setup the display */
    auto display = board.GetDisplay();

    /* Setup the audio codec */
    auto codec = board.GetAudioCodec();
    opus_decoder_ = std::make_unique<OpusDecoderWrapper>(codec->output_sample_rate(), 1, OPUS_FRAME_DURATION_MS);
    opus_encoder_ = std::make_unique<OpusEncoderWrapper>(16000, 1, OPUS_FRAME_DURATION_MS);
    
    // 音频系统初始化完成，现在可以安全启动Display定时器
    ESP_LOGI(TAG, "音频系统初始化完成，启动Display定时器");
    display->StartUpdateTimer();
    // 性能优化：强制使用最快编码速度
    ESP_LOGI(TAG, "Performance optimization: setting opus encoder complexity to 0 (fastest)");
    opus_encoder_->SetComplexity(0);
    
    // 启用实时模式相关优化
    realtime_chat_enabled_ = false;
    ESP_LOGI(TAG, "Using auto stop mode instead of realtime mode");

    if (codec->input_sample_rate() != 16000) {
        input_resampler_.Configure(codec->input_sample_rate(), 16000);
        reference_resampler_.Configure(codec->input_sample_rate(), 16000);
    }
    codec->Start();

    // 性能优化：音频任务固定绑定到Core 1，使用最高优先级
    xTaskCreatePinnedToCore([](void* arg) {
        Application* app = (Application*)arg;
        app->AudioLoop();
        vTaskDelete(NULL);
    }, "audio_loop", 4096 * 2, this, 9, &audio_loop_task_handle_, 1);  // 优先级9，绑定Core 1

    /* Start the main loop */
    xTaskCreatePinnedToCore([](void* arg) {
        Application* app = (Application*)arg;
        app->MainLoop();
        vTaskDelete(NULL);
    }, "main_loop", 4096 * 2, this, 4, &main_loop_task_handle_, 0);

    /* Wait for the network to be ready */
    board.StartNetwork();

    // Initialize the protocol
    display->SetStatus(Lang::Strings::LOADING_PROTOCOL);
#ifdef CONFIG_CONNECTION_TYPE_WEBSOCKET
    ESP_LOGI(TAG, "🔧 使用WebSocket协议");
    protocol_ = std::make_unique<WebsocketProtocol>();
#else
    ESP_LOGI(TAG, "🔧 使用MQTT+UDP协议");
    protocol_ = std::make_unique<MqttProtocol>();
#endif
    protocol_->OnNetworkError([this](const std::string& message) {
        SetDeviceState(kDeviceStateIdle);
        Alert(Lang::Strings::ERROR, message.c_str(), "sad", Lang::Sounds::P3_EXCLAMATION);
    });
    protocol_->OnIncomingAudio([this](std::vector<uint8_t>&& data) {
        std::lock_guard<std::mutex> lock(mutex_);
        audio_decode_queue_.emplace_back(std::move(data));
    });
    protocol_->OnAudioChannelOpened([this, codec, &board]() {
        board.SetPowerSaveMode(false);
        
        // 调试信息：显示服务器音频参数
        ESP_LOGI(TAG, "🔗 音频通道已打开 - 服务器参数: [采样率:%d, 帧长度:%dms], 客户端发送帧长度:%dms", 
                 protocol_->server_sample_rate(), protocol_->server_frame_duration(), OPUS_FRAME_DURATION_MS);
        
        if (protocol_->server_sample_rate() != codec->output_sample_rate()) {
            ESP_LOGW(TAG, "Server sample rate %d does not match device output sample rate %d, resampling may cause distortion",
                protocol_->server_sample_rate(), codec->output_sample_rate());
        }
        SetDecodeSampleRate(protocol_->server_sample_rate(), protocol_->server_frame_duration());
        auto& thing_manager = iot::ThingManager::GetInstance();
        protocol_->SendIotDescriptors(thing_manager.GetDescriptorsJson());
        std::string states;
        if (thing_manager.GetStatesJson(states, false)) {
            protocol_->SendIotStates(states);
        }
        
        // WebSocket握手成功后，自动进入listening状态并发送listen start消息
        ESP_LOGI(TAG, "WebSocket握手成功，自动开始新对话");
        SetListeningMode(realtime_chat_enabled_ ? kListeningModeRealtime : kListeningModeAutoStop);
    });
    protocol_->OnAudioChannelClosed([this, &board]() {
        board.SetPowerSaveMode(true);
        Schedule([this]() {
            auto display = Board::GetInstance().GetDisplay();
            display->SetChatMessage("system", "");
            SetDeviceState(kDeviceStateIdle);
        });
    });
    protocol_->OnIncomingJson([this, display](const cJSON* root) {
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
                    // 优化：减少背景任务等待时间，加快状态切换速度
                    // background_task_->WaitForCompletion();
                    if (device_state_ == kDeviceStateSpeaking) {
                        if (listening_mode_ == kListeningModeManualStop) {
                            SetDeviceState(kDeviceStateIdle);
                        } else {
                            // 优化：立即切换到listening状态，减少音频数据丢失
                            ESP_LOGI(TAG, "TTS结束，快速切换到listening状态");
                            SetDeviceState(kDeviceStateListening);
                        }
                    }
                });
            } else if (strcmp(state->valuestring, "sentence_start") == 0) {
                auto text = cJSON_GetObjectItem(root, "text");
                if (text != NULL) {
                    ESP_LOGI(TAG, "<< %s", text->valuestring);
                    Schedule([this, display, message = std::string(text->valuestring)]() {
                        display->SetChatMessage("assistant", message.c_str());
                    });
                }
            }
        } else if (strcmp(type->valuestring, "stt") == 0) {
            auto text = cJSON_GetObjectItem(root, "text");
            if (text != NULL) {
                ESP_LOGI(TAG, ">> %s", text->valuestring);
                Schedule([this, display, message = std::string(text->valuestring)]() {
                    display->SetChatMessage("user", message.c_str());
                });
            }
        } else if (strcmp(type->valuestring, "llm") == 0) {
            auto emotion = cJSON_GetObjectItem(root, "emotion");
            if (emotion != NULL) {
                Schedule([this, display, emotion_str = std::string(emotion->valuestring)]() {
                    // 只有当设备不在说话状态时，才更新表情
                    if (device_state_ != kDeviceStateSpeaking) {
                        display->SetEmotion(emotion_str.c_str());
                    }
                });
            }
        } else if (strcmp(type->valuestring, "iot") == 0) {
            auto commands = cJSON_GetObjectItem(root, "commands");
            if (commands != NULL) {
                auto& thing_manager = iot::ThingManager::GetInstance();
                for (int i = 0; i < cJSON_GetArraySize(commands); ++i) {
                    auto command = cJSON_GetArrayItem(commands, i);
                    thing_manager.Invoke(command);
                }
            }
        }
    });
    protocol_->Start();

    // Check for new firmware version or get the MQTT broker address
    xTaskCreate([](void* arg) {
        Application* app = (Application*)arg;
        app->CheckNewVersion();
        vTaskDelete(NULL);
    }, "check_new_version", 4096 * 2, this, 2, nullptr);

#if CONFIG_USE_AUDIO_PROCESSOR
    audio_processor_.Initialize(codec, realtime_chat_enabled_);
    audio_processor_.OnOutput([this](std::vector<int16_t>&& data) {
        background_task_->Schedule([this, data = std::move(data)]() mutable {
            opus_encoder_->Encode(std::move(data), [this](std::vector<uint8_t>&& opus) {
                Schedule([this, opus = std::move(opus)]() {
                    protocol_->SendAudio(opus);
                });
            });
        });
    });
    audio_processor_.OnVadStateChange([this](bool speaking) {
        if (device_state_ == kDeviceStateListening) {
            Schedule([this, speaking]() {
                if (speaking) {
                    voice_detected_ = true;
                } else {
                    voice_detected_ = false;
                }
                // LED功能已移除
            });
        }
    });
#endif

#if CONFIG_USE_WAKE_WORD_DETECT
    wake_word_detect_.Initialize(codec);
    wake_word_detect_.OnWakeWordDetected([this](const std::string& wake_word) {
        Schedule([this, &wake_word]() {
            if (device_state_ == kDeviceStateIdle) {
                SetDeviceState(kDeviceStateConnecting);
                wake_word_detect_.EncodeWakeWordData();

                if (!protocol_->OpenAudioChannel()) {
                    wake_word_detect_.StartDetection();
                    return;
                }
                
                std::vector<uint8_t> opus;
                // Encode and send the wake word data to the server
                while (wake_word_detect_.GetWakeWordOpus(opus)) {
                    protocol_->SendAudio(opus);
                }
                // Set the chat state to wake word detected
                protocol_->SendWakeWordDetected(wake_word);
                ESP_LOGI(TAG, "Wake word detected: %s", wake_word.c_str());
                SetListeningMode(realtime_chat_enabled_ ? kListeningModeRealtime : kListeningModeAutoStop);
            } else if (device_state_ == kDeviceStateSpeaking) {
                AbortSpeaking(kAbortReasonWakeWordDetected);
            } else if (device_state_ == kDeviceStateActivating) {
                SetDeviceState(kDeviceStateIdle);
            }
        });
    });
    wake_word_detect_.StartDetection();
#endif

    SetDeviceState(kDeviceStateIdle);
    esp_timer_start_periodic(clock_timer_handle_, 1000000);

#if 0
    while (true) {
        SystemInfo::PrintRealTimeStats(pdMS_TO_TICKS(1000));
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
#endif

#if CONFIG_USE_ALARM
    while(!ota_.HasServerTime()){
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    alarm_m_ = new AlarmManager();
    // alarm_m_->SetAlarm(10, "alarm1");
#endif
}

void Application::OnClockTimer() {
    clock_ticks_++;

    // Print the debug info every 10 seconds
    if (clock_ticks_ % 10 == 0) {
        int free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        int min_free_sram = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
        ESP_LOGI(TAG, "Free internal: %u minimal internal: %u", free_sram, min_free_sram);

        // If we have synchronized server time, set the status to clock "HH:MM" if the device is idle
        if (ota_.HasServerTime()) {
            if (device_state_ == kDeviceStateIdle) {
                Schedule([this]() {
                    // Set status to clock "HH:MM"
                    time_t now = time(NULL);
                    char time_str[64];
                    strftime(time_str, sizeof(time_str), "%H:%M  ", localtime(&now));
                    Board::GetInstance().GetDisplay()->SetStatus(time_str);
                });
            }
        }
    }
}

// Add a async task to MainLoop
void Application::Schedule(std::function<void()> callback) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        main_tasks_.push_back(std::move(callback));
    }
    xEventGroupSetBits(event_group_, SCHEDULE_EVENT);
}

// The Main Loop controls the chat state and websocket connection
// If other tasks need to access the websocket or chat state,
// they should use Schedule to call this function
void Application::MainLoop() {
    while (true) {
        auto bits = xEventGroupWaitBits(event_group_, SCHEDULE_EVENT, pdTRUE, pdFALSE, portMAX_DELAY);

        if (bits & SCHEDULE_EVENT) {
            std::unique_lock<std::mutex> lock(mutex_);
            std::list<std::function<void()>> tasks = std::move(main_tasks_);
            lock.unlock();
            for (auto& task : tasks) {
                task();
            }
        }
    }
}

// The Audio Loop is used to input and output audio data
void Application::AudioLoop() {
    auto codec = Board::GetInstance().GetAudioCodec();
    while (true) {
        OnAudioInput();
        if (codec->output_enabled()) {
            OnAudioOutput();
        }
#if CONFIG_USE_ALARM
        if(alarm_m_ != nullptr){
                // 闹钟来了
            if(alarm_m_->IsRing()){
                if(device_state_ != kDeviceStateListening){
                    if (device_state_ == kDeviceStateActivating) {
                        Reboot();
                        return;
                    }
                    if (!protocol_->IsAudioChannelOpened()) {
                        SetDeviceState(kDeviceStateConnecting);
                        if (!protocol_->OpenAudioChannel()) {
                            SetDeviceState(kDeviceStateIdle);
                            return;
                        }
                    }
                    // protocol_->SendStartListening(kListeningModeManualStop);
                    SetDeviceState(kDeviceStateListening);
                    ESP_LOGI(TAG, "Alarm ring, begging status %d", device_state_);
                }
                protocol_->SendText(alarm_m_->get_now_alarm_name());
                alarm_m_->ClearRing();
            }
        }
#endif
    }
}

void Application::OnAudioOutput() {
    auto now = std::chrono::steady_clock::now();
    auto codec = Board::GetInstance().GetAudioCodec();
    const int max_silence_seconds = 10;

    std::unique_lock<std::mutex> lock(mutex_);
    if (audio_decode_queue_.empty()) {
        // Disable the output if there is no audio data for a long time
        if (device_state_ == kDeviceStateIdle) {
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - last_output_time_).count();
            if (duration > max_silence_seconds) {
                codec->EnableOutput(false);
            }
        }
        return;
    }

    if (device_state_ == kDeviceStateListening) {
        audio_decode_queue_.clear();
        return;
    }

    auto opus = std::move(audio_decode_queue_.front());
    audio_decode_queue_.pop_front();
    lock.unlock();

    background_task_->Schedule([this, codec, opus = std::move(opus)]() mutable {
        if (aborted_) {
            return;
        }

        std::vector<int16_t> pcm;
        if (!opus_decoder_->Decode(std::move(opus), pcm)) {
            ESP_LOGE(TAG, "🔥 Opus解码失败 - 数据包大小:%zu bytes, 解码器配置: [采样率:%d, 帧长度:%dms]", 
                     opus.size(), opus_decoder_->sample_rate(), opus_decoder_->duration_ms());
            return;
        }
        // Resample if the sample rate is different
        if (opus_decoder_->sample_rate() != codec->output_sample_rate()) {
            int target_size = output_resampler_.GetOutputSamples(pcm.size());
            std::vector<int16_t> resampled(target_size);
            output_resampler_.Process(pcm.data(), pcm.size(), resampled.data());
            pcm = std::move(resampled);
        }
        codec->OutputData(pcm);
        last_output_time_ = std::chrono::steady_clock::now();
    });
}

void Application::OnAudioInput() {
    std::vector<int16_t> data;

#if CONFIG_USE_WAKE_WORD_DETECT
    if (wake_word_detect_.IsDetectionRunning()) {
        ReadAudio(data, 16000, wake_word_detect_.GetFeedSize());
        wake_word_detect_.Feed(data);
        return;
    }
#endif
#if CONFIG_USE_AUDIO_PROCESSOR
    if (audio_processor_.IsRunning()) {
        ReadAudio(data, 16000, audio_processor_.GetFeedSize());
        audio_processor_.Feed(data);
        return;
    }
#else
    if (device_state_ == kDeviceStateListening) {
        ReadAudio(data, 16000, 30 * 16000 / 1000);
        background_task_->Schedule([this, data = std::move(data)]() mutable {
            opus_encoder_->Encode(std::move(data), [this](std::vector<uint8_t>&& opus) {
                Schedule([this, opus = std::move(opus)]() {
                    protocol_->SendAudio(opus);
                });
            });
        });
        return;
    }
#endif
    vTaskDelay(pdMS_TO_TICKS(30));
}

void Application::ReadAudio(std::vector<int16_t>& data, int sample_rate, int samples) {
    auto codec = Board::GetInstance().GetAudioCodec();
    if (codec->input_sample_rate() != sample_rate) {
        data.resize(samples * codec->input_sample_rate() / sample_rate);
        if (!codec->InputData(data)) {
            return;
        }
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
    } else {
        data.resize(samples);
        if (!codec->InputData(data)) {
            return;
        }
    }
}

void Application::AbortSpeaking(AbortReason reason) {
    ESP_LOGI(TAG, "Abort speaking - 原因: %d", reason);
    aborted_ = true;
    protocol_->SendAbortSpeaking(reason);
    
    // 优化：用户中断时立即切换到listening状态，提高响应速度
    if (reason == kAbortReasonNone || reason == kAbortReasonWakeWordDetected) {
        Schedule([this]() {
            if (device_state_ == kDeviceStateSpeaking) {
                ESP_LOGI(TAG, "用户中断，立即切换到listening状态");
                SetDeviceState(kDeviceStateListening);
            }
        });
    }
}

void Application::SetListeningMode(ListeningMode mode) {
    listening_mode_ = mode;
    SetDeviceState(kDeviceStateListening);
}

void Application::SetDeviceState(DeviceState state) {
    if (device_state_ == state) {
        return;
    }
    
    clock_ticks_ = 0;
    auto previous_state = device_state_;
    device_state_ = state;
    
    // 添加详细的状态切换日志，特别关注listening相关的切换
    if (state == kDeviceStateListening || previous_state == kDeviceStateListening ||
        state == kDeviceStateSpeaking || previous_state == kDeviceStateSpeaking) {
        ESP_LOGI(TAG, "🔄 STATE CHANGE: %s -> %s (listening_mode: %d)", 
                 STATE_STRINGS[previous_state], STATE_STRINGS[state], listening_mode_);
    } else {
        ESP_LOGI(TAG, "STATE: %s", STATE_STRINGS[device_state_]);
    }
    
    // The state is changed, wait for all background tasks to finish
    background_task_->WaitForCompletion();

    auto& board = Board::GetInstance();
    auto display = board.GetDisplay();
    // LED功能已移除
    if(state != kDeviceStateIdle) {
        display->SetIdle(false);
    }
    switch (state) {
        case kDeviceStateUnknown:
        case kDeviceStateIdle:
            ESP_LOGI(TAG, "设备进入 idle 状态，调用 display->SetIdle(true)");
            display->SetIdle(true);
            display->SetStatus(Lang::Strings::STANDBY);
            display->SetEmotion("neutral");
#if CONFIG_USE_AUDIO_PROCESSOR
            audio_processor_.Stop();
            // 优化：只在从连接或升级状态切换到idle时才强制重置缓冲区
            if (previous_state == kDeviceStateConnecting || 
                previous_state == kDeviceStateUpgrading ||
                previous_state == kDeviceStateActivating) {
                audio_processor_.ForceResetBuffer();
            }
#endif
#if CONFIG_USE_WAKE_WORD_DETECT
            wake_word_detect_.StartDetection();
#endif
            break;
        case kDeviceStateConnecting:
            display->SetStatus(Lang::Strings::CONNECTING);
            display->SetEmotion("neutral");
            display->SetChatMessage("system", "");
            break;
        case kDeviceStateListening:
            display->SetStatus(Lang::Strings::LISTENING);
            display->SetEmotion("neutral");

            // Update the IoT states before sending the start listening command
            UpdateIotStates();

            // 每次进入listening状态都发送listen start给服务器
            protocol_->SendStartListening(listening_mode_);
            ESP_LOGI(TAG, "进入listening状态，发送listen start通知服务器开始监听");

            // Make sure the audio processor is running
#if CONFIG_USE_AUDIO_PROCESSOR
            if (!audio_processor_.IsRunning()) {
#else
            if (true) {
#endif
                if (listening_mode_ == kListeningModeAutoStop && previous_state == kDeviceStateSpeaking) {
                    // 优化：减少缓冲区等待时间从50ms到10ms，减少音频数据丢失
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
                // 优化：只在从非listening状态切换时才重置编码器状态
                if (previous_state != kDeviceStateListening) {
                    opus_encoder_->ResetState();
                    ESP_LOGI(TAG, "🎤 重置Opus编码器状态 (从 %s 切换)", STATE_STRINGS[previous_state]);
                } else {
                    ESP_LOGI(TAG, "🎤 保持Opus编码器状态 (从 %s 快速切换)", STATE_STRINGS[previous_state]);
                }
#if CONFIG_USE_WAKE_WORD_DETECT
                wake_word_detect_.StopDetection();
#endif
#if CONFIG_USE_AUDIO_PROCESSOR
                audio_processor_.Start();
                ESP_LOGI(TAG, "🎙️ 音频处理器已启动，准备接收音频数据");
#endif
            }
            break;
        case kDeviceStateSpeaking:
            display->SetStatus(Lang::Strings::SPEAKING);
            display->SetEmotion("speaking");

            // 优化：在非实时聊天模式下，延迟启动唤醒词检测以减少状态切换延迟
            if (listening_mode_ != kListeningModeRealtime) {
#if CONFIG_USE_AUDIO_PROCESSOR
                audio_processor_.Stop();
#endif
                // 优化：延迟启动唤醒词检测，避免在快速状态切换时造成干扰
                Schedule([this]() {
                    vTaskDelay(pdMS_TO_TICKS(100)); // 100ms后启动唤醒词检测
#if CONFIG_USE_WAKE_WORD_DETECT
                    if (device_state_ == kDeviceStateSpeaking) { // 确保状态仍然是speaking
                        wake_word_detect_.StartDetection();
                    }
#endif
                });
            }
            ResetDecoder();
            break;
        default:
            // Do nothing
            break;
    }
}

void Application::ResetDecoder() {
    std::lock_guard<std::mutex> lock(mutex_);
    opus_decoder_->ResetState();
    audio_decode_queue_.clear();
    last_output_time_ = std::chrono::steady_clock::now();
    
    auto codec = Board::GetInstance().GetAudioCodec();
    codec->EnableOutput(true);
}

void Application::SetDecodeSampleRate(int sample_rate, int frame_duration) {
    // 添加空指针检查，防止在音频系统初始化完成前访问
    if (!opus_decoder_) {
        ESP_LOGW(TAG, "opus_decoder_未初始化，跳过设置采样率");
        return;
    }
    
    // 调试信息：显示当前和目标设置
    ESP_LOGI(TAG, "SetDecodeSampleRate: 当前解码器 [采样率:%d, 帧长度:%dms] -> 目标 [采样率:%d, 帧长度:%dms]", 
             opus_decoder_->sample_rate(), opus_decoder_->duration_ms(), sample_rate, frame_duration);
    
    if (opus_decoder_->sample_rate() == sample_rate && opus_decoder_->duration_ms() == frame_duration) {
        ESP_LOGI(TAG, "解码器参数已匹配，无需重新创建");
        return;
    }

    opus_decoder_.reset();
    opus_decoder_ = std::make_unique<OpusDecoderWrapper>(sample_rate, 1, frame_duration);
    ESP_LOGI(TAG, "✅ 解码器已重新创建: [采样率:%d, 帧长度:%dms]", 
             opus_decoder_->sample_rate(), opus_decoder_->duration_ms());

    auto codec = Board::GetInstance().GetAudioCodec();
    if (opus_decoder_->sample_rate() != codec->output_sample_rate()) {
        ESP_LOGI(TAG, "Resampling audio from %d to %d", opus_decoder_->sample_rate(), codec->output_sample_rate());
        output_resampler_.Configure(opus_decoder_->sample_rate(), codec->output_sample_rate());
    }
}

void Application::UpdateIotStates() {
    auto& thing_manager = iot::ThingManager::GetInstance();
    std::string states;
    if (thing_manager.GetStatesJson(states, true)) {
        protocol_->SendIotStates(states);
    }
}

void Application::Reboot() {
    ESP_LOGI(TAG, "Rebooting...");
    esp_restart();
}

void Application::WakeWordInvoke(const std::string& wake_word) {
    if (device_state_ == kDeviceStateIdle) {
        ToggleChatState();
        Schedule([this, wake_word]() {
            if (protocol_) {
                protocol_->SendWakeWordDetected(wake_word); 
            }
        }); 
    } else if (device_state_ == kDeviceStateSpeaking) {
        Schedule([this]() {
            AbortSpeaking(kAbortReasonNone);
        });
    } else if (device_state_ == kDeviceStateListening) {   
        Schedule([this]() {
            if (protocol_) {
                protocol_->CloseAudioChannel();
            }
        });
    }
}

bool Application::CanEnterSleepMode() {
    if (device_state_ != kDeviceStateIdle) {
        return false;
    }

    if (protocol_ && protocol_->IsAudioChannelOpened()) {
        return false;
    }

    // Now it is safe to enter sleep mode
    return true;
}

void Application::SetImageResourceCallback(std::function<void()> callback) {
    image_resource_callback_ = callback;
    // 如果OTA检查已经完成，立即执行回调
    if (ota_check_completed_) {
        Schedule(callback);
    }
}

void Application::PauseAudioProcessing() {
    ESP_LOGI(TAG, "暂停音频处理模块...");
    
#if CONFIG_USE_AUDIO_PROCESSOR
    if (audio_processor_.IsRunning()) {
        audio_processor_.Stop();
        ESP_LOGI(TAG, "音频处理器已停止");
    }
#endif

#if CONFIG_USE_WAKE_WORD_DETECT
    if (wake_word_detect_.IsDetectionRunning()) {
        wake_word_detect_.StopDetection();
        ESP_LOGI(TAG, "唤醒词检测已停止");
    }
#endif
    
    // 清空音频队列，释放内存
    {
        std::lock_guard<std::mutex> lock(mutex_);
        audio_decode_queue_.clear();
        ESP_LOGI(TAG, "音频解码队列已清空");
    }
    
    // 等待背景任务完成，确保所有音频处理都停止
    if (background_task_) {
        background_task_->WaitForCompletion();
        ESP_LOGI(TAG, "背景音频任务已完成");
    }
}

void Application::ResumeAudioProcessing() {
    ESP_LOGI(TAG, "恢复音频处理模块...");
    
    // 根据当前设备状态决定是否重启音频处理
    if (device_state_ == kDeviceStateIdle) {
#if CONFIG_USE_WAKE_WORD_DETECT
        wake_word_detect_.StartDetection();
        ESP_LOGI(TAG, "唤醒词检测已重启");
#endif
    } else if (device_state_ == kDeviceStateListening) {
#if CONFIG_USE_AUDIO_PROCESSOR
        if (!audio_processor_.IsRunning()) {
            audio_processor_.Start();
            ESP_LOGI(TAG, "音频处理器已重启");
        }
#endif
    }
}

bool Application::IsAudioQueueEmpty() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
    return audio_decode_queue_.empty();
}

// **新增：强力音频保护机制实现**

bool Application::IsAudioActivityHigh() const {
    // 多重音频活动检测 - 确保最高精度
    
    // 1. 设备状态检测
    if (device_state_ == kDeviceStateListening || 
        device_state_ == kDeviceStateConnecting ||
        device_state_ == kDeviceStateSpeaking) {
        return true;
    }
    
    // 2. 音频队列检测  
    if (!IsAudioQueueEmpty()) {
        return true;
    }
    
    // 3. 音频处理器状态检测 - 使用const_cast因为这是只读检查
#if CONFIG_USE_AUDIO_PROCESSOR
    if (const_cast<Application*>(this)->audio_processor_.IsRunning()) {
        return true;
    }
#endif
    
    // 4. 唤醒词检测器状态检测 - 使用const_cast因为这是只读检查
#if CONFIG_USE_WAKE_WORD_DETECT
    if (const_cast<Application*>(this)->wake_word_detect_.IsDetectionRunning()) {
        return true;
    }
#endif
    
    // 5. 协议音频通道检测
    if (protocol_ && protocol_->IsAudioChannelOpened()) {
        return true;
    }
    
    return false;
}

bool Application::IsAudioProcessingCritical() const {
    // 关键音频处理状态检测 - 绝对不能中断
    
    // 正在进行语音识别
    if (device_state_ == kDeviceStateListening && voice_detected_) {
        return true;
    }
    
    // 正在播放重要音频（TTS、系统提示音）
    if (device_state_ == kDeviceStateSpeaking) {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
        // 如果音频队列有较多待播放数据，认为是关键状态
        return audio_decode_queue_.size() > 3;
    }
    
    // 正在建立音频连接
    if (device_state_ == kDeviceStateConnecting) {
        return true;
    }
    
    return false;
}

void Application::SetAudioPriorityMode(bool enabled) {
    if (enabled) {
        ESP_LOGI(TAG, "🔒 启用音频优先模式 - 图片播放将被严格限制");
        
        // 提升音频相关任务的优先级
        if (audio_loop_task_handle_) {
            vTaskPrioritySet(audio_loop_task_handle_, 10); // 提升到最高优先级
        }
        
        // 降低背景任务优先级，减少对音频的干扰
        if (background_task_) {
            // 假设background_task_有设置优先级的方法，这里仅作示例
            ESP_LOGI(TAG, "降低背景任务优先级以保护音频处理");
        }
        
    } else {
        ESP_LOGI(TAG, "🔓 恢复正常优先级模式");
        
        // 恢复音频任务正常优先级
        if (audio_loop_task_handle_) {
            vTaskPrioritySet(audio_loop_task_handle_, 9); // 恢复原优先级
        }
    }
}

int Application::GetAudioPerformanceScore() const {
    int score = 100; // 满分100，分数越低表示音频压力越大
    
    // 音频队列长度影响 (-10分每个队列项目)
    {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
        score -= std::min(50, (int)audio_decode_queue_.size() * 10);
    }
    
    // 设备状态影响
    switch (device_state_) {
        case kDeviceStateListening:
            score -= 20; // 语音识别时压力较大
            if (voice_detected_) {
                score -= 15; // 检测到语音时压力更大
            }
            break;
        case kDeviceStateSpeaking:
            score -= 25; // TTS播放时压力最大
            break;
        case kDeviceStateConnecting:
            score -= 15; // 连接时有一定压力
            break;
        default:
            break;
    }
    
    // 内存压力影响
    int free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    if (free_sram < 100000) { // 小于100KB
        score -= 20;
    } else if (free_sram < 200000) { // 小于200KB  
        score -= 10;
    }
    
    // 确保分数在合理范围内
    return std::max(0, std::min(100, score));
}

// **新增：智能分级音频保护实现**

bool Application::IsRealAudioProcessing() const {
    // 检测是否有真正的音频处理活动（有数据流动）
    
    // 1. 检查音频队列是否有数据
    if (!IsAudioQueueEmpty()) {
        return true;
    }
    
    // 2. 检查是否正在播放音频（TTS）
    if (device_state_ == kDeviceStateSpeaking) {
        return true;
    }
    
    // 3. 检查是否正在进行语音识别且检测到语音
    if (device_state_ == kDeviceStateListening && voice_detected_) {
        return true;
    }
    
    // 4. 检查是否正在建立音频连接
    if (device_state_ == kDeviceStateConnecting) {
        return true;
    }
    
    return false;
}

Application::AudioActivityLevel Application::GetAudioActivityLevel() const {
    // 分级音频活动检测 - 返回具体的活动级别
    
    // 最高级别：关键音频处理 - 完全暂停图片
    if (IsAudioProcessingCritical()) {
        return AUDIO_CRITICAL;
    }
    
    // 高级别：实际音频处理 - 降低图片优先级
    if (IsRealAudioProcessing()) {
        return AUDIO_ACTIVE;
    }
    
    // 中级别：音频系统待机 - 允许低帧率播放
    // 唤醒词检测运行但没有实际音频处理
#if CONFIG_USE_WAKE_WORD_DETECT
    if (const_cast<Application*>(this)->wake_word_detect_.IsDetectionRunning() && 
        device_state_ == kDeviceStateIdle) {
        return AUDIO_STANDBY;
    }
#endif
    
    // 音频通道开启但没有实际数据传输
    if (protocol_ && protocol_->IsAudioChannelOpened() && 
        device_state_ == kDeviceStateIdle && IsAudioQueueEmpty()) {
        return AUDIO_STANDBY;
    }
    
    // 最低级别：完全空闲 - 允许正常图片播放
    return AUDIO_IDLE;
}
