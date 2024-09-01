#include "Application.h"
#include <cstring>
#include "esp_log.h"
#include "model_path.h"
#include "SystemInfo.h"
#include "cJSON.h"
#include "silk_resampler.h"

#define TAG "application"


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
        assert(0 == silk_resampler_init(&resampler_state_, opus_decode_sample_rate_, CONFIG_AUDIO_OUTPUT_SAMPLE_RATE, 1));
    }
}

Application::~Application() {
    if (afe_detection_data_ != nullptr) {
        esp_afe_sr_v1.destroy(afe_detection_data_);
    }

    if (afe_communication_data_ != nullptr) {
        esp_afe_vc_v1.destroy(afe_communication_data_);
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

void Application::Start() {
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

    wifi_station_.Start();

    StartCommunication();
    StartDetection();

    xEventGroupSetBits(event_group_, DETECTION_RUNNING);
}

void Application::SetChatState(ChatState state) {
    chat_state_ = state;
    switch (chat_state_) {
        case kChatStateIdle:
            ESP_LOGI(TAG, "Chat state: idle");
            builtin_led_.TurnOff();
            break;
        case kChatStateConnecting:
            ESP_LOGI(TAG, "Chat state: connecting");
            builtin_led_.SetBlue();
            builtin_led_.TurnOn();
            break;
        case kChatStateListening:
            ESP_LOGI(TAG, "Chat state: listening");
            builtin_led_.SetRed();
            builtin_led_.TurnOn();
            break;
        case kChatStateSpeaking:
            ESP_LOGI(TAG, "Chat state: speaking");
            builtin_led_.SetGreen();
            builtin_led_.TurnOn();
            break;
    }

    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (ws_client_ && ws_client_->IsConnected()) {
        cJSON* root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "type", "state");
        cJSON_AddStringToObject(root, "state", chat_state_ == kChatStateListening ? "listening" : "speaking");
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
        .vad_init = false,
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
        .vad_init = false,
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

        if (res->wakeup_state == WAKENET_DETECTED) {
            ESP_LOGI(TAG, "Wakenet detected");

            xEventGroupClearBits(event_group_, DETECTION_RUNNING);
            SetChatState(kChatStateConnecting);
            StartWebSocketClient();

            std::lock_guard<std::recursive_mutex> lock(mutex_);
            if (ws_client_ && ws_client_->IsConnected()) {
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
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            if (ws_client_ && ws_client_->IsConnected()) {
                ws_client_->Send(opus.iov_base, opus.iov_len, true);
            }
        });

        free(pcm.iov_base);
    }
}

void Application::AudioDecodeTask() {
    while (true) {
        AudioPacket* packet;
        xQueueReceive(audio_decode_queue_, &packet, portMAX_DELAY);

        int frame_size = opus_decode_sample_rate_ / 1000 * opus_duration_ms_;
        packet->pcm.resize(frame_size);

        int ret = opus_decode(opus_decoder_, packet->opus.data(), packet->opus.size(), packet->pcm.data(), frame_size, 0);
        if (ret < 0) {
            ESP_LOGE(TAG, "Failed to decode audio, error code: %d", ret);
            delete packet;
            continue;
        }

        if (opus_decode_sample_rate_ != CONFIG_AUDIO_OUTPUT_SAMPLE_RATE) {
            int target_size = frame_size * CONFIG_AUDIO_OUTPUT_SAMPLE_RATE / opus_decode_sample_rate_;
            std::vector<int16_t> resampled(target_size);
            assert(0 == silk_resampler(&resampler_state_, resampled.data(), packet->pcm.data(), frame_size));
            packet->pcm = std::move(resampled);
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
        assert(0 == silk_resampler_init(&resampler_state_, opus_decode_sample_rate_, CONFIG_AUDIO_OUTPUT_SAMPLE_RATE, 1));
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

    ws_client_->OnConnected([this]() {
        ESP_LOGI(TAG, "Websocket connected");
        
        // Send hello message to describe the client
        // keys: message type, version, wakeup_model, audio_params (format, sample_rate, channels)
        std::string message = "{";
        message += "\"type\":\"hello\", \"version\":\"1.0\",";
        message += "\"wakeup_model\":\"" + std::string(wakenet_model_) + "\",";
        message += "\"audio_params\":{";
        message += "\"format\":\"opus\", \"sample_rate\":" + std::to_string(CONFIG_AUDIO_INPUT_SAMPLE_RATE) + ", \"channels\":1";
        message += "}}";
        ws_client_->Send(message);
    });

    ws_client_->OnData([this](const char* data, size_t len, bool binary) {
        auto packet = new AudioPacket();
        if (binary) {
            auto header = (AudioDataHeader*)data;
            packet->type = kAudioPacketTypeData;
            packet->timestamp = ntohl(header->timestamp);

            auto payload_size = ntohl(header->payload_size);
            packet->opus.resize(payload_size);
            memcpy(packet->opus.data(), data + sizeof(AudioDataHeader), payload_size);
        } else {
            // Parse JSON data
            auto root = cJSON_Parse(data);
            auto type = cJSON_GetObjectItem(root, "type");
            if (type != NULL) {
                if (strcmp(type->valuestring, "tts") == 0) {
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
                }
            }
            cJSON_Delete(root);
        }
        xQueueSend(audio_decode_queue_, &packet, portMAX_DELAY);
    });

    ws_client_->OnError([this](int error) {
        ESP_LOGE(TAG, "Websocket error: %d", error);
    });

    ws_client_->OnClosed([this]() {
        ESP_LOGI(TAG, "Websocket closed");
    });

    if (!ws_client_->Connect(CONFIG_WEBSOCKET_URL)) {
        ESP_LOGE(TAG, "Failed to connect to websocket server");
        return;
    }
}