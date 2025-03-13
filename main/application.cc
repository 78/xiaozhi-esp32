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
#include <esp_app_desc.h>

#define TAG "Application"

// 定义设备状态的字符串表示，用于日志输出
static const char* const STATE_STRINGS[] = {
    "unknown",       // 未知状态
    "starting",      // 启动中
    "configuring",   // 配置中
    "idle",          // 空闲状态
    "connecting",    // 连接中
    "listening",     // 监听中
    "speaking",      // 说话中
    "upgrading",     // 升级中
    "activating",    // 激活中
    "fatal_error",   // 致命错误
    "invalid_state"  // 无效状态
};

// 构造函数，初始化应用程序
Application::Application() {
    // 创建事件组，用于任务间通信
    event_group_ = xEventGroupCreate();
    // 创建后台任务，栈大小为4096 * 8字节
    background_task_ = new BackgroundTask(4096 * 8);

    // 创建时钟定时器，每秒触发一次
    esp_timer_create_args_t clock_timer_args = {
        .callback = [](void* arg) {
            Application* app = (Application*)arg;
            app->OnClockTimer();  // 定时器回调函数
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "clock_timer"
    };
    esp_timer_create(&clock_timer_args, &clock_timer_handle_);
}

// 析构函数，释放资源
Application::~Application() {
    if (clock_timer_handle_ != nullptr) {
        esp_timer_stop(clock_timer_handle_);  // 停止定时器
        esp_timer_delete(clock_timer_handle_);  // 删除定时器
    }
    if (background_task_ != nullptr) {
        delete background_task_;  // 删除后台任务
    }
    vEventGroupDelete(event_group_);  // 删除事件组
}

// 检查新版本
void Application::CheckNewVersion() {
    auto& board = Board::GetInstance();
    auto display = board.GetDisplay();
    // 设置OTA升级的POST数据
    ota_.SetPostData(board.GetJson());

    const int MAX_RETRY = 10;  // 最大重试次数
    int retry_count = 0;

    while (true) {
        if (!ota_.CheckVersion()) {  // 检查版本失败
            retry_count++;
            if (retry_count >= MAX_RETRY) {
                ESP_LOGE(TAG, "Too many retries, exit version check");  // 重试次数过多，退出
                return;
            }
            ESP_LOGW(TAG, "Check new version failed, retry in %d seconds (%d/%d)", 60, retry_count, MAX_RETRY);
            vTaskDelay(pdMS_TO_TICKS(60000));  // 延迟60秒后重试
            continue;
        }
        retry_count = 0;

        if (ota_.HasNewVersion()) {  // 有新版本
            Alert(Lang::Strings::OTA_UPGRADE, Lang::Strings::UPGRADING, "happy", Lang::Sounds::P3_UPGRADE);
            // 等待设备状态变为空闲
            do {
                vTaskDelay(pdMS_TO_TICKS(3000));
            } while (GetDeviceState() != kDeviceStateIdle);

            // 使用主任务进行升级，不可取消
            Schedule([this, display]() {
                SetDeviceState(kDeviceStateUpgrading);
                
                display->SetIcon(FONT_AWESOME_DOWNLOAD);  // 设置下载图标
                std::string message = std::string(Lang::Strings::NEW_VERSION) + ota_.GetFirmwareVersion();
                display->SetChatMessage("system", message.c_str());  // 显示新版本信息

                auto& board = Board::GetInstance();
                board.SetPowerSaveMode(false);  // 关闭省电模式
#if CONFIG_USE_WAKE_WORD_DETECT
                wake_word_detect_.StopDetection();  // 停止唤醒词检测
#endif
                // 预先关闭音频输出，避免升级过程有音频操作
                auto codec = board.GetAudioCodec();
                codec->EnableInput(false);  // 关闭音频输入
                codec->EnableOutput(false);  // 关闭音频输出
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    audio_decode_queue_.clear();  // 清空音频解码队列
                }
                background_task_->WaitForCompletion();  // 等待后台任务完成
                delete background_task_;
                background_task_ = nullptr;
                vTaskDelay(pdMS_TO_TICKS(1000));

                ota_.StartUpgrade([display](int progress, size_t speed) {  // 开始升级
                    char buffer[64];
                    snprintf(buffer, sizeof(buffer), "%d%% %zuKB/s", progress, speed / 1024);
                    display->SetChatMessage("system", buffer);  // 显示升级进度
                });

                // 如果升级成功，设备将重启，不会执行到这里
                display->SetStatus(Lang::Strings::UPGRADE_FAILED);  // 显示升级失败
                ESP_LOGI(TAG, "Firmware upgrade failed...");
                vTaskDelay(pdMS_TO_TICKS(3000));
                Reboot();  // 重启设备
            });

            return;
        }

        // 没有新版本，标记当前版本为有效
        ota_.MarkCurrentVersionValid();
        std::string message = std::string(Lang::Strings::VERSION) + ota_.GetCurrentVersion();
        display->ShowNotification(message.c_str());  // 显示当前版本信息
    
        if (ota_.HasActivationCode()) {  // 有激活码
            // 激活码有效
            SetDeviceState(kDeviceStateActivating);
            ShowActivationCode();  // 显示激活码

            // 60秒后再次检查或直到设备空闲
            for (int i = 0; i < 60; ++i) {
                if (device_state_ == kDeviceStateIdle) {
                    break;
                }
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            continue;
        }

        SetDeviceState(kDeviceStateIdle);  // 设置设备状态为空闲
        display->SetChatMessage("system", "");
        PlaySound(Lang::Sounds::P3_SUCCESS);  // 播放成功音效
        // 如果升级或空闲，退出循环
        break;
    }
}

// 显示激活码
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

    // 显示激活信息
    Alert(Lang::Strings::ACTIVATION, message.c_str(), "happy", Lang::Sounds::P3_ACTIVATION);
    vTaskDelay(pdMS_TO_TICKS(1000));
    background_task_->WaitForCompletion();  // 等待后台任务完成

    // 播放激活码的每个数字对应的音效
    for (const auto& digit : code) {
        auto it = std::find_if(digit_sounds.begin(), digit_sounds.end(),
            [digit](const digit_sound& ds) { return ds.digit == digit; });
        if (it != digit_sounds.end()) {
            PlaySound(it->sound);  // 播放数字音效
        }
    }
}

// 显示警告信息
void Application::Alert(const char* status, const char* message, const char* emotion, const std::string_view& sound) {
    ESP_LOGW(TAG, "Alert %s: %s [%s]", status, message, emotion);
    auto display = Board::GetInstance().GetDisplay();
    display->SetStatus(status);  // 设置状态
    display->SetEmotion(emotion);  // 设置表情
    display->SetChatMessage("system", message);  // 显示消息
    if (!sound.empty()) {
        PlaySound(sound);  // 播放音效
    }
}

// 取消警告
void Application::DismissAlert() {
    if (device_state_ == kDeviceStateIdle) {
        auto display = Board::GetInstance().GetDisplay();
        display->SetStatus(Lang::Strings::STANDBY);  // 设置状态为待机
        display->SetEmotion("neutral");  // 设置表情为中性
        display->SetChatMessage("system", "");  // 清空消息
    }
}

// 播放音效
void Application::PlaySound(const std::string_view& sound) {
    auto codec = Board::GetInstance().GetAudioCodec();
    codec->EnableOutput(true);  // 启用音频输出
    SetDecodeSampleRate(16000);  // 设置解码采样率
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
        audio_decode_queue_.emplace_back(std::move(opus));  // 将音频数据加入解码队列
    }
}

// 切换聊天状态
void Application::ToggleChatState() {
    if (device_state_ == kDeviceStateActivating) {
        SetDeviceState(kDeviceStateIdle);  // 如果正在激活，设置为空闲状态
        return;
    }

    if (!protocol_) {
        ESP_LOGE(TAG, "Protocol not initialized");  // 协议未初始化
        return;
    }

    if (device_state_ == kDeviceStateIdle) {
        Schedule([this]() {
            SetDeviceState(kDeviceStateConnecting);  // 设置为连接状态
            if (!protocol_->OpenAudioChannel()) {  // 打开音频通道
                return;
            }

            keep_listening_ = true;
            protocol_->SendStartListening(kListeningModeAutoStop);  // 开始监听
            SetDeviceState(kDeviceStateListening);  // 设置为监听状态
        });
    } else if (device_state_ == kDeviceStateSpeaking) {
        Schedule([this]() {
            AbortSpeaking(kAbortReasonNone);  // 中止说话
        });
    } else if (device_state_ == kDeviceStateListening) {
        Schedule([this]() {
            protocol_->CloseAudioChannel();  // 关闭音频通道
        });
    }
}

// 开始监听
void Application::StartListening() {
    if (device_state_ == kDeviceStateActivating) {
        SetDeviceState(kDeviceStateIdle);  // 如果正在激活，设置为空闲状态
        return;
    }

    if (!protocol_) {
        ESP_LOGE(TAG, "Protocol not initialized");  // 协议未初始化
        return;
    }
    
    keep_listening_ = false;
    if (device_state_ == kDeviceStateIdle) {
        Schedule([this]() {
            if (!protocol_->IsAudioChannelOpened()) {
                SetDeviceState(kDeviceStateConnecting);  // 设置为连接状态
                if (!protocol_->OpenAudioChannel()) {  // 打开音频通道
                    return;
                }
            }
            protocol_->SendStartListening(kListeningModeManualStop);  // 开始监听
            SetDeviceState(kDeviceStateListening);  // 设置为监听状态
        });
    } else if (device_state_ == kDeviceStateSpeaking) {
        Schedule([this]() {
            AbortSpeaking(kAbortReasonNone);  // 中止说话
            protocol_->SendStartListening(kListeningModeManualStop);  // 开始监听
            SetDeviceState(kDeviceStateListening);  // 设置为监听状态
        });
    }
}

// 停止监听
void Application::StopListening() {
    Schedule([this]() {
        if (device_state_ == kDeviceStateListening) {
            protocol_->SendStopListening();  // 停止监听
            SetDeviceState(kDeviceStateIdle);  // 设置为空闲状态
        }
    });
}

// 启动应用程序
void Application::Start() {
    auto& board = Board::GetInstance();
    SetDeviceState(kDeviceStateStarting);  // 设置为启动状态

    /* 设置显示 */
    auto display = board.GetDisplay();

    /* 设置音频编解码器 */
    auto codec = board.GetAudioCodec();
    opus_decode_sample_rate_ = codec->output_sample_rate();  // 设置解码采样率
    opus_decoder_ = std::make_unique<OpusDecoderWrapper>(opus_decode_sample_rate_, 1);  // 创建Opus解码器
    opus_encoder_ = std::make_unique<OpusEncoderWrapper>(16000, 1, OPUS_FRAME_DURATION_MS);  // 创建Opus编码器
    // 对于ML307开发板，设置编码复杂度为5以节省带宽
    // 对于其他开发板，设置编码复杂度为3以节省CPU
    if (board.GetBoardType() == "ml307") {
        ESP_LOGI(TAG, "ML307 board detected, setting opus encoder complexity to 5");
        opus_encoder_->SetComplexity(5);
    } else {
        ESP_LOGI(TAG, "WiFi board detected, setting opus encoder complexity to 3");
        opus_encoder_->SetComplexity(3);
    }

    // 如果输入采样率不是16000，配置重采样器
    if (codec->input_sample_rate() != 16000) {
        input_resampler_.Configure(codec->input_sample_rate(), 16000);
        reference_resampler_.Configure(codec->input_sample_rate(), 16000);
    }
    // 设置音频输入和输出的回调函数
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
    codec->Start();  // 启动音频编解码器

    /* 启动主循环 */
    xTaskCreate([](void* arg) {
        Application* app = (Application*)arg;
        app->MainLoop();  // 主循环
        vTaskDelete(NULL);
    }, "main_loop", 4096 * 2, this, 3, nullptr);

    /* 等待网络准备就绪 */
    board.StartNetwork();

    // 初始化协议
    display->SetStatus(Lang::Strings::LOADING_PROTOCOL);  // 设置状态为加载协议
#ifdef CONFIG_CONNECTION_TYPE_WEBSOCKET
    protocol_ = std::make_unique<WebsocketProtocol>();  // 使用WebSocket协议
#else
    protocol_ = std::make_unique<MqttProtocol>();  // 使用MQTT协议
#endif
    // 设置网络错误回调
    protocol_->OnNetworkError([this](const std::string& message) {
        SetDeviceState(kDeviceStateIdle);  // 设置为空闲状态
        Alert(Lang::Strings::ERROR, message.c_str(), "sad", Lang::Sounds::P3_EXCLAMATION);  // 显示错误信息
    });
    // 设置音频数据接收回调
    protocol_->OnIncomingAudio([this](std::vector<uint8_t>&& data) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (device_state_ == kDeviceStateSpeaking) {
            audio_decode_queue_.emplace_back(std::move(data));  // 将音频数据加入解码队列
        }
    });
    // 设置音频通道打开回调
    protocol_->OnAudioChannelOpened([this, codec, &board]() {
        board.SetPowerSaveMode(false);  // 关闭省电模式
        if (protocol_->server_sample_rate() != codec->output_sample_rate()) {
            ESP_LOGW(TAG, "Server sample rate %d does not match device output sample rate %d, resampling may cause distortion",
                protocol_->server_sample_rate(), codec->output_sample_rate());
        }
        SetDecodeSampleRate(protocol_->server_sample_rate());  // 设置解码采样率
        // 发送IoT设备描述符
        last_iot_states_.clear();
        auto& thing_manager = iot::ThingManager::GetInstance();
        protocol_->SendIotDescriptors(thing_manager.GetDescriptorsJson());
    });
    // 设置音频通道关闭回调
    protocol_->OnAudioChannelClosed([this, &board]() {
        board.SetPowerSaveMode(true);  // 开启省电模式
        Schedule([this]() {
            auto display = Board::GetInstance().GetDisplay();
            display->SetChatMessage("system", "");
            SetDeviceState(kDeviceStateIdle);  // 设置为空闲状态
        });
    });
    // 设置JSON数据接收回调
    protocol_->OnIncomingJson([this, display](const cJSON* root) {
        // 解析JSON数据
        auto type = cJSON_GetObjectItem(root, "type");
        if (strcmp(type->valuestring, "tts") == 0) {  // 文本转语音
            auto state = cJSON_GetObjectItem(root, "state");
            if (strcmp(state->valuestring, "start") == 0) {
                Schedule([this]() {
                    aborted_ = false;
                    if (device_state_ == kDeviceStateIdle || device_state_ == kDeviceStateListening) {
                        SetDeviceState(kDeviceStateSpeaking);  // 设置为说话状态
                    }
                });
            } else if (strcmp(state->valuestring, "stop") == 0) {
                Schedule([this]() {
                    if (device_state_ == kDeviceStateSpeaking) {
                        background_task_->WaitForCompletion();  // 等待后台任务完成
                        if (keep_listening_) {
                            protocol_->SendStartListening(kListeningModeAutoStop);  // 开始监听
                            SetDeviceState(kDeviceStateListening);  // 设置为监听状态
                        } else {
                            SetDeviceState(kDeviceStateIdle);  // 设置为空闲状态
                        }
                    }
                });
            } else if (strcmp(state->valuestring, "sentence_start") == 0) {
                auto text = cJSON_GetObjectItem(root, "text");
                if (text != NULL) {
                    ESP_LOGI(TAG, "<< %s", text->valuestring);
                    Schedule([this, display, message = std::string(text->valuestring)]() {
                        display->SetChatMessage("assistant", message.c_str());  // 显示助手消息
                    });
                }
            }
        } else if (strcmp(type->valuestring, "stt") == 0) {  // 语音转文本
            auto text = cJSON_GetObjectItem(root, "text");
            if (text != NULL) {
                ESP_LOGI(TAG, ">> %s", text->valuestring);
                Schedule([this, display, message = std::string(text->valuestring)]() {
                    display->SetChatMessage("user", message.c_str());  // 显示用户消息
                });
            }
        } else if (strcmp(type->valuestring, "llm") == 0) {  // 大语言模型
            auto emotion = cJSON_GetObjectItem(root, "emotion");
            if (emotion != NULL) {
                Schedule([this, display, emotion_str = std::string(emotion->valuestring)]() {
                    display->SetEmotion(emotion_str.c_str());  // 设置表情
                });
            }
        } else if (strcmp(type->valuestring, "iot") == 0) {  // IoT设备
            auto commands = cJSON_GetObjectItem(root, "commands");
            if (commands != NULL) {
                auto& thing_manager = iot::ThingManager::GetInstance();
                for (int i = 0; i < cJSON_GetArraySize(commands); ++i) {
                    auto command = cJSON_GetArrayItem(commands, i);
                    thing_manager.Invoke(command);  // 执行IoT命令
                }
            }
        }
    });
    protocol_->Start();  // 启动协议

    // 检查新固件版本或获取MQTT代理地址
    ota_.SetCheckVersionUrl(CONFIG_OTA_VERSION_URL);
    ota_.SetHeader("Device-Id", SystemInfo::GetMacAddress().c_str());
    ota_.SetHeader("Client-Id", board.GetUuid());
    ota_.SetHeader("Accept-Language", Lang::CODE);
    auto app_desc = esp_app_get_description();
    ota_.SetHeader("User-Agent", std::string(BOARD_NAME "/") + app_desc->version);

    // 创建任务检查新版本
    xTaskCreate([](void* arg) {
        Application* app = (Application*)arg;
        app->CheckNewVersion();  // 检查新版本
        vTaskDelete(NULL);
    }, "check_new_version", 4096 * 2, this, 2, nullptr);

#if CONFIG_USE_AUDIO_PROCESSOR
    // 初始化音频处理器
    audio_processor_.Initialize(codec->input_channels(), codec->input_reference());
    audio_processor_.OnOutput([this](std::vector<int16_t>&& data) {
        background_task_->Schedule([this, data = std::move(data)]() mutable {
            opus_encoder_->Encode(std::move(data), [this](std::vector<uint8_t>&& opus) {
                Schedule([this, opus = std::move(opus)]() {
                    protocol_->SendAudio(opus);  // 发送音频数据
                });
            });
        });
    });
#endif

#if CONFIG_USE_WAKE_WORD_DETECT
    // 初始化唤醒词检测
    wake_word_detect_.Initialize(codec->input_channels(), codec->input_reference());
    wake_word_detect_.OnVadStateChange([this](bool speaking) {
        Schedule([this, speaking]() {
            if (device_state_ == kDeviceStateListening) {
                if (speaking) {
                    voice_detected_ = true;  // 检测到语音
                } else {
                    voice_detected_ = false;  // 未检测到语音
                }
                auto led = Board::GetInstance().GetLed();
                led->OnStateChanged();  // 更新LED状态
            }
        });
    });

    // 设置唤醒词检测回调
    wake_word_detect_.OnWakeWordDetected([this](const std::string& wake_word) {
        Schedule([this, &wake_word]() {
            if (device_state_ == kDeviceStateIdle) {
                SetDeviceState(kDeviceStateConnecting);  // 设置为连接状态
                wake_word_detect_.EncodeWakeWordData();  // 编码唤醒词数据

                if (!protocol_->OpenAudioChannel()) {  // 打开音频通道
                    wake_word_detect_.StartDetection();  // 开始检测
                    return;
                }
                
                std::vector<uint8_t> opus;
                // 编码并发送唤醒词数据到服务器
                while (wake_word_detect_.GetWakeWordOpus(opus)) {
                    protocol_->SendAudio(opus);
                }
                // 设置聊天状态为唤醒词检测
                protocol_->SendWakeWordDetected(wake_word);
                ESP_LOGI(TAG, "Wake word detected: %s", wake_word.c_str());
                keep_listening_ = true;
                SetDeviceState(kDeviceStateListening);  // 设置为监听状态
            } else if (device_state_ == kDeviceStateSpeaking) {
                AbortSpeaking(kAbortReasonWakeWordDetected);  // 中止说话
            } else if (device_state_ == kDeviceStateActivating) {
                SetDeviceState(kDeviceStateIdle);  // 设置为空闲状态
            }

            // 恢复检测
            wake_word_detect_.StartDetection();
        });
    });
    wake_word_detect_.StartDetection();  // 开始唤醒词检测
#endif

    SetDeviceState(kDeviceStateIdle);  // 设置为空闲状态
    esp_timer_start_periodic(clock_timer_handle_, 1000000);  // 启动时钟定时器
}

// 时钟定时器回调函数
void Application::OnClockTimer() {
    clock_ticks_++;

    // 每10秒打印一次调试信息
    if (clock_ticks_ % 10 == 0) {
        // SystemInfo::PrintRealTimeStats(pdMS_TO_TICKS(1000));
        int free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        int min_free_sram = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
        ESP_LOGI(TAG, "Free internal: %u minimal internal: %u", free_sram, min_free_sram);

        // 如果已同步服务器时间，设置状态为时钟 "HH:MM"
        if (ota_.HasServerTime()) {
            if (device_state_ == kDeviceStateIdle) {
                Schedule([this]() {
                    // 设置状态为时钟 "HH:MM"
                    time_t now = time(NULL);
                    char time_str[64];
                    strftime(time_str, sizeof(time_str), "%H:%M  ", localtime(&now));
                    Board::GetInstance().GetDisplay()->SetStatus(time_str);
                });
            }
        }
    }
}

// 调度任务
void Application::Schedule(std::function<void()> callback) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        main_tasks_.push_back(std::move(callback));  // 将任务加入主任务队列
    }
    xEventGroupSetBits(event_group_, SCHEDULE_EVENT);  // 设置调度事件
}

// 主循环，控制聊天状态和WebSocket连接
void Application::MainLoop() {
    while (true) {
        auto bits = xEventGroupWaitBits(event_group_,
            SCHEDULE_EVENT | AUDIO_INPUT_READY_EVENT | AUDIO_OUTPUT_READY_EVENT,
            pdTRUE, pdFALSE, portMAX_DELAY);

        if (bits & AUDIO_INPUT_READY_EVENT) {
            InputAudio();  // 处理音频输入
        }
        if (bits & AUDIO_OUTPUT_READY_EVENT) {
            OutputAudio();  // 处理音频输出
        }
        if (bits & SCHEDULE_EVENT) {
            std::unique_lock<std::mutex> lock(mutex_);
            std::list<std::function<void()>> tasks = std::move(main_tasks_);
            lock.unlock();
            for (auto& task : tasks) {
                task();  // 执行任务
            }
        }
    }
}

// 重置解码器
void Application::ResetDecoder() {
    std::lock_guard<std::mutex> lock(mutex_);
    opus_decoder_->ResetState();  // 重置解码器状态
    audio_decode_queue_.clear();  // 清空音频解码队列
    last_output_time_ = std::chrono::steady_clock::now();
}

// 输出音频
void Application::OutputAudio() {
    auto now = std::chrono::steady_clock::now();
    auto codec = Board::GetInstance().GetAudioCodec();
    const int max_silence_seconds = 10;  // 最大静音时间

    std::unique_lock<std::mutex> lock(mutex_);
    if (audio_decode_queue_.empty()) {
        // 如果长时间没有音频数据，禁用输出
        if (device_state_ == kDeviceStateIdle) {
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - last_output_time_).count();
            if (duration > max_silence_seconds) {
                codec->EnableOutput(false);  // 禁用音频输出
            }
        }
        return;
    }

    if (device_state_ == kDeviceStateListening) {
        audio_decode_queue_.clear();  // 清空音频解码队列
        return;
    }

    last_output_time_ = now;
    auto opus = std::move(audio_decode_queue_.front());
    audio_decode_queue_.pop_front();
    lock.unlock();

    background_task_->Schedule([this, codec, opus = std::move(opus)]() mutable {
        if (aborted_) {
            return;
        }

        std::vector<int16_t> pcm;
        if (!opus_decoder_->Decode(std::move(opus), pcm)) {  // 解码音频数据
            return;
        }

        // 如果采样率不同，进行重采样
        if (opus_decode_sample_rate_ != codec->output_sample_rate()) {
            int target_size = output_resampler_.GetOutputSamples(pcm.size());
            std::vector<int16_t> resampled(target_size);
            output_resampler_.Process(pcm.data(), pcm.size(), resampled.data());
            pcm = std::move(resampled);
        }
        
        codec->OutputData(pcm);  // 输出音频数据
    });
}

// 输入音频
void Application::InputAudio() {
    auto codec = Board::GetInstance().GetAudioCodec();
    std::vector<int16_t> data;
    if (!codec->InputData(data)) {  // 获取音频输入数据
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

#if CONFIG_USE_WAKE_WORD_DETECT
    if (wake_word_detect_.IsDetectionRunning()) {
        wake_word_detect_.Feed(data);  // 喂入音频数据到唤醒词检测
    }
#endif
#if CONFIG_USE_AUDIO_PROCESSOR
    if (audio_processor_.IsRunning()) {
        audio_processor_.Input(data);  // 处理音频数据
    }
#else
    if (device_state_ == kDeviceStateListening) {
        background_task_->Schedule([this, data = std::move(data)]() mutable {
            opus_encoder_->Encode(std::move(data), [this](std::vector<uint8_t>&& opus) {
                Schedule([this, opus = std::move(opus)]() {
                    protocol_->SendAudio(opus);  // 发送音频数据
                });
            });
        });
    }
#endif
}

// 中止说话
void Application::AbortSpeaking(AbortReason reason) {
    ESP_LOGI(TAG, "Abort speaking");
    aborted_ = true;
    protocol_->SendAbortSpeaking(reason);  // 发送中止说话命令
}

// 设置设备状态
void Application::SetDeviceState(DeviceState state) {
    if (device_state_ == state) {
        return;
    }
    
    clock_ticks_ = 0;
    auto previous_state = device_state_;
    device_state_ = state;
    ESP_LOGI(TAG, "STATE: %s", STATE_STRINGS[device_state_]);  // 记录状态变化
    // 状态变化，等待所有后台任务完成
    background_task_->WaitForCompletion();

    auto& board = Board::GetInstance();
    auto codec = board.GetAudioCodec();
    auto display = board.GetDisplay();
    auto led = board.GetLed();
    led->OnStateChanged();  // 更新LED状态
    switch (state) {
        case kDeviceStateUnknown:
        case kDeviceStateIdle:
            display->SetStatus(Lang::Strings::STANDBY);  // 设置状态为待机
            display->SetEmotion("neutral");  // 设置表情为中性
#if CONFIG_USE_AUDIO_PROCESSOR
            audio_processor_.Stop();  // 停止音频处理器
#endif
            break;
        case kDeviceStateConnecting:
            display->SetStatus(Lang::Strings::CONNECTING);  // 设置状态为连接中
            display->SetEmotion("neutral");  // 设置表情为中性
            display->SetChatMessage("system", "");  // 清空消息
            break;
        case kDeviceStateListening:
            display->SetStatus(Lang::Strings::LISTENING);  // 设置状态为监听中
            display->SetEmotion("neutral");  // 设置表情为中性
            ResetDecoder();  // 重置解码器
            opus_encoder_->ResetState();  // 重置编码器状态
#if CONFIG_USE_AUDIO_PROCESSOR
            audio_processor_.Start();  // 启动音频处理器
#endif
            UpdateIotStates();  // 更新IoT状态
            if (previous_state == kDeviceStateSpeaking) {
                // FIXME: 等待扬声器清空缓冲区
                vTaskDelay(pdMS_TO_TICKS(120));
            }
            break;
        case kDeviceStateSpeaking:
            display->SetStatus(Lang::Strings::SPEAKING);  // 设置状态为说话中
            ResetDecoder();  // 重置解码器
            codec->EnableOutput(true);  // 启用音频输出
#if CONFIG_USE_AUDIO_PROCESSOR
            audio_processor_.Stop();  // 停止音频处理器
#endif
            break;
        default:
            // 其他状态不做处理
            break;
    }
}

// 设置解码采样率
void Application::SetDecodeSampleRate(int sample_rate) {
    if (opus_decode_sample_rate_ == sample_rate) {
        return;
    }

    opus_decode_sample_rate_ = sample_rate;
    opus_decoder_.reset();
    opus_decoder_ = std::make_unique<OpusDecoderWrapper>(opus_decode_sample_rate_, 1);  // 创建新的解码器

    auto codec = Board::GetInstance().GetAudioCodec();
    if (opus_decode_sample_rate_ != codec->output_sample_rate()) {
        ESP_LOGI(TAG, "Resampling audio from %d to %d", opus_decode_sample_rate_, codec->output_sample_rate());
        output_resampler_.Configure(opus_decode_sample_rate_, codec->output_sample_rate());  // 配置重采样器
    }
}

// 更新IoT状态
void Application::UpdateIotStates() {
    auto& thing_manager = iot::ThingManager::GetInstance();
    auto states = thing_manager.GetStatesJson();
    if (states != last_iot_states_) {
        last_iot_states_ = states;
        protocol_->SendIotStates(states);  // 发送IoT状态
    }
}

// 重启设备
void Application::Reboot() {
    ESP_LOGI(TAG, "Rebooting...");
    esp_restart();  // 重启设备
}

// 唤醒词调用
void Application::WakeWordInvoke(const std::string& wake_word) {
    if (device_state_ == kDeviceStateIdle) {
        ToggleChatState();  // 切换聊天状态
        Schedule([this, wake_word]() {
            if (protocol_) {
                protocol_->SendWakeWordDetected(wake_word);  // 发送唤醒词检测
            }
        }); 
    } else if (device_state_ == kDeviceStateSpeaking) {
        Schedule([this]() {
            AbortSpeaking(kAbortReasonNone);  // 中止说话
        });
    } else if (device_state_ == kDeviceStateListening) {   
        Schedule([this]() {
            if (protocol_) {
                protocol_->CloseAudioChannel();  // 关闭音频通道
            }
        });
    }
}

// 判断是否可以进入睡眠模式
bool Application::CanEnterSleepMode() {
    if (device_state_ != kDeviceStateIdle) {
        return false;  // 如果设备不处于空闲状态，不能进入睡眠模式
    }

    if (protocol_ && protocol_->IsAudioChannelOpened()) {
        return false;  // 如果音频通道已打开，不能进入睡眠模式
    }

    // 现在可以安全进入睡眠模式
    return true;
}