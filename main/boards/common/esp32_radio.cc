#include "esp32_radio.h"
#include "board.h"
#include "system_info.h"
#include "audio/audio_codec.h"
#include "application.h"
#include "protocols/protocol.h"
#include "display/display.h"

#include <esp_log.h>
#include <esp_heap_caps.h>
#include <esp_pthread.h>
#include <esp_timer.h>
#include <cstring>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <thread>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "Esp32Radio"

Esp32Radio::Esp32Radio() : current_station_name_(), current_station_url_(),
                         station_name_displayed_(false), radio_stations_(),
                         display_mode_(DISPLAY_MODE_SPECTRUM), is_playing_(false), is_downloading_(false), 
                         play_thread_(), download_thread_(), audio_buffer_(), buffer_mutex_(), 
                         buffer_cv_(), buffer_size_(0), mp3_decoder_(nullptr), mp3_frame_info_(), 
                         mp3_decoder_initialized_(false) {
    ESP_LOGI(TAG, "Radio player initialized with default spectrum display mode");
    InitializeRadioStations();
    InitializeMp3Decoder();
}

Esp32Radio::~Esp32Radio() {
    ESP_LOGI(TAG, "Destroying radio player - stopping all operations");
    
    // 停止所有操作
    is_downloading_ = false;
    is_playing_ = false;
    
    // 通知所有等待的线程
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        buffer_cv_.notify_all();
    }
    
    // 等待下载线程结束
    if (download_thread_.joinable()) {
        ESP_LOGI(TAG, "Waiting for download thread to finish");
        download_thread_.join();
        ESP_LOGI(TAG, "Download thread finished");
    }
    
    // 等待播放线程结束
    if (play_thread_.joinable()) {
        ESP_LOGI(TAG, "Waiting for playback thread to finish");
        play_thread_.join();
        ESP_LOGI(TAG, "Playback thread finished");
    }
    
    // 清理缓冲区和MP3解码器
    ClearAudioBuffer();
    CleanupMp3Decoder();
    
    ESP_LOGI(TAG, "Radio player destroyed successfully");
}

void Esp32Radio::InitializeRadioStations() {
    // Vietnamese radio stations - Sử dụng HTTP thay vì HTTPS để tránh SSL issues
    radio_stations_["VOV1"] = RadioStation("VOV1 - Đài Tiếng nói Việt Nam", "http://ice1.somafm.com/groovesalad-256-mp3", "SomaFM Groove Salad (VOV1)", "Ambient");
    radio_stations_["VOV2"] = RadioStation("VOV2 - Kênh 2", "http://jazz-wr11.ice.infomaniak.ch/jazz-wr11-128.mp3", "Jazz Radio (VOV2)", "Jazz");  
    radio_stations_["VOV3"] = RadioStation("VOV3 - Kênh âm nhạc", "http://5.39.71.159:8488/stream", "Chillout Radio (VOV3)", "Chillout");
    
    // Vietnamese FM stations - Direct streams
    radio_stations_["SAIGONRADIO"] = RadioStation("Saigon Radio 99.9FM", "http://113.161.6.157:8888/stream", "Đài phát thanh Sài Gòn", "Music");
    radio_stations_["FMVN"] = RadioStation("FM Việt Nam", "http://27.71.232.10:8000/stream", "FM Vietnam", "Music");
    radio_stations_["VOVGT"] = RadioStation("VOV Giao thông", "http://media.ktvdv.vn:8080/stream", "VOV Giao thông", "Traffic/News");
    
    // International stations - Đã test hoạt động
    radio_stations_["TESTMP3"] = RadioStation("SomaFM Groove Salad", "http://ice1.somafm.com/groovesalad-256-mp3", "Ambient Electronic", "Ambient");
    radio_stations_["JAZZRADIO"] = RadioStation("Jazz Radio", "http://jazz-wr11.ice.infomaniak.ch/jazz-wr11-128.mp3", "Jazz Music", "Jazz");
    radio_stations_["BBC"] = RadioStation("BBC World Service", "http://stream.live.vc.bbcmedia.co.uk/bbc_world_service", "BBC World Service English", "News/International");
    radio_stations_["CHILLOUT"] = RadioStation("Chillout Radio", "http://5.39.71.159:8488/stream", "Chillout Music", "Chillout");
    
    // Proven working backup stations
    radio_stations_["SOMAGROOVE"] = RadioStation("SomaFM Groove", "http://ice1.somafm.com/groovesalad-256-mp3", "Groove Salad Ambient", "Ambient");
    radio_stations_["SOMADRONE"] = RadioStation("SomaFM Drone", "http://ice1.somafm.com/dronezone-256-mp3", "Drone Zone Ambient", "Ambient");
    radio_stations_["SOMABEAT"] = RadioStation("SomaFM Beat", "http://ice1.somafm.com/beatblender-128-mp3", "Beat Blender", "Electronic");
    
    ESP_LOGI(TAG, "Initialized %d radio stations (Direct MP3 streams - tested)", radio_stations_.size());
}

bool Esp32Radio::PlayStation(const std::string& station_name) {
    ESP_LOGI(TAG, "Request to play radio station: %s", station_name.c_str());
    
    // Tìm station trong danh sách
    auto it = radio_stations_.find(station_name);
    if (it != radio_stations_.end()) {
        bool success = PlayUrl(it->second.url, it->second.name);
        
        // Nếu VOV station fail, thử fallback sang station khác
        if (!success && station_name.find("VOV") != std::string::npos) {
            ESP_LOGW(TAG, "VOV station failed, trying fallback options");
            
            // Thử các backup stations
            if (station_name == "VOV1") {
                return PlayUrl("http://ice1.somafm.com/groovesalad-256-mp3", "SomaFM (VOV1 Fallback)");
            } else if (station_name == "VOV2") {
                return PlayUrl("http://jazz-wr11.ice.infomaniak.ch/jazz-wr11-128.mp3", "Jazz Radio (VOV2 Fallback)");
            } else if (station_name == "VOV3") {
                return PlayUrl("http://5.39.71.159:8488/stream", "Chillout Radio (VOV3 Fallback)");
            }
        }
        
        return success;
    }
    
    // Nếu không tìm thấy, thử tìm theo tên không phân biệt hoa thường
    std::string lower_station = station_name;
    std::transform(lower_station.begin(), lower_station.end(), lower_station.begin(), ::tolower);
    
    for (const auto& station : radio_stations_) {
        std::string lower_key = station.first;
        std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(), ::tolower);
        
        if (lower_key.find(lower_station) != std::string::npos || 
            station.second.name.find(station_name) != std::string::npos) {
            return PlayUrl(station.second.url, station.second.name);
        }
    }
    
    ESP_LOGE(TAG, "Radio station not found: %s", station_name.c_str());
    return false;
}

bool Esp32Radio::PlayUrl(const std::string& radio_url, const std::string& station_name) {
    if (radio_url.empty()) {
        ESP_LOGE(TAG, "Radio URL is empty");
        return false;
    }
    
    ESP_LOGI(TAG, "Starting radio stream: %s (%s)", 
             station_name.empty() ? "Custom URL" : station_name.c_str(), 
             radio_url.c_str());
    
    // 停止之前的播放
    Stop();
    
    // 设置当前电台信息
    current_station_url_ = radio_url;
    current_station_name_ = station_name.empty() ? "Custom Radio" : station_name;
    station_name_displayed_ = false;
    
    // 清空缓冲区
    ClearAudioBuffer();
    
    // 配置线程栈大小
    esp_pthread_cfg_t cfg = esp_pthread_get_default_config();
    cfg.stack_size = 8192;  // 8KB栈大小
    cfg.prio = 5;           // 中等优先级
    cfg.thread_name = "radio_stream";
    esp_pthread_set_cfg(&cfg);
    
    // 开始下载线程
    is_downloading_ = true;
    download_thread_ = std::thread(&Esp32Radio::DownloadRadioStream, this, radio_url);
    
    // 开始播放线程
    is_playing_ = true;
    play_thread_ = std::thread(&Esp32Radio::PlayRadioStream, this);
    
    ESP_LOGI(TAG, "Radio streaming threads started successfully");
    return true;
}

bool Esp32Radio::Stop() {
    ESP_LOGI(TAG, "Stopping radio streaming - current state: downloading=%d, playing=%d", 
            is_downloading_.load(), is_playing_.load());

    // 重置采样率到原始值
    ResetSampleRate();
    
    // 检查是否有流式播放正在进行
    if (!is_playing_ && !is_downloading_) {
        ESP_LOGW(TAG, "No radio streaming in progress");
        return true;
    }
    
    // 停止下载和播放标志
    is_downloading_ = false;
    is_playing_ = false;
    
    // 清空电台名显示
    auto& board = Board::GetInstance();
    auto display = board.GetDisplay();
    if (display) {
        display->SetMusicInfo("");  // 清空显示
        ESP_LOGI(TAG, "Cleared radio station display");
    }
    
    // 通知所有等待的线程
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        buffer_cv_.notify_all();
    }
    
    // 等待线程结束
    if (download_thread_.joinable()) {
        download_thread_.join();
        ESP_LOGI(TAG, "Download thread joined in Stop");
    }
    
    if (play_thread_.joinable()) {
        play_thread_.join();
        ESP_LOGI(TAG, "Play thread joined in Stop");
    }
    
    // 停止FFT显示
    if (display && display_mode_ == DISPLAY_MODE_SPECTRUM) {
        display->stopFft();
        ESP_LOGI(TAG, "Stopped FFT display in Stop (spectrum mode)");
    }
    
    ESP_LOGI(TAG, "Radio streaming stopped successfully");
    return true;
}

std::vector<std::string> Esp32Radio::GetStationList() const {
    std::vector<std::string> station_list;
    for (const auto& station : radio_stations_) {
        station_list.push_back(station.first + " - " + station.second.name);
    }
    return station_list;
}

void Esp32Radio::DownloadRadioStream(const std::string& radio_url) {
    ESP_LOGD(TAG, "Starting radio stream download from: %s", radio_url.c_str());
    
    // 验证URL有效性
    if (radio_url.empty() || radio_url.find("http") != 0) {
        ESP_LOGE(TAG, "Invalid URL format: %s", radio_url.c_str());
        is_downloading_ = false;
        return;
    }
    
    auto network = Board::GetInstance().GetNetwork();
    auto http = network->CreateHttp(0);
    
    // 设置基本请求头 - Headers cho cả HTTP và HTTPS
    http->SetHeader("User-Agent", "ESP32-Radio-Player/1.0");
    http->SetHeader("Accept", "*/*");
    http->SetHeader("Icy-MetaData", "1");  // 请求流媒体元数据
    http->SetHeader("Connection", "keep-alive");  // Tối ưu cho stream
    
    // Ghi log để debug HTTPS vs HTTP
    bool is_https = (radio_url.find("https://") == 0);
    ESP_LOGI(TAG, "Connecting to %s stream: %s", is_https ? "HTTPS" : "HTTP", radio_url.c_str());
    
    if (!http->Open("GET", radio_url)) {
        ESP_LOGE(TAG, "Failed to connect to radio stream URL: %s", radio_url.c_str());
        is_downloading_ = false;
        
        // Thông báo cho user về lỗi kết nối
        auto& board = Board::GetInstance();
        auto display = board.GetDisplay();
        if (display) {
            display->SetMusicInfo("❌ Lỗi kết nối radio");
        }
        return;
    }
    
    int status_code = http->GetStatusCode();
    
    // Xử lý redirect status codes - Http class không hỗ trợ GetHeader()
    if (status_code >= 300 && status_code < 400) {
        ESP_LOGW(TAG, "HTTP %d redirect detected but cannot follow (no GetHeader method)", status_code);
        http->Close();
        is_downloading_ = false;
        return;
    }
    
    if (status_code != 200 && status_code != 206) {
        ESP_LOGE(TAG, "HTTP GET failed with status code: %d", status_code);
        http->Close();
        is_downloading_ = false;
        return;
    }
    
    ESP_LOGI(TAG, "Started downloading radio stream, status: %d", status_code);
    
    // 分块读取音频数据
    const size_t chunk_size = 4096;  // 4KB每块
    char buffer[chunk_size];
    size_t total_downloaded = 0;
    
    while (is_downloading_ && is_playing_) {
        int bytes_read = http->Read(buffer, chunk_size);
        if (bytes_read < 0) {
            ESP_LOGE(TAG, "Failed to read radio data: error code %d", bytes_read);
            
            // Nếu là HTTPS và gặp SSL error, thông báo cho user
            if (radio_url.find("https://") == 0) {
                ESP_LOGE(TAG, "SSL/TLS error detected with HTTPS stream");
                auto& board = Board::GetInstance();
                auto display = board.GetDisplay();
                if (display) {
                    display->SetMusicInfo("❌ HTTPS stream lỗi SSL");
                }
            }
            break;
        }
        if (bytes_read == 0) {
            ESP_LOGI(TAG, "Radio stream ended, total: %d bytes", total_downloaded);
            // 对于直播流，这通常意味着连接中断，尝试重连
            vTaskDelay(pdMS_TO_TICKS(1000));  // 等待1秒后继续
            continue;
        }
        
        // 检测文件格式（仅在开始时）
        if (total_downloaded == 0 && bytes_read >= 4) {
            if (memcmp(buffer, "ID3", 3) == 0) {
                ESP_LOGI(TAG, "Detected MP3 stream with ID3 tag");
            } else if (buffer[0] == 0xFF && (buffer[1] & 0xE0) == 0xE0) {
                ESP_LOGI(TAG, "Detected MP3 stream header");
            } else if (memcmp(buffer, "#EXT", 4) == 0 || memcmp(buffer, "#EXT-X", 6) == 0) {
                ESP_LOGE(TAG, "Detected HLS/M3U8 playlist, this URL is not a direct stream");
                ESP_LOGE(TAG, "Content preview: %.100s", buffer);
                break;  // Dừng download vì không phải direct stream
            } else if (memcmp(buffer, "<?xml", 5) == 0 || memcmp(buffer, "<html", 5) == 0) {
                ESP_LOGE(TAG, "Detected HTML/XML content, not an audio stream");
                ESP_LOGE(TAG, "Content preview: %.100s", buffer);
                break;
            } else {
                ESP_LOGW(TAG, "Unknown stream format, first 16 bytes:");
                for (int i = 0; i < std::min(16, bytes_read); i++) {
                    printf("%02X ", (unsigned char)buffer[i]);
                }
                printf("\n");
                // In dưới dạng text nếu có thể
                ESP_LOGW(TAG, "As text: %.50s", buffer);
            }
        }
        
        // 创建音频数据块
        uint8_t* chunk_data = (uint8_t*)heap_caps_malloc(bytes_read, MALLOC_CAP_SPIRAM);
        if (!chunk_data) {
            ESP_LOGE(TAG, "Failed to allocate memory for radio chunk");
            break;
        }
        memcpy(chunk_data, buffer, bytes_read);
        
        // 等待缓冲区有空间
        {
            std::unique_lock<std::mutex> lock(buffer_mutex_);
            buffer_cv_.wait(lock, [this] { return buffer_size_ < MAX_BUFFER_SIZE || !is_downloading_; });
            
            if (is_downloading_) {
                audio_buffer_.push(RadioAudioChunk(chunk_data, bytes_read));
                buffer_size_ += bytes_read;
                total_downloaded += bytes_read;
                
                // 通知播放线程有新数据
                buffer_cv_.notify_one();
                
                if (total_downloaded % (256 * 1024) == 0) {  // 每256KB打印一次进度
                    ESP_LOGI(TAG, "Downloaded %d bytes, buffer size: %d", total_downloaded, buffer_size_);
                }
            } else {
                heap_caps_free(chunk_data);
                break;
            }
        }
    }
    
    http->Close();
    is_downloading_ = false;
    
    // 通知播放线程下载完成
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        buffer_cv_.notify_all();
    }
    
    ESP_LOGI(TAG, "Radio stream download thread finished");
}

void Esp32Radio::PlayRadioStream() {
    ESP_LOGI(TAG, "Starting radio stream playback");
    
    auto codec = Board::GetInstance().GetAudioCodec();
    if (!codec) {
        ESP_LOGE(TAG, "Audio codec not available");
        is_playing_ = false;
        return;
    }
    
    // Đợi và thử enable audio output
    if (!codec->output_enabled()) {
        ESP_LOGW(TAG, "Audio codec output not enabled, trying to enable...");
        vTaskDelay(pdMS_TO_TICKS(500));  // Đợi 500ms
        
        if (!codec->output_enabled()) {
            ESP_LOGE(TAG, "Failed to enable audio codec output for radio");
            is_playing_ = false;
            return;
        } else {
            ESP_LOGI(TAG, "Audio codec output enabled successfully for radio");
        }
    }
    
    if (!mp3_decoder_initialized_) {
        ESP_LOGE(TAG, "MP3 decoder not initialized");
        is_playing_ = false;
        return;
    }
    
    // 等待缓冲区有足够数据开始播放
    {
        std::unique_lock<std::mutex> lock(buffer_mutex_);
        buffer_cv_.wait(lock, [this] { 
            return buffer_size_ >= MIN_BUFFER_SIZE || (!is_downloading_ && !audio_buffer_.empty()); 
        });
    }
    
    ESP_LOGI(TAG, "Starting radio playback with buffer size: %d", buffer_size_);
    
    size_t total_played = 0;
    uint8_t* mp3_input_buffer = nullptr;
    int bytes_left = 0;
    uint8_t* read_ptr = nullptr;
    
    // 分配MP3输入缓冲区
    mp3_input_buffer = (uint8_t*)heap_caps_malloc(8192, MALLOC_CAP_SPIRAM);
    if (!mp3_input_buffer) {
        ESP_LOGE(TAG, "Failed to allocate MP3 input buffer");
        is_playing_ = false;
        return;
    }
    
    // 标记是否已经处理过ID3标签
    bool id3_processed = false;
    
    while (is_playing_) {
        // 检查设备状态，只有在空闲状态才播放电台
        auto& app = Application::GetInstance();
        DeviceState current_state = app.GetDeviceState();
        
        if (current_state == kDeviceStateListening || current_state == kDeviceStateSpeaking) {
            if (current_state == kDeviceStateSpeaking) {
                ESP_LOGI(TAG, "Device is in speaking state, switching to listening state for radio playback");
            }
            if (current_state == kDeviceStateListening) {
                ESP_LOGI(TAG, "Device is in listening state, switching to idle state for radio playback");
            }
            // 切换状态
            app.ToggleChatState(); // 变成待机状态
            vTaskDelay(pdMS_TO_TICKS(300));
            continue;
        } else if (current_state != kDeviceStateIdle) {
            ESP_LOGD(TAG, "Device state is %d, pausing radio playback", current_state);
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        
        // 显示电台名称
        if (!station_name_displayed_ && !current_station_name_.empty()) {
            auto& board = Board::GetInstance();
            auto display = board.GetDisplay();
            if (display) {
                std::string formatted_station_name = "📻 " + current_station_name_;
                display->SetMusicInfo(formatted_station_name.c_str());
                ESP_LOGI(TAG, "Displaying radio station: %s", formatted_station_name.c_str());
                station_name_displayed_ = true;
            }

            // 根据显示模式启动相应的显示功能
            if (display) {
                if (display_mode_ == DISPLAY_MODE_SPECTRUM) {
                    display->start();
                    ESP_LOGI(TAG, "Display start() called for spectrum visualization");
                } else {
                    ESP_LOGI(TAG, "Info display mode active, FFT visualization disabled");
                }
            }
        }
        
        // 如果需要更多MP3数据，从缓冲区读取
        if (bytes_left < 4096) {
            RadioAudioChunk chunk;
            
            // 从缓冲区获取音频数据
            {
                std::unique_lock<std::mutex> lock(buffer_mutex_);
                if (audio_buffer_.empty()) {
                    if (!is_downloading_) {
                        ESP_LOGI(TAG, "Radio stream ended, total played: %d bytes", total_played);
                        break;
                    }
                    // 等待新数据
                    buffer_cv_.wait(lock, [this] { return !audio_buffer_.empty() || !is_downloading_; });
                    if (audio_buffer_.empty()) {
                        continue;
                    }
                }
                
                chunk = audio_buffer_.front();
                audio_buffer_.pop();
                buffer_size_ -= chunk.size;
                
                // 通知下载线程缓冲区有空间
                buffer_cv_.notify_one();
            }
            
            // 将新数据添加到MP3输入缓冲区
            if (chunk.data && chunk.size > 0) {
                // 移动剩余数据到缓冲区开头
                if (bytes_left > 0 && read_ptr != mp3_input_buffer) {
                    memmove(mp3_input_buffer, read_ptr, bytes_left);
                }
                
                // 检查缓冲区空间
                size_t space_available = 8192 - bytes_left;
                size_t copy_size = std::min(chunk.size, space_available);
                
                // 复制新数据
                memcpy(mp3_input_buffer + bytes_left, chunk.data, copy_size);
                bytes_left += copy_size;
                read_ptr = mp3_input_buffer;
                
                // 检查并跳过ID3标签
                if (!id3_processed && bytes_left >= 10) {
                    size_t id3_skip = SkipId3Tag(read_ptr, bytes_left);
                    if (id3_skip > 0) {
                        read_ptr += id3_skip;
                        bytes_left -= id3_skip;
                        ESP_LOGI(TAG, "Skipped ID3 tag: %u bytes", (unsigned int)id3_skip);
                    }
                    id3_processed = true;
                }
                
                // 释放chunk内存
                heap_caps_free(chunk.data);
            }
        }
        
        // 确保有足够数据进行解码
        if (bytes_left < 512) {
            ESP_LOGD(TAG, "Not enough data for MP3 decode: %d bytes", bytes_left);
            continue;
        }
        
        // 尝试找到MP3帧同步
        int sync_offset = MP3FindSyncWord(read_ptr, bytes_left);
        if (sync_offset < 0) {
            ESP_LOGW(TAG, "No MP3 sync word found in %d bytes, clearing buffer", bytes_left);
            // Debug: in ra một số byte đầu
            if (bytes_left >= 16) {
                ESP_LOGW(TAG, "First 16 bytes: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                        read_ptr[0], read_ptr[1], read_ptr[2], read_ptr[3],
                        read_ptr[4], read_ptr[5], read_ptr[6], read_ptr[7],
                        read_ptr[8], read_ptr[9], read_ptr[10], read_ptr[11],
                        read_ptr[12], read_ptr[13], read_ptr[14], read_ptr[15]);
            }
            bytes_left = 0;
            continue;
        }
        
        // 跳过到同步位置
        if (sync_offset > 0) {
            ESP_LOGD(TAG, "Found MP3 sync at offset %d", sync_offset);
            read_ptr += sync_offset;
            bytes_left -= sync_offset;
        }
        
        // 解码MP3帧
        int16_t pcm_buffer[2304];
        int decode_result = MP3Decode(mp3_decoder_, &read_ptr, &bytes_left, pcm_buffer, 0);
        
        if (decode_result == 0) {
            // 解码成功，获取帧信息
            MP3GetLastFrameInfo(mp3_decoder_, &mp3_frame_info_);
            
            // 基本的帧信息有效性检查
            if (mp3_frame_info_.samprate == 0 || mp3_frame_info_.nChans == 0) {
                ESP_LOGW(TAG, "Invalid frame info: rate=%d, channels=%d, skipping", 
                        mp3_frame_info_.samprate, mp3_frame_info_.nChans);
                continue;
            }
            
            // 将PCM数据发送到Application的音频解码队列
            if (mp3_frame_info_.outputSamps > 0) {
                int16_t* final_pcm_data = pcm_buffer;
                int final_sample_count = mp3_frame_info_.outputSamps;
                std::vector<int16_t> mono_buffer;
                
                // 如果是双通道，转换为单通道混合
                if (mp3_frame_info_.nChans == 2) {
                    int stereo_samples = mp3_frame_info_.outputSamps;
                    int mono_samples = stereo_samples / 2;
                    
                    mono_buffer.resize(mono_samples);
                    
                    for (int i = 0; i < mono_samples; ++i) {
                        int left = pcm_buffer[i * 2];
                        int right = pcm_buffer[i * 2 + 1];
                        mono_buffer[i] = (int16_t)((left + right) / 2);
                    }
                    
                    final_pcm_data = mono_buffer.data();
                    final_sample_count = mono_samples;
                } 
                
                // 创建AudioStreamPacket
                AudioStreamPacket packet;
                packet.sample_rate = mp3_frame_info_.samprate;
                packet.frame_duration = 60;
                packet.timestamp = 0;
                
                // 将int16_t PCM数据转换为uint8_t字节数组
                size_t pcm_size_bytes = final_sample_count * sizeof(int16_t);
                packet.payload.resize(pcm_size_bytes);
                memcpy(packet.payload.data(), final_pcm_data, pcm_size_bytes);

                if (final_pcm_data_fft == nullptr) {
                    final_pcm_data_fft = (int16_t*)heap_caps_malloc(
                        final_sample_count * sizeof(int16_t),
                        MALLOC_CAP_SPIRAM
                    );
                }
                
                memcpy(
                    final_pcm_data_fft,
                    final_pcm_data,
                    final_sample_count * sizeof(int16_t)
                );
                
                // 发送到Application的音频解码队列
                app.AddAudioData(std::move(packet));
                total_played += pcm_size_bytes;
                
                // 打印播放进度
                if (total_played % (128 * 1024) == 0) {
                    ESP_LOGI(TAG, "Played %d bytes, buffer size: %d", total_played, buffer_size_);
                }
            }
            
        } else {
            // 解码失败 - Xử lý các lỗi khác nhau
            static int consecutive_errors = 0;
            consecutive_errors++;
            
            if (consecutive_errors <= 5) {
                ESP_LOGW(TAG, "MP3 decode failed with error: %d (attempt %d)", decode_result, consecutive_errors);
                
                // Debug thông tin về dữ liệu hiện tại
                if (bytes_left >= 4) {
                    ESP_LOGW(TAG, "Current data header: %02X %02X %02X %02X", 
                            read_ptr[0], read_ptr[1], read_ptr[2], read_ptr[3]);
                }
            } else if (consecutive_errors == 10) {
                ESP_LOGE(TAG, "Too many consecutive MP3 decode errors, may not be MP3 stream");
            }
            
            // Đặt lại counter khi decode thành công
            if (decode_result == 0) {
                consecutive_errors = 0;
            }
            
            // 跳过一些字节继续尝试 - Skip nhiều hơn khi lỗi liên tục
            int skip_bytes = (consecutive_errors > 5) ? 64 : 1;
            if (bytes_left > skip_bytes) {
                read_ptr += skip_bytes;
                bytes_left -= skip_bytes;
            } else {
                bytes_left = 0;
            }
        }
    }
    
    // 清理
    if (mp3_input_buffer) {
        heap_caps_free(mp3_input_buffer);
    }
    
    ESP_LOGI(TAG, "Radio stream playback finished, total played: %d bytes", total_played);
    is_playing_ = false;
    
    // 停止FFT显示
    if (display_mode_ == DISPLAY_MODE_SPECTRUM) {
        auto& board = Board::GetInstance();
        auto display = board.GetDisplay();
        if (display) {
            display->stopFft();
            ESP_LOGI(TAG, "Stopped FFT display from play thread (spectrum mode)");
        }
    }
}

void Esp32Radio::ClearAudioBuffer() {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    
    while (!audio_buffer_.empty()) {
        RadioAudioChunk chunk = audio_buffer_.front();
        audio_buffer_.pop();
        if (chunk.data) {
            heap_caps_free(chunk.data);
        }
    }
    
    buffer_size_ = 0;
    ESP_LOGI(TAG, "Radio audio buffer cleared");
}

bool Esp32Radio::InitializeMp3Decoder() {
    mp3_decoder_ = MP3InitDecoder();
    if (mp3_decoder_ == nullptr) {
        ESP_LOGE(TAG, "Failed to initialize MP3 decoder");
        mp3_decoder_initialized_ = false;
        return false;
    }
    
    mp3_decoder_initialized_ = true;
    ESP_LOGI(TAG, "MP3 decoder initialized successfully");
    return true;
}

void Esp32Radio::CleanupMp3Decoder() {
    if (mp3_decoder_ != nullptr) {
        MP3FreeDecoder(mp3_decoder_);
        mp3_decoder_ = nullptr;
    }
    mp3_decoder_initialized_ = false;
    ESP_LOGI(TAG, "MP3 decoder cleaned up");
}

void Esp32Radio::ResetSampleRate() {
    auto& board = Board::GetInstance();
    auto codec = board.GetAudioCodec();
    if (codec && codec->original_output_sample_rate() > 0 && 
        codec->output_sample_rate() != codec->original_output_sample_rate()) {
        ESP_LOGI(TAG, "重置采样率：从 %d Hz 重置到原始值 %d Hz", 
                codec->output_sample_rate(), codec->original_output_sample_rate());
        if (codec->SetOutputSampleRate(-1)) {
            ESP_LOGI(TAG, "成功重置采样率到原始值: %d Hz", codec->output_sample_rate());
        } else {
            ESP_LOGW(TAG, "无法重置采样率到原始值");
        }
    }
}

size_t Esp32Radio::SkipId3Tag(uint8_t* data, size_t size) {
    if (!data || size < 10) {
        return 0;
    }
    
    // 检查ID3v2标签头 "ID3"
    if (memcmp(data, "ID3", 3) != 0) {
        return 0;
    }
    
    // 计算标签大小（synchsafe integer格式）
    uint32_t tag_size = ((uint32_t)(data[6] & 0x7F) << 21) |
                        ((uint32_t)(data[7] & 0x7F) << 14) |
                        ((uint32_t)(data[8] & 0x7F) << 7)  |
                        ((uint32_t)(data[9] & 0x7F));
    
    // ID3v2头部(10字节) + 标签内容
    size_t total_skip = 10 + tag_size;
    
    // 确保不超过可用数据大小
    if (total_skip > size) {
        total_skip = size;
    }
    
    ESP_LOGI(TAG, "Found ID3v2 tag, skipping %u bytes", (unsigned int)total_skip);
    return total_skip;
}

void Esp32Radio::SetDisplayMode(DisplayMode mode) {
    DisplayMode old_mode = display_mode_.load();
    display_mode_ = mode;
    
    ESP_LOGI(TAG, "Display mode changed from %s to %s", 
            (old_mode == DISPLAY_MODE_SPECTRUM) ? "SPECTRUM" : "INFO",
            (mode == DISPLAY_MODE_SPECTRUM) ? "SPECTRUM" : "INFO");
}