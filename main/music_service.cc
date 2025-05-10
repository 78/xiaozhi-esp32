#include "music_service.h"
#include <esp_log.h>
#include <cstring>
#include <esp_crt_bundle.h>
#include "board.h"
#include "audio_codec.h"
#include <inttypes.h>
#include <cctype>
#include "application.h"
#include "display/display.h"
#include "esp_audio_dec_default.h"
#include "esp_audio_simple_dec_default.h"

#define TAG "MusicService"
#define PRELOAD_BUFFER_THRESHOLD 50
#define BUFFER_LOW_THRESHOLD 5
#define BUFFER_HIGH_THRESHOLD 25
#define BACKGROUND_PLAY_MODE true

bool g_music_active = false;
bool g_audio_output_managed_by_music = false;
int32_t g_last_voice_ratio = 50;
bool g_music_interrupt_requested = false;

extern "C" {
    bool CheckMusicActiveStatus() {
        return g_music_active;
    }
    
    bool IsAudioOutputManagedByMusic() {
        return g_audio_output_managed_by_music;
    }
    
    void RequestMusicInterrupt() {
        g_music_interrupt_requested = true;
    }
    
    bool IsMusicInterruptRequested() {
        return g_music_interrupt_requested;
    }
    
    void ClearMusicInterruptRequest() {
        g_music_interrupt_requested = false;
    }
}

std::string UrlEncode(const std::string& str);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreorder"
MusicService::MusicService() 
    : is_playing_(false)
    , song_id_(0)
    , keyword_()
    , current_song_name_()
    , current_artist_()
    , url_to_play_()
    , streaming_task_handle_(nullptr)
    , audio_queue_(nullptr)
    , sample_rate_(44100)
    , bits_per_sample_(16)
    , channels_(2)
    , actual_codec_sample_rate_(0)
    , mp3_decoder_(nullptr)
    , is_decoder_initialized_(false)
    , decode_input_buffer_(nullptr)
    , decode_output_buffer_(nullptr)
    , decode_input_buffer_size_(0)
    , decode_output_buffer_size_(0)
    , lyrics_()
    , current_lyric_index_(0)
    , playback_start_time_(0)
    , has_lyrics_(false)
    , should_continue_search_(true) 
    , http_client_close_requested_(false)
    , active_http_client_(nullptr) {
#pragma GCC diagnostic pop
    
    audio_queue_ = xQueueCreate(AUDIO_QUEUE_SIZE, sizeof(std::vector<uint8_t>*));
    if (audio_queue_ == nullptr) {
        ESP_LOGE(TAG, "无法创建音频队列");
    }
    
    g_music_active = false;
    g_audio_output_managed_by_music = false;
}

MusicService::~MusicService() {
    Stop();
    
    CleanupMp3Decoder();
    
    if (audio_queue_ != nullptr) {
        std::vector<uint8_t>* buffer;
        while (xQueueReceive(audio_queue_, &buffer, 0) == pdTRUE) {
            delete buffer;
        }
        vQueueDelete(audio_queue_);
        audio_queue_ = nullptr;
    }
    
    g_music_active = false;
    g_audio_output_managed_by_music = false;
}

bool MusicService::Initialize() {
    return true;
}

bool MusicService::PlaySong(const std::string& keyword) {
    should_continue_search_ = true;
    g_music_interrupt_requested = false;
    
    g_music_active = true;
    
    auto& app = Application::GetInstance();
    app.SetDeviceState(kDeviceStateMusicPlaying);
    
    if (!should_continue_search_ || g_music_interrupt_requested) {
        g_music_active = false;
        return false;
    }
    
    if (!SearchMusic(keyword)) {
        ESP_LOGE(TAG, "搜索歌曲失败，关键词: %s", keyword.c_str());
        g_music_active = false;
        return false;
    }
    
    if (!should_continue_search_ || g_music_interrupt_requested) {
        g_music_active = false;
        return false;
    }
    
    return StartStreaming(url_to_play_);
}

bool MusicService::Stop() {
    if (!is_playing_) {
        g_music_interrupt_requested = false;
        return true;
    }
    
    should_continue_search_ = false;
    g_music_interrupt_requested = false;
    
    is_playing_ = false;
    
    g_music_active = false;
    
    if (streaming_task_handle_ != nullptr) {
        for (int i = 0; i < 10; i++) {
            if (eTaskGetState(streaming_task_handle_) == eDeleted) {
                streaming_task_handle_ = nullptr;
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        
        if (streaming_task_handle_ != nullptr) {
            ESP_LOGW(TAG, "流式播放任务仍在运行，不强制删除以避免崩溃");
            streaming_task_handle_ = nullptr;
        }
    }
    
    if (audio_queue_ != nullptr) {
        std::vector<uint8_t>* buffer = nullptr;
        while (xQueueReceive(audio_queue_, &buffer, 0) == pdTRUE) {
            if (buffer != nullptr) {
                delete buffer;
            }
        }
    }
    
    auto& app = Application::GetInstance();
    
    app.ForceResetAudioHardware();
    
    app.ReleaseAudioState(AUDIO_STATE_MUSIC);
    
    if (g_audio_output_managed_by_music) {
        g_audio_output_managed_by_music = false;
    }
    
    auto& board = Board::GetInstance();
    auto codec = board.GetAudioCodec();
    
    if (auto* display = board.GetDisplay()) {
        display->SetChatMessage("assistant", "播放结束");
    }
    
    sample_rate_ = 44100;
    bits_per_sample_ = 16;
    channels_ = 2;
    actual_codec_sample_rate_ = 0;
    
    if (app.GetDeviceState() != kDeviceStateMusicPlaying) {
        xTaskCreate(
            [](void* arg) {
                vTaskDelay(pdMS_TO_TICKS(1000));
                
                auto& app = Application::GetInstance();
                auto protocol = app.GetProtocol();
                if (protocol && !protocol->IsAudioChannelOpened()) {
                    protocol->OpenAudioChannel();
                }
                
                vTaskDelete(nullptr);
            },
            "RestoreMQTT",
            4096,
            nullptr,
            5,
            nullptr
        );
    }
    
    return true;
}

bool MusicService::IsPlaying() const {
    return is_playing_;
}

std::string MusicService::GetCurrentSongInfo() const {
    if (!is_playing_) {
        return "没有正在播放的歌曲";
    }
    
    return current_song_name_ + " - " + current_artist_;
}

bool MusicService::SearchMusic(const std::string& keyword) {
    should_continue_search_ = true;
    current_song_name_ = "";
    current_artist_ = "";
    song_id_ = 0;
    url_to_play_ = "";
    
    char* encoded_buf = (char*)heap_caps_malloc(128, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    char* search_url = (char*)heap_caps_malloc(512, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    char* url_fetch_url = (char*)heap_caps_malloc(512, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    char* response_buffer = (char*)heap_caps_malloc(HTTP_RESPONSE_BUFFER_SIZE, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    
    if (!encoded_buf || !search_url || !url_fetch_url || !response_buffer) {
        ESP_LOGE(TAG, "无法分配搜索所需内存");
        if (encoded_buf) heap_caps_free(encoded_buf);
        if (search_url) heap_caps_free(search_url);
        if (url_fetch_url) heap_caps_free(url_fetch_url);
        if (response_buffer) heap_caps_free(response_buffer);
        return false;
    }
    
    auto& board = Board::GetInstance();
    
    std::string encoded_keyword = UrlEncode(keyword);
    strncpy(encoded_buf, encoded_keyword.c_str(), 128);
    encoded_buf[127] = '\0';
    
    bool success = false;
    std::string current_source = "kuwo";
    
    do {
        if (!should_continue_search_ || g_music_interrupt_requested) {
            heap_caps_free(response_buffer);
            heap_caps_free(encoded_buf);
            heap_caps_free(search_url);
            heap_caps_free(url_fetch_url);
            return false;
        }
        
        snprintf(search_url, 512, 
                "https://music-api.gdstudio.xyz/api.php?types=search&source=%s&count=10&pages=1&name=%s",
                current_source.c_str(), encoded_buf);
        
        snprintf(url_fetch_url, 512, 
                "https://music-api.gdstudio.xyz/api.php?types=url&source=%s&id=%%d&br=320",
                current_source.c_str());
        
        if (board.GetDisplay()) {
            std::string searchInfo = "正在搜索: ";
            searchInfo += keyword;
            board.GetDisplay()->SetChatMessage("assistant", searchInfo.c_str());
        }
        
        memset(response_buffer, 0, HTTP_RESPONSE_BUFFER_SIZE);
        int response_len = 0;
    
        if (!should_continue_search_ || g_music_interrupt_requested) {
            heap_caps_free(encoded_buf);
            heap_caps_free(search_url);
            heap_caps_free(url_fetch_url);
            heap_caps_free(response_buffer);
            return false;
        }
        
        bool http_success = SendHttpRequest(search_url, HTTP_METHOD_GET, 
                            nullptr, 0, 
                         response_buffer, HTTP_RESPONSE_BUFFER_SIZE, 
                            &response_len, "application/json; charset=UTF-8",
                            true);
                            
        if (!should_continue_search_ || g_music_interrupt_requested) {
            heap_caps_free(encoded_buf);
            heap_caps_free(search_url);
            heap_caps_free(url_fetch_url);
            heap_caps_free(response_buffer);
            return false;
        }
    
        if (!http_success) {
            if (response_len > 0) {
                ESP_LOGE(TAG, "API错误响应: %s", response_buffer);
            }
            
            heap_caps_free(response_buffer);
            heap_caps_free(encoded_buf);
            heap_caps_free(search_url);
            heap_caps_free(url_fetch_url);
            return false;
        }
    
        if (response_len <= 0) {
            ESP_LOGE(TAG, "搜索API返回空响应");
            
            heap_caps_free(response_buffer);
            heap_caps_free(encoded_buf);
            heap_caps_free(search_url);
            heap_caps_free(url_fetch_url);
            return false;
        }
    
        if (response_len < HTTP_RESPONSE_BUFFER_SIZE) {
            response_buffer[response_len] = '\0';
        } else {
            response_buffer[HTTP_RESPONSE_BUFFER_SIZE - 1] = '\0';
        }
    
        cJSON *root = cJSON_Parse(response_buffer);
        if (root == NULL) {
            ESP_LOGE(TAG, "解析响应JSON失败");
            
            heap_caps_free(response_buffer);
            heap_caps_free(encoded_buf);
            heap_caps_free(search_url);
            heap_caps_free(url_fetch_url);
            return false;
        }
    
        if (!cJSON_IsArray(root) || cJSON_GetArraySize(root) == 0) {
            ESP_LOGE(TAG, "响应格式错误或搜索结果为空");
            cJSON_Delete(root);
            
            heap_caps_free(response_buffer);
            heap_caps_free(encoded_buf);
            heap_caps_free(search_url);
            heap_caps_free(url_fetch_url);
            return false;
        }
        
        cJSON *song = cJSON_GetArrayItem(root, 0);
        if (song == NULL) {
            ESP_LOGE(TAG, "无法获取歌曲信息");
            cJSON_Delete(root);
            
            heap_caps_free(response_buffer);
            heap_caps_free(encoded_buf);
            heap_caps_free(search_url);
            heap_caps_free(url_fetch_url);
            return false;
        }
        
        cJSON *id = cJSON_GetObjectItem(song, "id");
        cJSON *name = cJSON_GetObjectItem(song, "name");
        cJSON *artist_array = cJSON_GetObjectItem(song, "artist");
        
        if (!id || !cJSON_IsString(id) || !name || !cJSON_IsString(name)) {
            ESP_LOGE(TAG, "歌曲信息字段缺失或格式错误");
            cJSON_Delete(root);
            
            heap_caps_free(response_buffer);
            heap_caps_free(encoded_buf);
            heap_caps_free(search_url);
            heap_caps_free(url_fetch_url);
            return false;
        }
        
        const char* id_str = id->valuestring;
        song_id_ = atoi(id_str);
        if (song_id_ <= 0) {
            ESP_LOGE(TAG, "歌曲ID无效: %s", id_str);
            cJSON_Delete(root);
            
            heap_caps_free(response_buffer);
            heap_caps_free(encoded_buf);
            heap_caps_free(search_url);
            heap_caps_free(url_fetch_url);
            return false;
        }
        
        current_song_name_ = name->valuestring;
        
        current_artist_ = "未知艺术家";
        if (artist_array && cJSON_IsArray(artist_array) && cJSON_GetArraySize(artist_array) > 0) {
            cJSON *first_artist = cJSON_GetArrayItem(artist_array, 0);
            if (first_artist && cJSON_IsString(first_artist)) {
                current_artist_ = first_artist->valuestring;
            }
        }
        
        cJSON_Delete(root);
        
        if (!should_continue_search_ || g_music_interrupt_requested) {
            heap_caps_free(encoded_buf);
            heap_caps_free(search_url);
            heap_caps_free(url_fetch_url);
            heap_caps_free(response_buffer);
            return false;
        }
        
        snprintf(url_fetch_url, 512, 
                "https://music-api.gdstudio.xyz/api.php?types=url&source=%s&id=%d&br=320",
                current_source.c_str(), song_id_);
        
        auto& board = Board::GetInstance();
        if (auto* display = board.GetDisplay()) {
            std::string matchInfo = "匹配歌曲：" + current_song_name_;
            if (!current_artist_.empty()) {
                matchInfo += " - " + current_artist_;
            }
            display->SetChatMessage("assistant", matchInfo.c_str());
        }
        
        if (!should_continue_search_ || g_music_interrupt_requested) {
            heap_caps_free(encoded_buf);
            heap_caps_free(search_url);
            heap_caps_free(url_fetch_url);
            heap_caps_free(response_buffer);
            return false;
        }
        
        memset(response_buffer, 0, HTTP_RESPONSE_BUFFER_SIZE);
        response_len = 0;
        
        http_success = SendHttpRequest(url_fetch_url, HTTP_METHOD_GET,
                                    nullptr, 0,
                                    response_buffer, HTTP_RESPONSE_BUFFER_SIZE,
                                    &response_len, "application/json; charset=UTF-8",
                                    true);
        
        if (!http_success) {
            if (response_len > 0) {
                ESP_LOGE(TAG, "API错误响应: %s", response_buffer);
            }
            
            heap_caps_free(response_buffer);
            heap_caps_free(encoded_buf);
            heap_caps_free(search_url);
            heap_caps_free(url_fetch_url);
            return false;
        }
        
        if (response_len < HTTP_RESPONSE_BUFFER_SIZE) {
            response_buffer[response_len] = '\0';
        } else {
            response_buffer[HTTP_RESPONSE_BUFFER_SIZE - 1] = '\0';
        }
        
        cJSON *url_root = cJSON_Parse(response_buffer);
        if (url_root == NULL) {
            ESP_LOGE(TAG, "解析URL响应JSON失败");
            
            heap_caps_free(response_buffer);
            heap_caps_free(encoded_buf);
            heap_caps_free(search_url);
            heap_caps_free(url_fetch_url);
            return false;
        }
        
        cJSON *url_data = cJSON_GetObjectItem(url_root, "url");
        if (url_data == NULL || !cJSON_IsString(url_data)) {
            ESP_LOGE(TAG, "未找到歌曲URL");
            cJSON_Delete(url_root);
            
            heap_caps_free(response_buffer);
            heap_caps_free(encoded_buf);
            heap_caps_free(search_url);
            heap_caps_free(url_fetch_url);
            return false;
        }
        
        url_to_play_ = url_data->valuestring;
        
        bool is_mp3_format = true;
        std::string url_lower = url_to_play_;
        std::transform(url_lower.begin(), url_lower.end(), url_lower.begin(), 
                   [](unsigned char c){ return std::tolower(c); });
        
        if (url_lower.find(".aac") != std::string::npos || 
            url_lower.find(".flac") != std::string::npos ||
            url_lower.find(".wav") != std::string::npos ||
            url_lower.find(".ogg") != std::string::npos) {
            is_mp3_format = false;
            ESP_LOGW(TAG, "检测到非MP3格式音频URL: %s", url_to_play_.c_str());
        }
        
        cJSON_Delete(url_root);
        
        if (!current_song_name_.empty() && board.GetDisplay()) {
            std::string playInfo = "开始播放：" + current_song_name_;
            if (!current_artist_.empty()) {
                playInfo += " - " + current_artist_;
            }
            board.GetDisplay()->SetChatMessage("assistant", playInfo.c_str());
        }
        
        success = true;
        break;
        
    } while (false);
    
    heap_caps_free(response_buffer);
    heap_caps_free(encoded_buf);
    heap_caps_free(search_url);
    heap_caps_free(url_fetch_url);
    
    return success;
}

bool MusicService::StartStreaming(const std::string& url) {
    Stop();
    
    url_to_play_ = url;
    
    auto& board = Board::GetInstance();
    auto codec = board.GetAudioCodec();
    
    sample_rate_ = 44100;
    bits_per_sample_ = 16;
    channels_ = 2;
    
    g_audio_output_managed_by_music = true;
    
    if (codec) {
    codec->EnableOutput(true);
    } else {
        ESP_LOGE(TAG, "获取编解码器失败，无法控制输出");
    }
    
    current_lyric_index_ = 0;
    has_lyrics_ = false;
    lyrics_.clear();
    
    if (song_id_ > 0) {
        xTaskCreate(
            [](void* arg) {
                MusicService* service = static_cast<MusicService*>(arg);
                vTaskDelay(pdMS_TO_TICKS(1000));
                
                bool success = false;
                
                try {
                    success = service->FetchLyrics(service->song_id_);
                } catch(...) {
                    ESP_LOGE(TAG, "获取歌词时发生异常");
                }
                
                if (success) {
                    service->playback_start_time_ = xTaskGetTickCount() * portTICK_PERIOD_MS;
                } else {
                    service->has_lyrics_ = false;
                }
                
                vTaskDelete(nullptr);
            },
            "LyricsTask",
            6144,
            this,
            2,
            nullptr
        );
    }
    
    is_playing_ = true;
    
    BaseType_t ret = xTaskCreate(
        [](void* arg) {
            MusicService* service = static_cast<MusicService*>(arg);
            service->StreamingTask();
        },
        "StreamingTask",
        12288,
        this,
        5,
        &streaming_task_handle_
    );
    
    if (ret != pdPASS) {
        is_playing_ = false;
        g_music_active = false;
        g_audio_output_managed_by_music = false;
        
        auto& app = Application::GetInstance();
        app.ReleaseAudioState(AUDIO_STATE_MUSIC);
        
        ESP_LOGE(TAG, "Free heap: %" PRIu32 " bytes", esp_get_free_heap_size());
        ESP_LOGE(TAG, "Minimum free heap: %" PRIu32 " bytes", esp_get_minimum_free_heap_size());
        
        return false;
    }
    
    return streaming_task_handle_ != nullptr;
}

bool MusicService::IsMusicActive() {
    return g_music_active;
}


void MusicService::StreamingTask() {
    auto& app = Application::GetInstance();
    
    g_music_active = true;
    
    app.SetDeviceState(kDeviceStateMusicPlaying);
    
    app.AbortSpeaking(kAbortReasonPlayMusic);
    
    app.ReleaseAudioState(AUDIO_STATE_LISTENING | AUDIO_STATE_SPEAKING);

    bool success = app.RequestAudioState(AUDIO_STATE_MUSIC);
    if (!success) {
        
        for (int i = 0; i < 3; i++) {
            vTaskDelay(pdMS_TO_TICKS(20));
            success = app.RequestAudioState(AUDIO_STATE_MUSIC);
            if (success) {
                break;
            }
            
            if (g_music_interrupt_requested) {
                g_music_active = false;
                is_playing_ = false;
                vTaskDelete(nullptr);
                return;
            }
        }
    }
    
    CleanupMp3Decoder();
    
    if (!InitMp3Decoder()) {
        ESP_LOGE(TAG, "MP3解码器初始化失败，无法播放");
        is_playing_ = false;
        g_music_active = false;
        g_audio_output_managed_by_music = false;
        vTaskDelete(nullptr);
        return;
    }
    
    esp_http_client_config_t config = {};
    config.url = url_to_play_.c_str();
    config.method = HTTP_METHOD_GET;
    config.timeout_ms = 30000;
    
    config.buffer_size = 8192;
    config.buffer_size_tx = 2048;
    
    if (strncmp(url_to_play_.c_str(), "https", 5) == 0) {
        config.crt_bundle_attach = esp_crt_bundle_attach;
    }
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        ESP_LOGE(TAG, "HTTP客户端初始化失败");
        CleanupMp3Decoder();
        is_playing_ = false;
        g_music_active = false;
        g_audio_output_managed_by_music = false;
        vTaskDelete(nullptr);
        return;
    }
    
    esp_http_client_set_header(client, "Accept", "audio/mpeg");
    
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP连接打开失败: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        CleanupMp3Decoder();
        is_playing_ = false;
        g_music_active = false;
        g_audio_output_managed_by_music = false;
        vTaskDelete(nullptr);
        return;
    }
    
    int content_length = esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);
    
    char *content_type = NULL;
    esp_http_client_get_header(client, "Content-Type", &content_type);
    
    if (status_code != 200) {
        ESP_LOGE(TAG, "HTTP请求失败，状态码: %d", status_code);
        esp_http_client_cleanup(client);
        CleanupMp3Decoder();
        is_playing_ = false;
        g_music_active = false;
        g_audio_output_managed_by_music = false;
        vTaskDelete(nullptr);
        return;
    }
    
    bool is_possibly_non_mp3 = false;
    if (content_type != NULL) {
        if (strstr(content_type, "audio/aac") != NULL ||
            strstr(content_type, "audio/flac") != NULL ||
            strstr(content_type, "audio/wav") != NULL ||
            strstr(content_type, "audio/x-aac") != NULL) {
            ESP_LOGW(TAG, "检测到非MP3音频格式: %s", content_type);
            is_possibly_non_mp3 = true;
        }
    }
    
    if (!is_possibly_non_mp3) {
        std::string url_lower = url_to_play_;
        std::transform(url_lower.begin(), url_lower.end(), url_lower.begin(), 
                      [](unsigned char c){ return std::tolower(c); });
        
        if (url_lower.find(".aac") != std::string::npos || 
            url_lower.find(".flac") != std::string::npos ||
            url_lower.find(".wav") != std::string::npos ||
            url_lower.find(".ogg") != std::string::npos) {
            ESP_LOGW(TAG, "URL检测到可能的非MP3格式: %s", url_to_play_.c_str());
            is_possibly_non_mp3 = true;
        }
    }
    
    TaskHandle_t player_task_handle = nullptr;
    
    auto& board = Board::GetInstance();
    auto codec = board.GetAudioCodec();
    
    codec->EnableOutput(true);
    
    app.SetDeviceState(kDeviceStateMusicPlaying);
    
    uint8_t *mp3_buffer = new uint8_t[AUDIO_BUFFER_SIZE];
    if (mp3_buffer == nullptr) {
        ESP_LOGE(TAG, "音频缓冲区分配失败");
        esp_http_client_cleanup(client);
        CleanupMp3Decoder();
        is_playing_ = false;
        g_music_active = false;
        g_audio_output_managed_by_music = false;
        vTaskDelete(nullptr);
        return;
    }
    
    int bytes_read = 0;
    uint32_t last_state_check_time = 0;
    uint32_t last_audio_check_time = 0;
    uint32_t last_lyric_check_time = 0;
    int decode_error_count = 0;
    bool format_error_detected = false;
    
    bool buffer_full = false;
    bool player_task_started = false;
    uint32_t buffer_check_time = 0;
    int consecutive_small_reads = 0;
    
    if (is_possibly_non_mp3) {
        ESP_LOGW(TAG, "预先检测到可能不支持的音频格式，将监控解码错误");
    }
    
    while (is_playing_ && audio_queue_ != nullptr) {
        if (g_music_interrupt_requested) {
            is_playing_ = false;
            break;
        }
        
        if (!is_playing_ || audio_queue_ == nullptr) {
            break;
        }
        
        UBaseType_t queue_size = uxQueueMessagesWaiting(audio_queue_);
        
        if (queue_size >= BUFFER_HIGH_THRESHOLD) {
            if (!buffer_full) {
                buffer_full = true;
            }
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        } else {
            buffer_full = false;
        }
        
        if (!player_task_started && queue_size >= PRELOAD_BUFFER_THRESHOLD) {
            if (auto* display = board.GetDisplay()) {
                char display_message[256];
                snprintf(display_message, sizeof(display_message), "正在播放: %s - %s", 
                         current_song_name_.c_str(), current_artist_.c_str());
                display->SetChatMessage("assistant", display_message);
            }
            
            BaseType_t ret = xTaskCreate(
                AudioPlayerTask,
                "AudioPlayerTask",
                8192,
                this,
                15,
                &player_task_handle
            );
    
            if (ret != pdPASS) {
                ESP_LOGE(TAG, "创建音频播放任务失败");
                delete[] mp3_buffer;
                esp_http_client_cleanup(client);
                is_playing_ = false;
                g_music_active = false;
                g_audio_output_managed_by_music = false;
                CleanupMp3Decoder();
                vTaskDelete(nullptr);
                return;
            }
            
            player_task_started = true;
        }
        
        if (!is_playing_ || audio_queue_ == nullptr) {
            break;
        }
        
        bytes_read = esp_http_client_read(client, (char*)mp3_buffer, AUDIO_BUFFER_SIZE);
        
        if (bytes_read <= 0) {
            if (bytes_read < 0) {
                ESP_LOGE(TAG, "HTTP读取错误");
            }
            
            if (!is_playing_ || audio_queue_ == nullptr) {
                break;
            }
            
            if (is_decoder_initialized_) {
                ProcessMp3Data(mp3_buffer, 0, true);
            }
            
            auto& board = Board::GetInstance();
            if (auto* display = board.GetDisplay()) {
                display->SetChatMessage("assistant", "播放结束");
            }
            
            break;
        }
        
        if (!is_playing_ || audio_queue_ == nullptr) {
            break;
        }
        
        if (bytes_read < 1024) {
            consecutive_small_reads++;
            if (consecutive_small_reads > 5) {
                if (queue_size < BUFFER_LOW_THRESHOLD) {
                    ESP_LOGW(TAG, "检测到网络缓慢，缓冲区偏低(%d)，增加读取间隔", queue_size);
                    vTaskDelay(pdMS_TO_TICKS(50));
                }
            }
        } else {
            consecutive_small_reads = 0;
        }
        
        ESP_LOGD(TAG, "读取了 %d 字节的MP3音频数据", bytes_read);
        
        bool decode_success = ProcessMp3Data(mp3_buffer, bytes_read, false);
        if (!decode_success) {
            decode_error_count++;
            
            if (decode_error_count >= 3) {
                format_error_detected = true;
                ESP_LOGE(TAG, "MP3解码连续失败%d次，很可能是不支持的音频格式", decode_error_count);
                
                auto& board = Board::GetInstance();
                if (auto* display = board.GetDisplay()) {
                    display->SetChatMessage("assistant", "无法播放该音频格式，请尝试其他音乐");
                }
                
                break;
            }
            
            ESP_LOGW(TAG, "MP3解码失败 (#%d)，可能为非MP3格式，尝试继续处理", decode_error_count);
            
            continue;
        } else {
            decode_error_count = 0;
        }
        
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (current_time - last_audio_check_time > 500) {
            auto& board = Board::GetInstance();
            auto codec = board.GetAudioCodec();
            if (codec != nullptr) {
                codec->EnableOutput(true);
            }
            last_audio_check_time = current_time;
        }
        
        if (current_time - last_state_check_time > 1000) {
            if (g_music_interrupt_requested) {
                is_playing_ = false;
                break;
            }
            
            DeviceState current_state = app.GetDeviceState();
            
            if (current_state == kDeviceStateIdle) {
                if (!is_playing_) {
                    break;
                }
                
                if (!g_audio_output_managed_by_music) {
                    auto& board = Board::GetInstance();
                    auto codec = board.GetAudioCodec();
                    if (codec != nullptr) {
                        g_audio_output_managed_by_music = true;
                        codec->EnableOutput(true);
                    }
                }
                
                app.SetDeviceState(kDeviceStateMusicPlaying);
            } 
            else if (current_state != kDeviceStateMusicPlaying && 
                     current_state != kDeviceStateListening && 
                     current_state != kDeviceStateSpeaking) {
                app.SetDeviceState(kDeviceStateMusicPlaying);
                
                if (!g_audio_output_managed_by_music) {
                    g_audio_output_managed_by_music = true;
                    
                    auto& board = Board::GetInstance();
                    auto codec = board.GetAudioCodec();
                    if (codec != nullptr) {
                        codec->EnableOutput(true);
                    }
                }
            }
            
            last_state_check_time = current_time;
        }
        
        if (!is_playing_ || audio_queue_ == nullptr) {
            break;
        }
        
        if (has_lyrics_ && (current_time - last_lyric_check_time > 500)) {
            try {
                UpdateLyrics();
            } catch(...) {
                ESP_LOGE(TAG, "更新歌词时发生异常");
                has_lyrics_ = false;
            }
            last_lyric_check_time = current_time;
        }
        
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    
    delete[] mp3_buffer;
    
    if (player_task_started && player_task_handle != nullptr) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    esp_http_client_cleanup(client);
    
    CleanupMp3Decoder();
    
    if (!is_playing_) {
        vTaskDelete(nullptr);
        return;
    }
    
    app.ReleaseAudioState(AUDIO_STATE_MUSIC);
    
    if (g_audio_output_managed_by_music) {
        g_audio_output_managed_by_music = false;
    }
    
    g_music_active = false;
    
    is_playing_ = false;
    
    vTaskDelay(pdMS_TO_TICKS(50));
    
    Application::GetInstance().SetDeviceState(kDeviceStateListening);
    
    vTaskDelete(nullptr);
}

bool MusicService::SendHttpRequest(const char* url, esp_http_client_method_t method, 
                                  const char* post_data, size_t post_len,
                                  char* response_buffer, size_t buffer_size, 
                                  int* response_len, const char* headers,
                                  bool light_logging) {
    if (!should_continue_search_ || g_music_interrupt_requested) {
        return false;
    }
    
    esp_http_client_config_t config = {};
    config.url = url;
    config.method = method;
    config.timeout_ms = 5000;
    config.buffer_size = 3072;
    config.buffer_size_tx = 2048;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.disable_auto_redirect = false;
    
    if (!should_continue_search_ || g_music_interrupt_requested) {
        return false;
    }
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        ESP_LOGE(TAG, "HTTP客户端初始化失败");
        return false;
    }
    
    active_http_client_ = client;
    
    esp_http_client_set_header(client, "Content-Type", headers);
    esp_http_client_set_header(client, "Accept", "application/json");
    esp_http_client_set_header(client, "User-Agent", "ESP32 Music Player");
    esp_http_client_set_header(client, "Accept-Charset", "utf-8");
    
    if (!should_continue_search_ || g_music_interrupt_requested) {
        esp_http_client_cleanup(client);
        active_http_client_ = nullptr;
        return false;
    }
    
    if (method == HTTP_METHOD_POST && post_data != nullptr && post_len > 0) {
        esp_http_client_set_post_field(client, post_data, post_len);
    }
    
    esp_err_t err;
    for (int retry = 0; retry < 2; retry++) {
        if (!should_continue_search_ || g_music_interrupt_requested) {
            esp_http_client_cleanup(client);
            active_http_client_ = nullptr;
            return false;
        }
        
        err = esp_http_client_open(client, method == HTTP_METHOD_POST ? post_len : 0);
        if (err == ESP_OK) {
            break;
        }
        
        if (!light_logging) {
            ESP_LOGW(TAG, "HTTP连接尝试%d失败: %s", retry+1, esp_err_to_name(err));
        }
        
        vTaskDelay(pdMS_TO_TICKS(retry == 0 ? 200 : 400));
        
        if (g_music_interrupt_requested) {
            esp_http_client_cleanup(client);
            active_http_client_ = nullptr;
            return false;
        }
    }
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP连接打开失败: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        active_http_client_ = nullptr;
        return false;
    }
    
    bool is_active = (active_http_client_ == client);
    
    if (!should_continue_search_ || !is_active) {
        esp_http_client_cleanup(client);
        if (active_http_client_ == client) {
            active_http_client_ = nullptr;
        }
        return false;
    }
    
    esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);
    
    if (!should_continue_search_ || active_http_client_ != client) {
        esp_http_client_cleanup(client);
        if (active_http_client_ == client) {
            active_http_client_ = nullptr;
        }
        return false;
    }
    
    *response_len = 0;
    if (response_buffer != nullptr && buffer_size > 0) {
        int bytes_read = 0;
        int total_read = 0;
        
        int max_retries = 2;
        int retry_count = 0;
        bool keep_reading = true;
        
        while (keep_reading && retry_count < max_retries) {
            if (!should_continue_search_ || g_music_interrupt_requested) {
                esp_http_client_cleanup(client);
                if (active_http_client_ == client) {
                    active_http_client_ = nullptr;
                }
                return false;
            }
            
            bytes_read = esp_http_client_read(client, 
                                             response_buffer + total_read, 
                                             buffer_size - total_read - 1);
            
            if (bytes_read > 0) {
                total_read += bytes_read;
                retry_count = 0;
            } else if (bytes_read == 0) {
                if (total_read > 0) {
                    vTaskDelay(pdMS_TO_TICKS(50));
                    retry_count++;
                    
                    if (retry_count >= 1) {
                        keep_reading = false;
                    }
                } else {
                    vTaskDelay(pdMS_TO_TICKS(200));
                    retry_count++;
                }
            } else {
                ESP_LOGE(TAG, "读取响应时发生错误: %d", bytes_read);
                keep_reading = false;
            }
            
            if (total_read >= buffer_size - 1) {
                if (!light_logging) {
                    ESP_LOGW(TAG, "响应缓冲区已满，停止读取");
                }
                keep_reading = false;
            }
        }
        
        *response_len = total_read;
        
        if (total_read < buffer_size) {
            response_buffer[total_read] = '\0';
        } else {
            response_buffer[buffer_size - 1] = '\0';
        }
    }
    
    esp_http_client_cleanup(client);
    
    if (active_http_client_ == client) {
        active_http_client_ = nullptr;
    }
    
    if (!should_continue_search_) {
        return false;
    }
    
    return (status_code == 200);
}

bool MusicService::InitMp3Decoder() {
    
    if (is_decoder_initialized_) {
        ESP_LOGW(TAG, "解码器已初始化，先清理资源");
        CleanupMp3Decoder();
    }
    
    esp_audio_dec_register_default();
    esp_audio_simple_dec_register_default();
    
    decode_input_buffer_size_ = 4096;
    decode_output_buffer_size_ = 4096 * 2;
    
    decode_input_buffer_ = (uint8_t*)malloc(decode_input_buffer_size_);
    decode_output_buffer_ = (uint8_t*)malloc(decode_output_buffer_size_);
    
    if (!decode_input_buffer_ || !decode_output_buffer_) {
        ESP_LOGE(TAG, "无法分配解码缓冲区");
        CleanupMp3Decoder();
        return false;
    }
    
    esp_audio_simple_dec_cfg_t dec_cfg = {
        .dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_MP3,
        .dec_cfg = NULL,
        .cfg_size = 0
    };
    
    esp_audio_err_t ret = esp_audio_simple_dec_open(&dec_cfg, &mp3_decoder_);
    if (ret != ESP_AUDIO_ERR_OK) {
        ESP_LOGE(TAG, "无法打开MP3解码器, 错误: %d", ret);
        CleanupMp3Decoder();
        return false;
    }
    
    is_decoder_initialized_ = true;
    return true;
}

void MusicService::CleanupMp3Decoder() {
    if (mp3_decoder_) {
        esp_audio_simple_dec_close(mp3_decoder_);
        mp3_decoder_ = NULL;
    }
    
    if (decode_input_buffer_) {
        free(decode_input_buffer_);
        decode_input_buffer_ = NULL;
    }
    
    if (decode_output_buffer_) {
        free(decode_output_buffer_);
        decode_output_buffer_ = NULL;
    }
    
    if (is_decoder_initialized_) {
        esp_audio_simple_dec_unregister_default();
        esp_audio_dec_unregister_default();
        
        is_decoder_initialized_ = false;
    }
}

bool MusicService::ProcessMp3Data(const uint8_t* mp3_data, int mp3_size, bool is_end_of_stream) {
    if (!is_decoder_initialized_ || !mp3_decoder_) {
        ESP_LOGE(TAG, "解码器未初始化");
        return false;
    }
    
    if (mp3_size <= 0 && !is_end_of_stream) {
        return true;
    }
    
    esp_audio_simple_dec_raw_t raw = {
        .buffer = (uint8_t*)mp3_data,
        .len = (uint32_t)mp3_size,
        .eos = is_end_of_stream,
        .consumed = 0
    };
    
    auto& board = Board::GetInstance();
    auto codec = board.GetAudioCodec();
    
    if (codec) {
        codec->EnableOutput(true);
    }
    
    static int speed_sample_counter = 0;
    
    static int16_t prev_samples[16] = {0};
    static int32_t freq_energy = 0;
    static bool high_freq_mode = false;
    static int silence_counter = 0;
    
    while (raw.len > 0 || is_end_of_stream) {
        esp_audio_simple_dec_out_t out_frame = {};
        out_frame.buffer = decode_output_buffer_;
        out_frame.len = (uint32_t)decode_output_buffer_size_;
        
        esp_audio_err_t ret = esp_audio_simple_dec_process(mp3_decoder_, &raw, &out_frame);
        
        if (is_end_of_stream && raw.len == 0) {
            break;
        }
        
        if (ret == ESP_AUDIO_ERR_BUFF_NOT_ENOUGH) {
            ESP_LOGW(TAG, "输出缓冲区不足，扩大缓冲区: %u -> %u", 
                    (unsigned int)decode_output_buffer_size_, (unsigned int)out_frame.needed_size);
            
            uint8_t* new_buffer = (uint8_t*)realloc(decode_output_buffer_, out_frame.needed_size);
            if (!new_buffer) {
                ESP_LOGE(TAG, "无法扩展输出缓冲区");
        return false;
        }
    
            decode_output_buffer_ = new_buffer;
            decode_output_buffer_size_ = out_frame.needed_size;
            continue;
        }
        
        if (ret != ESP_AUDIO_ERR_OK) {
            ESP_LOGE(TAG, "MP3解码失败: %d", ret);
            return false;
        }
        
        if (out_frame.decoded_size > 0) {
            if (sample_rate_ == 0) {
                esp_audio_simple_dec_info_t dec_info = {};
                esp_audio_simple_dec_get_info(mp3_decoder_, &dec_info);
                
                sample_rate_ = dec_info.sample_rate;
                bits_per_sample_ = dec_info.bits_per_sample;
                channels_ = dec_info.channel;
                
                actual_codec_sample_rate_ = sample_rate_;
                
                if (codec) {
                    codec->EnableOutput(true);
                }
            }
            
            const size_t total_samples = out_frame.decoded_size / 2;
            const int16_t* raw_samples = (int16_t*)out_frame.buffer;
            
            std::vector<int16_t> pcm_data;
            pcm_data.reserve(total_samples / 3 + 16);
            
            int32_t total_energy = 0;
            int32_t high_freq_energy = 0;
            int32_t mid_freq_energy = 0;
            const size_t analysis_window = std::min(total_samples, size_t(64));
            
            for (size_t i = 1; i < analysis_window; i++) {
                int32_t diff = abs(raw_samples[i] - raw_samples[i-1]);
                int32_t mid_diff = 0;
                if (i >= 3) {
                    mid_diff = abs(raw_samples[i] - raw_samples[i-3])/3;
                }
                
                total_energy += abs(raw_samples[i]);
                high_freq_energy += diff;
                mid_freq_energy += mid_diff;
            }
            
            if (total_energy > 0) {
                freq_energy = (freq_energy * 3 + (high_freq_energy * 100 / total_energy)) / 4;
                
                int32_t voice_ratio = 0;
                if (high_freq_energy > 0) {
                    voice_ratio = (mid_freq_energy * 100) / high_freq_energy;
                }
                
                if (freq_energy > 30) {
                    if (voice_ratio > 40) {
                        high_freq_mode = true;
                        if (speed_sample_counter % 100 == 0) {
                            ESP_LOGD(TAG, "检测到高频内容(含人声) (能量比: %d%%, 人声比: %d%%), 使用适中降噪", 
                                    (int)freq_energy, (int)voice_ratio);
                        }
        } else {
                        high_freq_mode = true;
                        if (speed_sample_counter % 100 == 0) {
                            ESP_LOGD(TAG, "检测到乐器噪音 (能量比: %d%%, 人声比: %d%%), 使用强化降噪", 
                                    (int)freq_energy, (int)voice_ratio);
                        }
                    }
                } else if (freq_energy < 20) {
                    high_freq_mode = false;
                }
                
                g_last_voice_ratio = (g_last_voice_ratio * 3 + voice_ratio) / 4;
            }
            
            bool is_silence = total_energy < 500 * analysis_window;
            if (is_silence) {
                silence_counter++;
                if (silence_counter > 5) {
                    freq_energy = 0;
                    high_freq_mode = false;
        }
    } else {
                silence_counter = 0;
            }
            
            for (size_t i = 0; i < total_samples; i += 7) {
                {
                    int32_t weighted_avg = 0;
                    int32_t weights_sum = 0;
                    
                    int hist_depth = high_freq_mode ? 12 : 6;
                    int current_weight_base = high_freq_mode ? 6 : 10;
                    
                    for (int j = 0; j < hist_depth; j++) {
                        int idx = (j < 8) ? j : (15 - (j - 8));
                        if (idx < 16) {
                            int weight = high_freq_mode ? 
                                          (hist_depth - j) / 2 + 1 :
                                          (hist_depth - j) / 3 + 1;
                            
                            weighted_avg += prev_samples[idx] * weight;
                            weights_sum += weight;
                        }
                    }
                    
                    for (int j = 0; j < 3 && (i + j) < total_samples; j++) {
                        int weight = current_weight_base - j * (high_freq_mode ? 1 : 2);
                        
                        weighted_avg += raw_samples[i + j] * weight;
                        weights_sum += weight;
                        
                        for (int k = 15; k > 0; k--) {
                            prev_samples[k] = prev_samples[k-1];
                        }
                        prev_samples[0] = raw_samples[i + j];
                    }
                    
                    if (weights_sum > 0) {
                        weighted_avg /= weights_sum;
                        
                        if (weighted_avg > 32767) weighted_avg = 32767;
                        if (weighted_avg < -32768) weighted_avg = -32768;
                        
                        if (!pcm_data.empty() && high_freq_mode) {
                            int16_t last_output = pcm_data.back();
                            int voice_factor = static_cast<int>(70 - g_last_voice_ratio/2);
                            int smoothing_factor = std::min(70, (voice_factor > 30 ? voice_factor : 30));
                            weighted_avg = (weighted_avg * smoothing_factor + last_output * (100 - smoothing_factor)) / 100;
                        }
                        
                        pcm_data.push_back((int16_t)weighted_avg);
                    }
                }
                
                if (i + 3 < total_samples) {
                    int32_t weighted_avg = 0;
                    int32_t weights_sum = 0;
                    
                    int hist_depth = high_freq_mode ? 6 : 3;
                    
                    int current_weight_base = 10;
                    if (high_freq_mode) {
                        current_weight_base = 7 + g_last_voice_ratio / 12;
                    }
                    
                    for (int j = 0; j < hist_depth; j++) {
                        int idx = (j < 8) ? j : (15 - (j - 8));
                        if (idx < 16) {
                            int weight = high_freq_mode ? 
                                          (hist_depth - j) / 2 + 1 :
                                          (hist_depth - j) / 3 + 1;
                            
                            weighted_avg += prev_samples[idx] * weight;
                            weights_sum += weight;
                        }
                    }
                    
                    for (int j = 3; j < 7 && (i + j) < total_samples; j++) {
                        int weight = current_weight_base - (j - 3) * (high_freq_mode ? 1 : 2);
                        
                        weighted_avg += raw_samples[i + j] * weight;
                        weights_sum += weight;
                        
                        for (int k = 15; k > 0; k--) {
                            prev_samples[k] = prev_samples[k-1];
                        }
                        prev_samples[0] = raw_samples[i + j];
                    }
                    
                    if (weights_sum > 0) {
                        weighted_avg /= weights_sum;
                        
                        if (weighted_avg > 32767) weighted_avg = 32767;
                        if (weighted_avg < -32768) weighted_avg = -32768;
                        
                        if (!pcm_data.empty() && high_freq_mode) {
                            int16_t last_output = pcm_data.back();
                            int voice_factor = static_cast<int>(70 - g_last_voice_ratio/2);
                            int smoothing_factor = std::min(70, (voice_factor > 30 ? voice_factor : 30));
                            weighted_avg = (weighted_avg * smoothing_factor + last_output * (100 - smoothing_factor)) / 100;
                        }
                        
                        pcm_data.push_back((int16_t)weighted_avg);
                    }
                }
                
                speed_sample_counter++;
            }
            
            if (codec && !pcm_data.empty()) {
                codec->OutputData(pcm_data);
                ESP_LOGD(TAG, "解码MP3数据: 原始%u样本, 3.6倍速(增强降噪后)输出%zu样本", 
                        (unsigned int)total_samples, pcm_data.size());
            } else if (codec == nullptr) {
                ESP_LOGE(TAG, "找不到解码器，无法输出PCM数据");
        return false;
            }
        }
        
        raw.buffer += raw.consumed;
        raw.len -= raw.consumed;
    }
    
    return true;
}

bool MusicService::FetchLyrics(int song_id) {
    lyrics_.clear();
    has_lyrics_ = false;
    current_lyric_index_ = 0;
    
    if (song_id <= 0) {
        ESP_LOGE(TAG, "歌曲ID无效");
        return false;
    }
    
    char* lyrics_url = (char*)heap_caps_malloc(256, MALLOC_CAP_8BIT);
    if (!lyrics_url) {
        ESP_LOGE(TAG, "无法分配歌词URL缓冲区");
        return false;
    }
    
    snprintf(lyrics_url, 256, 
             "https://music-api.gdstudio.xyz/api.php?types=lyric&source=kuwo&id=%d", 
             song_id);
    
    char* response_buffer = (char*)heap_caps_malloc(HTTP_RESPONSE_BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!response_buffer) {
        ESP_LOGE(TAG, "无法分配歌词响应缓冲区");
        heap_caps_free(lyrics_url);
        return false;
    }
    
    memset(response_buffer, 0, HTTP_RESPONSE_BUFFER_SIZE);
    int response_len = 0;
    
    try {
        bool http_success = SendHttpRequest(lyrics_url, HTTP_METHOD_GET,
                                          nullptr, 0,
                                          response_buffer, HTTP_RESPONSE_BUFFER_SIZE,
                                          &response_len, "application/json; charset=UTF-8",
                                          false);
        
        if (!http_success) {
            ESP_LOGE(TAG, "获取歌词失败");
            if (response_len > 0) {
                ESP_LOGE(TAG, "API错误响应: %s", response_buffer);
            }
            heap_caps_free(response_buffer);
            heap_caps_free(lyrics_url);
        return false;
    }
    
        if (response_len <= 0) {
            ESP_LOGE(TAG, "歌词API返回空响应");
            heap_caps_free(response_buffer);
            heap_caps_free(lyrics_url);
        return false;
    }
    
        if (response_len < HTTP_RESPONSE_BUFFER_SIZE) {
            response_buffer[response_len] = '\0';
        } else {
            response_buffer[HTTP_RESPONSE_BUFFER_SIZE - 1] = '\0';
            ESP_LOGW(TAG, "歌词响应被截断");
        }
        
        if (response_buffer[0] != '{' && response_buffer[0] != '[') {
            ESP_LOGE(TAG, "歌词响应不是有效的JSON格式: %.32s...", response_buffer);
            heap_caps_free(response_buffer);
            heap_caps_free(lyrics_url);
        return false;
    }
    
        cJSON *root = cJSON_Parse(response_buffer);
        if (root == NULL) {
            ESP_LOGE(TAG, "解析歌词JSON失败");
            heap_caps_free(response_buffer);
            heap_caps_free(lyrics_url);
            return false;
        }
        
        std::unique_ptr<cJSON, decltype(&cJSON_Delete)> json_guard(root, cJSON_Delete);
        
        cJSON *lyric = cJSON_GetObjectItem(root, "lyric");
        if (!lyric || !cJSON_IsString(lyric) || strlen(lyric->valuestring) == 0) {
            lyric = cJSON_GetObjectItem(root, "lrc");
            if (!lyric || !cJSON_IsString(lyric) || strlen(lyric->valuestring) == 0) {
                ESP_LOGE(TAG, "未找到有效的歌词内容");
                heap_caps_free(response_buffer);
                heap_caps_free(lyrics_url);
        return false;
            }
        }
        
        const char* lrc_text = lyric->valuestring;
        
        bool parse_success = ParseLyrics(lrc_text);
        if (!parse_success) {
            ESP_LOGE(TAG, "解析LRC歌词失败");
        } else {
            has_lyrics_ = lyrics_.size() > 0;
        }
        
        heap_caps_free(response_buffer);
        heap_caps_free(lyrics_url);
        
        return parse_success;
        
    } catch (std::exception& e) {
        ESP_LOGE(TAG, "歌词获取过程中发生异常: %s", e.what());
        heap_caps_free(response_buffer);
        heap_caps_free(lyrics_url);
        return false;
    } catch (...) {
        ESP_LOGE(TAG, "歌词获取发生异常");
        heap_caps_free(response_buffer);
        heap_caps_free(lyrics_url);
        return false;
    }
}

bool MusicService::ParseLyrics(const char* lrc_text) {
    if (!lrc_text || strlen(lrc_text) == 0) {
        ESP_LOGE(TAG, "歌词文本为空");
        return false;
    }
    
    lyrics_.clear();
    current_lyric_index_ = 0;
    
    try {
        size_t text_len = strlen(lrc_text);
        char* buffer = (char*)heap_caps_malloc(text_len + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!buffer) {
            ESP_LOGE(TAG, "无法分配歌词缓冲区内存");
        return false;
    }
    
        memcpy(buffer, lrc_text, text_len);
        buffer[text_len] = '\0';
        
        char* line = strtok(buffer, "\n");
        while (line != nullptr) {
            if (strstr(line, "[") != nullptr) {
                char* bracket_start = strchr(line, '[');
                if (bracket_start != nullptr) {
                    char* bracket_end = strchr(bracket_start, ']');
                    if (bracket_end != nullptr) {
                        *bracket_end = '\0';
                        const char* timestamp = bracket_start + 1;
                        
                        const char* lyric_text = bracket_end + 1;
                        while (*lyric_text == ' ' || *lyric_text == '\t') lyric_text++;
                        
                        if (strstr(timestamp, ":") != nullptr) {
                            int milliseconds = 0;
                            if (ConvertTimestampToMilliseconds(timestamp, &milliseconds)) {
                                lyrics_.push_back(std::make_pair(milliseconds, lyric_text));
                            }
                        }
                    }
                }
            }
            
            line = strtok(nullptr, "\n");
        }
        
        heap_caps_free(buffer);
        
        std::sort(lyrics_.begin(), lyrics_.end(), 
                 [](const std::pair<int, std::string>& a, const std::pair<int, std::string>& b) {
                     return a.first < b.first;
                 });
        
        has_lyrics_ = !lyrics_.empty();
        
        return true;
    } catch (std::exception& e) {
        ESP_LOGE(TAG, "解析歌词时发生异常: %s", e.what());
        return false;
    } catch (...) {
        ESP_LOGE(TAG, "解析歌词时发生未知异常");
        return false;
    }
}

bool MusicService::ConvertTimestampToMilliseconds(const std::string& timestamp, int* milliseconds) {
    if (!milliseconds) {
        return false;
    }
    
    int minutes = 0;
    int seconds = 0;
    int ms = 0;
    
    int matched = sscanf(timestamp.c_str(), "%d:%d.%d", &minutes, &seconds, &ms);
    
    if (matched < 2) {
        return false;
    }
    
    *milliseconds = minutes * 60 * 1000 + seconds * 1000 + ms;
    
    return true;
}

void MusicService::UpdateLyrics() {
    if (!has_lyrics_ || lyrics_.empty()) {
        return;
    }
    
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint32_t elapsed_time = current_time - playback_start_time_;
    
    if (current_lyric_index_ >= lyrics_.size()) {
        current_lyric_index_ = 0;
    }
    
    size_t next_index = current_lyric_index_;
    
    while (next_index < lyrics_.size() && 
           lyrics_[next_index].first < elapsed_time) {
        next_index++;
    }
    
    if (next_index > 0 && next_index != current_lyric_index_) {
        next_index--;
        if (next_index != current_lyric_index_) {
            current_lyric_index_ = next_index;
            char* lyric_buffer = (char*)heap_caps_malloc(256, MALLOC_CAP_8BIT);
            if (lyric_buffer) {
                snprintf(lyric_buffer, 256, "%s", lyrics_[current_lyric_index_].second.c_str());
                DisplayLyric(lyric_buffer);
                heap_caps_free(lyric_buffer);
    } else {
                DisplayLyric(lyrics_[current_lyric_index_].second);
            }
        }
    }
}

void MusicService::DisplayLyric(const std::string& lyric) {
    if (lyric.empty()) {
        return;
    }
    
    printf("%s\n", lyric.c_str());
    
    auto& board = Board::GetInstance();
    auto* display = board.GetDisplay();
    if (!display) {
        return;
    }
    
    char* display_buffer = (char*)heap_caps_malloc(300, MALLOC_CAP_8BIT);
    if (!display_buffer) {
        display->SetChatMessage("assistant", lyric.c_str());
        return;
    }
    
    snprintf(display_buffer, 300, "%s", lyric.c_str());
    
    display->SetChatMessage("assistant", display_buffer);
    
    heap_caps_free(display_buffer);
}

void MusicService::CancelSearch() {
    should_continue_search_ = false;
    
    if (is_playing_) {
        g_music_active = false;
    }
    
    song_id_ = 0;
    current_song_name_ = "";
    current_artist_ = "";
    url_to_play_ = "";
}

void MusicService::AbortHttpRequest() {
    should_continue_search_ = false;
    
    if (active_http_client_ != nullptr) {
        active_http_client_ = nullptr;
    }
}

void MusicService::AudioPlayerTask(void* arg) {
    MusicService* service = static_cast<MusicService*>(arg);
    
    vTaskPrioritySet(NULL, 15);
    
    auto& board = Board::GetInstance();
    auto codec = board.GetAudioCodec();
    
    const float CORRECTED_RATIO = 1.5f;
    
    int sample_counter = 0;
    
    std::vector<int16_t> pcm_output_buffer;
    pcm_output_buffer.reserve(AUDIO_BUFFER_SIZE * 8);
    
    uint32_t last_log_time = 0;
    
    int empty_queue_count = 0;
    const int MAX_EMPTY_QUEUE_COUNT = 3;
    
    uint32_t cumulative_sample_count = 0;
    uint32_t performance_check_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    bool buffer_low_warning = false;
    uint32_t buffer_status_check_time = 0;
    
    while (service->is_playing_ && service->audio_queue_ != nullptr) {
        if (g_music_interrupt_requested) {
            service->is_playing_ = false;
            break;
        }
        
        if (!service->is_playing_ || service->audio_queue_ == nullptr) {
            break;
        }
        
        UBaseType_t queue_size = uxQueueMessagesWaiting(service->audio_queue_);
        
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (current_time - buffer_status_check_time > 1000) {
            buffer_status_check_time = current_time;
            
            if (queue_size < BUFFER_LOW_THRESHOLD && !buffer_low_warning) {
                ESP_LOGW(TAG, "播放缓冲区过低(%d)，可能导致卡顿", queue_size);
                buffer_low_warning = true;
            } else if (queue_size > BUFFER_LOW_THRESHOLD && buffer_low_warning) {
                buffer_low_warning = false;
            }
            
            uint32_t elapsed_ms = current_time - performance_check_time;
            if (elapsed_ms > 5000) {
                float samples_per_second = (float)cumulative_sample_count * 1000 / elapsed_ms;
                
                cumulative_sample_count = 0;
                performance_check_time = current_time;
            }
        }
        
        if (!service->is_playing_ || service->audio_queue_ == nullptr) {
            break;
        }
        
        std::vector<uint8_t>* buffer = nullptr;
        if (xQueueReceive(service->audio_queue_, &buffer, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (buffer != nullptr) {
                empty_queue_count = 0;
                
                const int16_t* pcm_data = reinterpret_cast<const int16_t*>(buffer->data());
                const size_t sample_count = buffer->size() / sizeof(int16_t);
                
                pcm_output_buffer.clear();
                
                cumulative_sample_count += sample_count;
                
                if (!service->is_playing_ || service->audio_queue_ == nullptr) {
                    delete buffer;
                    break;
                }
                
                if (!pcm_output_buffer.empty()) {
                    if (queue_size < BUFFER_LOW_THRESHOLD) {
                        vTaskDelay(pdMS_TO_TICKS(5));
                    }
                    
                    codec->OutputData(pcm_output_buffer);
                }
                
                delete buffer;
            }
        } else {
            empty_queue_count++;
            
            if (!service->is_playing_ || service->audio_queue_ == nullptr) {
                break;
            }
            
            if (empty_queue_count >= MAX_EMPTY_QUEUE_COUNT && 
                service->audio_queue_ != nullptr &&
                uxQueueMessagesWaiting(service->audio_queue_) == 0 && 
                !service->is_playing_) {
                break;
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    vTaskDelete(nullptr);
}
    