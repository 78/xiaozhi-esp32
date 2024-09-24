#include "Application.h"
#include "BuiltinLed.h"
#include <cstring>
#include "esp_log.h"
#include "model_path.h"
#include "SystemInfo.h"
#include "cJSON.h"
#include "driver/gpio.h"

#define TAG "Application"


Application::Application() {
    event_group_ = xEventGroupCreate();
    audio_encode_queue_ = xQueueCreate(100, sizeof(iovec));
    audio_decode_queue_ = xQueueCreate(100, sizeof(AudioPacket*));

    srmodel_list_t *models = esp_srmodel_init("model");
    for (int i = 0; i < models->num; i++) {
        ESP_LOGI(TAG, "Model %d: %s", i, models->model_name[i]);
        if (strstr(models->model_name[i], ESP_WN_PREFIX) != NULL) {
            wakenet_model_ = models->model_name[i];
        } else if (strstr(models->model_name[i], ESP_NSNET_PREFIX) != NULL) {
            nsnet_model_ = models->model_name[i];
        }
    }
    
    opus_encoder_.Configure(CONFIG_AUDIO_INPUT_SAMPLE_RATE, 1);
    opus_decoder_ = opus_decoder_create(opus_decode_sample_rate_, 1, NULL);
    if (opus_decode_sample_rate_ != CONFIG_AUDIO_OUTPUT_SAMPLE_RATE) {
        opus_resampler_.Configure(opus_decode_sample_rate_, CONFIG_AUDIO_OUTPUT_SAMPLE_RATE);
    }

    firmware_upgrade_.SetCheckVersionUrl(CONFIG_OTA_VERSION_URL);
}

Application::~Application() {
    if (afe_detection_data_ != nullptr) {
        esp_afe_sr_v1.destroy(afe_detection_data_);
    }

    if (afe_communication_data_ != nullptr) {
        esp_afe_vc_v1.destroy(afe_communication_data_);
    }

    if (wake_word_encode_task_stack_ != nullptr) {
        free(wake_word_encode_task_stack_);
    }
    for (auto& pcm : wake_word_pcm_) {
        free(pcm.iov_base);
    }
    for (auto& packet : wake_word_opus_) {
        heap_caps_free(packet);
    }
    
    if (opus_decoder_ != nullptr) {
        opus_decoder_destroy(opus_decoder_);
    }
    if (audio_encode_task_stack_ != nullptr) {
        free(audio_encode_task_stack_);
    }
    if (audio_decode_task_stack_ != nullptr) {
        free(audio_decode_task_stack_);
    }
    vQueueDelete(audio_decode_queue_);
    vQueueDelete(audio_encode_queue_);

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
        firmware_upgrade_.StartUpgrade();
        // If upgrade success, the device will reboot and never reach here
        ESP_LOGI(TAG, "Firmware upgrade failed...");
        SetChatState(kChatStateIdle);
    } else {
        firmware_upgrade_.MarkValid();
    }
}

void Application::Start() {
    // Initialize the audio device
    audio_device_.Start(CONFIG_AUDIO_INPUT_SAMPLE_RATE, CONFIG_AUDIO_OUTPUT_SAMPLE_RATE);
    audio_device_.OnStateChanged([this]() {
        if (audio_device_.playing()) {
            SetChatState(kChatStateSpeaking);
        } else {
            // Check if communication is still running
            if (xEventGroupGetBits(event_group_) & COMMUNICATION_RUNNING) {
                SetChatState(kChatStateListening);
            } else {
                SetChatState(kChatStateIdle);
            }
        }
    });

    // OPUS encoder / decoder use a lot of stack memory
    const size_t opus_stack_size = 4096 * 8;
    audio_encode_task_stack_ = (StackType_t*)malloc(opus_stack_size);
    xTaskCreateStatic([](void* arg) {
        Application* app = (Application*)arg;
        app->AudioEncodeTask();
    }, "opus_encode", opus_stack_size, this, 1, audio_encode_task_stack_, &audio_encode_task_buffer_);
    audio_decode_task_stack_ = (StackType_t*)malloc(opus_stack_size);
    xTaskCreateStatic([](void* arg) {
        Application* app = (Application*)arg;
        app->AudioDecodeTask();
    }, "opus_decode", opus_stack_size, this, 1, audio_decode_task_stack_, &audio_decode_task_buffer_);

    StartCommunication();
    StartDetection();

    // Blink the LED to indicate the device is running
    auto& builtin_led = BuiltinLed::GetInstance();
    builtin_led.SetGreen();
    builtin_led.BlinkOnce();
    xEventGroupSetBits(event_group_, DETECTION_RUNNING);

    // Launch a task to check for new firmware version
    xTaskCreate([](void* arg) {
        Application* app = (Application*)arg;
        app->CheckNewVersion();
        vTaskDelete(NULL);
    }, "check_new_version", 4096 * 2, this, 1, NULL);
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
        case kChatStateTesting:
            builtin_led.SetRed();
            builtin_led.TurnOn();
            break;
        case kChatStateUpgrading:
            builtin_led.SetGreen();
            builtin_led.StartContinuousBlink(100);
            break;
    }

    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (ws_client_ && ws_client_->IsConnected()) {
        cJSON* root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "type", "state");
        cJSON_AddStringToObject(root, "state", state_str[chat_state_]);
        char* json = cJSON_PrintUnformatted(root);
        ws_client_->Send(json);
        cJSON_Delete(root);
        free(json);
    }
}

void Application::StartCommunication() {
    afe_config_t afe_config = {
        .aec_init = false,
        .se_init = true,
        .vad_init = true,
        .wakenet_init = false,
        .voice_communication_init = true,
        .voice_communication_agc_init = true,
        .voice_communication_agc_gain = 10,
        .vad_mode = VAD_MODE_3,
        .wakenet_model_name = NULL,
        .wakenet_model_name_2 = NULL,
        .wakenet_mode = DET_MODE_90,
        .afe_mode = SR_MODE_HIGH_PERF,
        .afe_perferred_core = 0,
        .afe_perferred_priority = 5,
        .afe_ringbuf_size = 50,
        .memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM,
        .afe_linear_gain = 1.0,
        .agc_mode = AFE_MN_PEAK_AGC_MODE_2,
        .pcm_config = {
            .total_ch_num = 1,
            .mic_num = 1,
            .ref_num = 0,
            .sample_rate = CONFIG_AUDIO_INPUT_SAMPLE_RATE,
        },
        .debug_init = false,
        .debug_hook = {{ AFE_DEBUG_HOOK_MASE_TASK_IN, NULL }, { AFE_DEBUG_HOOK_FETCH_TASK_IN, NULL }},
        .afe_ns_mode = NS_MODE_SSP,
        .afe_ns_model_name = NULL,
        .fixed_first_channel = true,
    };

    afe_communication_data_ = esp_afe_vc_v1.create_from_config(&afe_config);
    
    xTaskCreate([](void* arg) {
        Application* app = (Application*)arg;
        app->AudioCommunicationTask();
    }, "audio_communication", 4096 * 2, this, 5, NULL);
}

void Application::StartDetection() {
    afe_config_t afe_config = {
        .aec_init = false,
        .se_init = true,
        .vad_init = true,
        .wakenet_init = true,
        .voice_communication_init = false,
        .voice_communication_agc_init = false,
        .voice_communication_agc_gain = 10,
        .vad_mode = VAD_MODE_3,
        .wakenet_model_name = wakenet_model_,
        .wakenet_model_name_2 = NULL,
        .wakenet_mode = DET_MODE_90,
        .afe_mode = SR_MODE_HIGH_PERF,
        .afe_perferred_core = 0,
        .afe_perferred_priority = 5,
        .afe_ringbuf_size = 50,
        .memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM,
        .afe_linear_gain = 1.0,
        .agc_mode = AFE_MN_PEAK_AGC_MODE_2,
        .pcm_config = {
            .total_ch_num = 1,
            .mic_num = 1,
            .ref_num = 0,
            .sample_rate = CONFIG_AUDIO_INPUT_SAMPLE_RATE
        },
        .debug_init = false,
        .debug_hook = {{ AFE_DEBUG_HOOK_MASE_TASK_IN, NULL }, { AFE_DEBUG_HOOK_FETCH_TASK_IN, NULL }},
        .afe_ns_mode = NS_MODE_SSP,
        .afe_ns_model_name = NULL,
        .fixed_first_channel = true,
    };

    afe_detection_data_ = esp_afe_sr_v1.create_from_config(&afe_config);
    xTaskCreate([](void* arg) {
        Application* app = (Application*)arg;
        app->AudioFeedTask();
    }, "audio_feed", 4096 * 2, this, 5, NULL);

    xTaskCreate([](void* arg) {
        Application* app = (Application*)arg;
        app->AudioDetectionTask();
    }, "audio_detection", 4096 * 2, this, 5, NULL);
}

void Application::AudioFeedTask() {
    int chunk_size = esp_afe_vc_v1.get_feed_chunksize(afe_detection_data_);
    int16_t buffer[chunk_size];
    ESP_LOGI(TAG, "Audio feed task started, chunk size: %d", chunk_size);

    while (true) {
        audio_device_.Read(buffer, chunk_size);

        auto event_bits = xEventGroupGetBits(event_group_);
        if (event_bits & DETECTION_RUNNING) {
            esp_afe_sr_v1.feed(afe_detection_data_, buffer);
        } else if (event_bits & COMMUNICATION_RUNNING) {
            esp_afe_vc_v1.feed(afe_communication_data_, buffer);
        }
    }

    vTaskDelete(NULL);
}

void Application::StoreWakeWordData(uint8_t* data, size_t size) {
    // store audio data to wake_word_pcm_
    auto iov = (iovec){
        .iov_base = heap_caps_malloc(size, MALLOC_CAP_SPIRAM),
        .iov_len = size
    };
    memcpy(iov.iov_base, data, size);
    wake_word_pcm_.push_back(iov);
    // remove the oldest packet if the size is larger than 50, about 2 seconds
    if (wake_word_pcm_.size() > 50) {
        heap_caps_free(wake_word_pcm_.front().iov_base);
        wake_word_pcm_.pop_front();
    }
}

void Application::EncodeWakeWordData() {
    wake_word_opus_.clear();
    if (wake_word_encode_task_stack_ == nullptr) {
        wake_word_encode_task_stack_ = (StackType_t*)malloc(4096 * 8);
    }
    wake_word_encode_task_ = xTaskCreateStatic([](void* arg) {
        Application* app = (Application*)arg;
        auto start_time = esp_timer_get_time();
        // encode detect packets
        OpusEncoder* encoder = new OpusEncoder();
        encoder->Configure(CONFIG_AUDIO_INPUT_SAMPLE_RATE, 1, 60);
        encoder->SetComplexity(2);

        for (auto& pcm: app->wake_word_pcm_) {
            encoder->Encode(pcm, [app](const iovec opus) {
                auto protocol = app->AllocateBinaryProtocol(opus.iov_base, opus.iov_len);
                app->wake_word_opus_.push_back(protocol);
            });
            heap_caps_free(pcm.iov_base);
        }
        app->wake_word_pcm_.clear();

        auto end_time = esp_timer_get_time();
        ESP_LOGI(TAG, "Encode wake word data opus packets: %d in %lld ms", app->wake_word_opus_.size(), (end_time - start_time) / 1000);
        xEventGroupSetBits(app->event_group_, WAKE_WORD_ENCODED);
        delete encoder;
        vTaskDelete(NULL);
    }, "encode_detect_packets", 4096 * 8, this, 1, wake_word_encode_task_stack_, &wake_word_encode_task_buffer_);
}

void Application::SendWakeWordData() {
    for (auto& protocol: wake_word_opus_) {
        ws_client_->Send(protocol, sizeof(BinaryProtocol) + ntohl(protocol->payload_size), true);
        heap_caps_free(protocol);
    }
    wake_word_opus_.clear();
}

BinaryProtocol* Application::AllocateBinaryProtocol(void* payload, size_t payload_size) {
    auto last_timestamp = audio_device_.playing() ? audio_device_.last_timestamp() : 0;
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

void Application::CheckTestButton() {
    if (gpio_get_level(GPIO_NUM_1) == 0) {
        if (chat_state_ == kChatStateIdle) {
            SetChatState(kChatStateTesting);
            test_resampler_.Configure(CONFIG_AUDIO_INPUT_SAMPLE_RATE, CONFIG_AUDIO_OUTPUT_SAMPLE_RATE);
        }
    } else {
        if (chat_state_ == kChatStateTesting) {
            SetChatState(kChatStateIdle);
            
            // 创建新线程来处理音频播放
            xTaskCreate([](void* arg) {
                Application* app = static_cast<Application*>(arg);
                app->PlayTestAudio();
                vTaskDelete(NULL);
            }, "play_test_audio", 4096, this, 1, NULL);
        }
    }
}

void Application::PlayTestAudio() {
    // 写入音频数据到扬声器
    auto packet = new AudioPacket();
    packet->type = kAudioPacketTypeStart;
    audio_device_.QueueAudioPacket(packet);

    for (auto& pcm : test_pcm_) {
        packet = new AudioPacket();
        packet->type = kAudioPacketTypeData;
        packet->pcm.resize(test_resampler_.GetOutputSamples(pcm.iov_len / 2));
        test_resampler_.Process((int16_t*)pcm.iov_base, pcm.iov_len / 2, packet->pcm.data());
        audio_device_.QueueAudioPacket(packet);
        heap_caps_free(pcm.iov_base);
    }
    // 清除测试PCM数据
    test_pcm_.clear();

    // 停止音频设备
    packet = new AudioPacket();
    packet->type = kAudioPacketTypeStop;
    audio_device_.QueueAudioPacket(packet);
}

void Application::AudioDetectionTask() {
    auto chunk_size = esp_afe_sr_v1.get_fetch_chunksize(afe_detection_data_);
    ESP_LOGI(TAG, "Audio detection task started, chunk size: %d", chunk_size);

    while (true) {
        xEventGroupWaitBits(event_group_, DETECTION_RUNNING, pdFALSE, pdTRUE, portMAX_DELAY);

        auto res = esp_afe_sr_v1.fetch(afe_detection_data_);
        if (res == nullptr || res->ret_value == ESP_FAIL) {
            ESP_LOGE(TAG, "Error in fetch");
            if (res != nullptr) {
                ESP_LOGI(TAG, "Error code: %d", res->ret_value);
            }
            continue;;
        }

        // Store the wake word data for voice recognition, like who is speaking
        StoreWakeWordData((uint8_t*)res->data, res->data_size);

        CheckTestButton();
        if (chat_state_ == kChatStateTesting) {
            auto& builtin_led = BuiltinLed::GetInstance();
            if (res->vad_state == AFE_VAD_SPEECH) {
                iovec iov = {
                    .iov_base = heap_caps_malloc(res->data_size, MALLOC_CAP_SPIRAM),
                    .iov_len = (size_t)res->data_size
                };
                memcpy(iov.iov_base, res->data, res->data_size);
                test_pcm_.push_back(iov);
                builtin_led.SetRed(128);
            } else {
                builtin_led.SetRed(32);
            }
            builtin_led.TurnOn();
            continue;
        }

        if (chat_state_ == kChatStateIdle && res->wakeup_state == WAKENET_DETECTED) {
            xEventGroupClearBits(event_group_, DETECTION_RUNNING);
            SetChatState(kChatStateConnecting);

            // Encode the wake word data and start websocket client at the same time
            // They both consume a lot of time (700ms), so we can do them in parallel
            EncodeWakeWordData();
            StartWebSocketClient();

            // Here the websocket is done, and we also wait for the wake word data to be encoded
            xEventGroupWaitBits(event_group_, WAKE_WORD_ENCODED, pdTRUE, pdTRUE, portMAX_DELAY);

            std::lock_guard<std::recursive_mutex> lock(mutex_);
            if (ws_client_ && ws_client_->IsConnected()) {
                // Send the wake word data to the server
                SendWakeWordData();
                // Send a ready message to indicate the server that the wake word data is sent
                SetChatState(kChatStateWakeWordDetected);
                opus_encoder_.ResetState();
                // If connected, the hello message is already sent, so we can start communication
                xEventGroupSetBits(event_group_, COMMUNICATION_RUNNING);
            } else {
                SetChatState(kChatStateIdle);
                xEventGroupSetBits(event_group_, DETECTION_RUNNING);
            }
        }
    }
}

void Application::AudioCommunicationTask() {
    int chunk_size = esp_afe_vc_v1.get_fetch_chunksize(afe_communication_data_);
    ESP_LOGI(TAG, "Audio communication task started, chunk size: %d", chunk_size);

    while (true) {
        xEventGroupWaitBits(event_group_, COMMUNICATION_RUNNING, pdFALSE, pdTRUE, portMAX_DELAY);

        auto res = esp_afe_vc_v1.fetch(afe_communication_data_);
        if (res == nullptr || res->ret_value == ESP_FAIL) {
            ESP_LOGE(TAG, "Error in fetch");
            if (res != nullptr) {
                ESP_LOGI(TAG, "Error code: %d", res->ret_value);
            }
            continue;
        }

        // Check if the websocket client is disconnected by the server
        {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            if (ws_client_ == nullptr || !ws_client_->IsConnected()) {
                if (ws_client_ != nullptr) {
                    delete ws_client_;
                    ws_client_ = nullptr;
                }
                if (audio_device_.playing()) {
                    audio_device_.Break();
                }
                SetChatState(kChatStateIdle);
                xEventGroupSetBits(event_group_, DETECTION_RUNNING);
                xEventGroupClearBits(event_group_, COMMUNICATION_RUNNING);
                continue;
            }
        }

        if (chat_state_ == kChatStateListening) {
            // Update the LED state based on the VAD state
            auto& builtin_led = BuiltinLed::GetInstance();
            if (res->vad_state == AFE_VAD_SPEECH) {
                builtin_led.SetRed(128);
            } else {
                builtin_led.SetRed(32);
            }
            builtin_led.TurnOn();

            // Send audio data to server
            iovec data = {
                .iov_base = malloc(res->data_size),
                .iov_len = (size_t)res->data_size
            };
            memcpy(data.iov_base, res->data, res->data_size);
            xQueueSend(audio_encode_queue_, &data, portMAX_DELAY);
        }
    }
}

void Application::AudioEncodeTask() {
    ESP_LOGI(TAG, "Audio encode task started");
    while (true) {
        iovec pcm;
        xQueueReceive(audio_encode_queue_, &pcm, portMAX_DELAY);

        // Encode audio data
        opus_encoder_.Encode(pcm, [this](const iovec opus) {
            auto protocol = AllocateBinaryProtocol(opus.iov_base, opus.iov_len);
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            if (ws_client_ && ws_client_->IsConnected()) {
                ws_client_->Send(protocol, sizeof(BinaryProtocol) + opus.iov_len, true);
            }
            heap_caps_free(protocol);
        });

        free(pcm.iov_base);
    }
}

void Application::AudioDecodeTask() {
    while (true) {
        AudioPacket* packet;
        xQueueReceive(audio_decode_queue_, &packet, portMAX_DELAY);

        if (packet->type == kAudioPacketTypeData) {
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
        }

        audio_device_.QueueAudioPacket(packet);
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
        delete ws_client_;
    }

    std::string token = "Bearer " + std::string(CONFIG_WEBSOCKET_ACCESS_TOKEN);
    ws_client_ = new WebSocketClient();
    ws_client_->SetHeader("Authorization", token.c_str());
    ws_client_->SetHeader("Device-Id", SystemInfo::GetMacAddress().c_str());
    ws_client_->SetHeader("Protocol-Version", std::to_string(PROTOCOL_VERSION).c_str());

    ws_client_->OnConnected([this]() {
        ESP_LOGI(TAG, "Websocket connected");
        
        // Send hello message to describe the client
        // keys: message type, version, wakeup_model, audio_params (format, sample_rate, channels)
        std::string message = "{";
        message += "\"type\":\"hello\",";
        message += "\"wakeup_model\":\"" + std::string(wakenet_model_) + "\",";
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
            xQueueSend(audio_decode_queue_, &packet, portMAX_DELAY);
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
                    xQueueSend(audio_decode_queue_, &packet, portMAX_DELAY);
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

    if (!ws_client_->Connect(CONFIG_WEBSOCKET_URL)) {
        ESP_LOGE(TAG, "Failed to connect to websocket server");
        return;
    }
}