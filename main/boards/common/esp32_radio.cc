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
                         mp3_decoder_initialized_(false), aac_decoder_(nullptr), aac_info_(),
                         aac_decoder_initialized_(false), aac_info_ready_(false), aac_out_buffer_(),
                         is_radio_mode_(false), format_detected_(false) {
    ESP_LOGI(TAG, "Radio player initialized with dual decoder support (MP3 + AAC)");
    InitializeRadioStations();
    InitializeMp3Decoder();
    // AAC decoder will be initialized on-demand for radio streams
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
    
    // 清理缓冲区和解码器
    ClearAudioBuffer();
    CleanupMp3Decoder();
    CleanupAacDecoder();
    
    ESP_LOGI(TAG, "Radio player destroyed successfully");
}

void Esp32Radio::InitializeRadioStations() {
    // Vietnamese VOV radio stations - AAC+ format (now supported!)
    // These streams return Content-Type: audio/aacp and require AAC decoder
    radio_stations_["VOV1"] = RadioStation("VOV 1 - Đài Tiếng nói Việt Nam", "https://stream.vovmedia.vn/vov-1", "Kênh thông tin tổng hợp", "News/Talk");
    radio_stations_["VOV2"] = RadioStation("VOV 2 - Âm thanh Việt Nam", "https://stream.vovmedia.vn/vov-2", "Kênh văn hóa - văn nghệ", "Culture/Music");  
    radio_stations_["VOV3"] = RadioStation("VOV 3 - Tiếng nói Việt Nam", "https://stream.vovmedia.vn/vov-3", "Kênh thông tin - giải trí", "Entertainment");
    radio_stations_["VOV5"] = RadioStation("VOV 5 - Tiếng nói người Việt", "https://stream.vovmedia.vn/vov5", "Kênh dành cho người Việt ở nước ngoài", "Overseas Vietnamese");
    radio_stations_["VOVGT"] = RadioStation("VOV Giao thông Hà Nội", "https://stream.vovmedia.vn/vovgt-hn", "Thông tin giao thông Hà Nội", "Traffic");
    radio_stations_["VOVGT_HCM"] = RadioStation("VOV Giao thông TP.HCM", "https://stream.vovmedia.vn/vovgt-hcm", "Thông tin giao thông TP. Hồ Chí Minh", "Traffic");
    
    // Additional VOV stations
    radio_stations_["VOV_ENGLISH"] = RadioStation("VOV Tiếng Anh", "https://stream.vovmedia.vn/vov247", "VOV English Service", "International");
    radio_stations_["VOV_MEKONG"] = RadioStation("VOV Mê Kông", "https://stream.vovmedia.vn/vovmekong", "Kênh vùng Đồng bằng sông Cửu Long", "Regional");
    
    // Vietnamese FM stations - Try direct streams first, may be MP3 or AAC
    radio_stations_["SAIGONRADIO"] = RadioStation("Saigon Radio 99.9FM", "http://113.161.6.157:8888/stream", "Đài phát thanh Sài Gòn", "Music");
    radio_stations_["FMVN"] = RadioStation("FM Việt Nam", "http://27.71.232.10:8000/stream", "FM Vietnam", "Music");
    
    // International stations - Đã test hoạt động
    radio_stations_["TESTMP3"] = RadioStation("SomaFM Groove Salad", "http://ice1.somafm.com/groovesalad-256-mp3", "Ambient Electronic", "Ambient");
    radio_stations_["JAZZRADIO"] = RadioStation("Jazz Radio", "http://jazz-wr11.ice.infomaniak.ch/jazz-wr11-128.mp3", "Jazz Music", "Jazz");
    radio_stations_["BBC"] = RadioStation("BBC World Service", "http://stream.live.vc.bbcmedia.co.uk/bbc_world_service", "BBC World Service English", "News/International");
    radio_stations_["CHILLOUT"] = RadioStation("Chillout Radio", "http://5.39.71.159:8488/stream", "Chillout Music", "Chillout");
    
    // Proven working backup stations
    radio_stations_["SOMAGROOVE"] = RadioStation("SomaFM Groove", "http://ice1.somafm.com/groovesalad-256-mp3", "Groove Salad Ambient", "Ambient");
    radio_stations_["SOMADRONE"] = RadioStation("SomaFM Drone", "http://ice1.somafm.com/dronezone-256-mp3", "Drone Zone Ambient", "Ambient");
    radio_stations_["SOMABEAT"] = RadioStation("SomaFM Beat", "http://ice1.somafm.com/beatblender-128-mp3", "Beat Blender", "Electronic");
    
    ESP_LOGI(TAG, "Initialized %d radio stations (MP3 + AAC format support)", radio_stations_.size());
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
    
    // 清空缓冲区和重置格式检测
    ClearAudioBuffer();
    format_detected_ = false;  // Reset format detection for new stream
    is_radio_mode_.store(false);  // Default to MP3, will be updated by format detection
    
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
        
        // Enhanced format detection with dual decoder support
        if (!format_detected_ && total_downloaded == 0 && bytes_read >= 4) {
            if (memcmp(buffer, "ID3", 3) == 0) {
                ESP_LOGI(TAG, "✅ Detected MP3 stream with ID3 tag");
                is_radio_mode_.store(false);  // MP3 format
                format_detected_ = true;
            } else if (buffer[0] == 0xFF && (buffer[1] & 0xE0) == 0xE0) {
                ESP_LOGI(TAG, "✅ Detected MP3 stream header");
                is_radio_mode_.store(false);  // MP3 format
                format_detected_ = true;
            } else if ((buffer[0] == 0xFF && (buffer[1] & 0xF0) == 0xF0) || 
                       memcmp(buffer, "ADTS", 4) == 0) {
                ESP_LOGI(TAG, "✅ Detected AAC/ADTS stream - Using AAC decoder");
                ESP_LOGI(TAG, "VOV streams use audio/aacp (AAC+) format - now supported!");
                is_radio_mode_.store(true);   // AAC format - radio mode
                format_detected_ = true;
            } else if (memcmp(buffer, "#EXT", 4) == 0 || memcmp(buffer, "#EXT-X", 6) == 0) {
                ESP_LOGE(TAG, "❌ Detected HLS/M3U8 playlist, this URL is not a direct stream");
                ESP_LOGE(TAG, "Content preview: %.100s", buffer);
                break;  // Dừng download vì không phải direct stream
            } else if (memcmp(buffer, "<?xml", 5) == 0 || memcmp(buffer, "<html", 5) == 0) {
                ESP_LOGE(TAG, "❌ Detected HTML/XML content, not an audio stream");
                ESP_LOGE(TAG, "Content preview: %.100s", buffer);
                break;
            } else {
                ESP_LOGW(TAG, "⚠️  Unknown stream format, first 16 bytes:");
                for (int i = 0; i < std::min(16, bytes_read); i++) {
                    printf("%02X ", (unsigned char)buffer[i]);
                }
                printf("\n");
                // In dưới dạng text nếu có thể
                ESP_LOGW(TAG, "As text: %.50s", buffer);
                
                // Heuristic: VOV URLs likely AAC, others likely MP3
                if (radio_url.find("vovmedia.vn") != std::string::npos) {
                    ESP_LOGW(TAG, "VOV domain detected, assuming AAC format");
                    is_radio_mode_.store(true);
                } else {
                    ESP_LOGW(TAG, "Non-VOV domain, attempting MP3 decode");
                    is_radio_mode_.store(false);
                }
                format_detected_ = true;
            }
            
            // Log detected format
            ESP_LOGI(TAG, "Stream format: %s decoder will be used", 
                    is_radio_mode_.load() ? "AAC" : "MP3");
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
    ESP_LOGI(TAG, "Starting radio stream playback with dual decoder support");
    
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
    
    // Wait for format detection
    while (is_playing_ && !format_detected_) {
        vTaskDelay(pdMS_TO_TICKS(100));
        ESP_LOGD(TAG, "Waiting for format detection...");
    }
    
    if (!is_playing_) {
        ESP_LOGI(TAG, "Playback stopped during format detection");
        return;
    }
    
    bool radio_mode = is_radio_mode_.load();
    ESP_LOGI(TAG, "Format detected: %s decoder will be used", radio_mode ? "AAC" : "MP3");
    
    // Initialize appropriate decoder
    if (radio_mode) {
        if (!InitializeAacDecoder()) {
            ESP_LOGE(TAG, "Failed to initialize AAC decoder for radio mode");
            is_playing_ = false;
            return;
        }
    } else {
        if (!mp3_decoder_initialized_) {
            ESP_LOGE(TAG, "MP3 decoder not initialized");
            is_playing_ = false;
            return;
        }
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
    uint8_t* input_buffer = nullptr;
    int bytes_left = 0;
    uint8_t* read_ptr = nullptr;
    
    // 分配输入缓冲区 (cho cả MP3 và AAC)
    input_buffer = (uint8_t*)heap_caps_malloc(8192, MALLOC_CAP_SPIRAM);
    if (!input_buffer) {
        ESP_LOGE(TAG, "Failed to allocate input buffer");
        is_playing_ = false;
        return;
    }
    
    // 标记是否已经处理过ID3标签 (chỉ cho MP3)
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
        
        // 如果需要更多音频数据，从缓冲区读取
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
            
            // 将新数据添加到输入缓冲区
            if (chunk.data && chunk.size > 0) {
                // 移动剩余数据到缓冲区开头
                if (bytes_left > 0 && read_ptr != input_buffer) {
                    memmove(input_buffer, read_ptr, bytes_left);
                }
                
                // 检查缓冲区空间
                size_t space_available = 8192 - bytes_left;
                size_t copy_size = std::min(chunk.size, space_available);
                
                // 复制新数据
                memcpy(input_buffer + bytes_left, chunk.data, copy_size);
                bytes_left += copy_size;
                read_ptr = input_buffer;
                
                // 检查并跳过ID3标签 (chỉ cho MP3)
                if (!radio_mode && !id3_processed && bytes_left >= 10) {
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
        
        // DUAL DECODER LOGIC - AAC for radio, MP3 for music
        if (radio_mode) {
            // === AAC DECODER PATH ===
            if (bytes_left <= 0) {
                continue; // Need more data
            }
            
            bool input_eos = (!is_downloading_ && audio_buffer_.empty());
            
            esp_audio_simple_dec_raw_t raw = {};
            raw.buffer = read_ptr;
            raw.len = bytes_left;
            raw.eos = input_eos;
            
            esp_audio_simple_dec_out_t out_frame = {};
            out_frame.buffer = aac_out_buffer_.data();
            out_frame.len = aac_out_buffer_.size();
            
            while (raw.len > 0 && is_playing_) {
                esp_audio_err_t dec_ret = esp_audio_simple_dec_process(aac_decoder_, &raw, &out_frame);
                if (dec_ret == ESP_AUDIO_ERR_BUFF_NOT_ENOUGH) {
                    // Output buffer not enough, expand and retry
                    aac_out_buffer_.resize(out_frame.needed_size);
                    out_frame.buffer = aac_out_buffer_.data();
                    out_frame.len = out_frame.needed_size;
                    continue;
                }
                if (dec_ret != ESP_AUDIO_ERR_OK) {
                    ESP_LOGE(TAG, "AAC decode error: %d", dec_ret);
                    // Skip some bytes and continue
                    if (raw.len > 1) {
                        raw.buffer++;
                        raw.len--;
                    } else {
                        bytes_left = 0;
                        break;
                    }
                    continue;
                }
                
                if (out_frame.decoded_size > 0) {
                    // First decode -> get stream info
                    if (!aac_info_ready_) {
                        esp_audio_simple_dec_get_info(aac_decoder_, &aac_info_);
                        aac_info_ready_ = true;
                        ESP_LOGI(TAG, "AAC stream info: %d Hz, %d bits, %d ch",
                                aac_info_.sample_rate, aac_info_.bits_per_sample, aac_info_.channel);
                    }
                    
                    int bits_per_sample = (aac_info_.bits_per_sample > 0) ? aac_info_.bits_per_sample : 16;
                    int bytes_per_sample = bits_per_sample / 8;
                    int channels = (aac_info_.channel > 0) ? aac_info_.channel : 2;
                    
                    int total_samples = out_frame.decoded_size / bytes_per_sample;
                    int samples_per_channel = (channels > 0) ? (total_samples / channels) : total_samples;
                    
                    int16_t* pcm_in = reinterpret_cast<int16_t*>(out_frame.buffer);
                    std::vector<int16_t> mono_buffer;
                    int16_t* final_pcm_data = nullptr;
                    int final_sample_count = 0;
                    
                    if (channels == 2) {
                        // Downmix stereo -> mono
                        mono_buffer.resize(samples_per_channel);
                        for (int i = 0; i < samples_per_channel; ++i) {
                            int left = pcm_in[i * 2];
                            int right = pcm_in[i * 2 + 1];
                            mono_buffer[i] = (int16_t)((left + right) / 2);
                        }
                        final_pcm_data = mono_buffer.data();
                        final_sample_count = samples_per_channel;
                    } else if (channels == 1) {
                        final_pcm_data = pcm_in;
                        final_sample_count = total_samples;
                    } else {
                        // Unknown channels: treat as mono
                        final_pcm_data = pcm_in;
                        final_sample_count = total_samples;
                    }
                    
                    // Create AudioStreamPacket
                    AudioStreamPacket packet;
                    packet.sample_rate = aac_info_.sample_rate;
                    packet.frame_duration = 60;
                    packet.timestamp = 0;
                    
                    size_t pcm_size_bytes = final_sample_count * sizeof(int16_t);
                    packet.payload.resize(pcm_size_bytes);
                    memcpy(packet.payload.data(), final_pcm_data, pcm_size_bytes);
                    
                    // Save for FFT display
                    if (final_pcm_data_fft == nullptr) {
                        final_pcm_data_fft = (int16_t*)heap_caps_malloc(
                            final_sample_count * sizeof(int16_t), MALLOC_CAP_SPIRAM);
                    }
                    if (final_pcm_data_fft) {
                        memcpy(final_pcm_data_fft, final_pcm_data, pcm_size_bytes);
                    }
                    
                    app.AddAudioData(std::move(packet));
                    total_played += pcm_size_bytes;
                    
                    if (total_played % (128 * 1024) == 0) {
                        ESP_LOGI(TAG, "AAC: Played %d bytes, buffer size: %d", total_played, buffer_size_);
                    }
                }
                
                // Update input pointer based on consumed bytes
                raw.len -= raw.consumed;
                raw.buffer += raw.consumed;
            }
            
            // Update read_ptr and bytes_left for main loop
            bytes_left = raw.len;
            read_ptr = const_cast<uint8_t*>(raw.buffer);
            
            // Check for end of stream
            if (input_eos && bytes_left == 0) {
                ESP_LOGI(TAG, "AAC radio stream ended");
                break;
            }
            
        } else {
            // === MP3 DECODER PATH ===
            if (bytes_left < 512) {
                ESP_LOGD(TAG, "Not enough data for MP3 decode: %d bytes", bytes_left);
                continue;
            }
            
            // Find MP3 sync word
            int sync_offset = MP3FindSyncWord(read_ptr, bytes_left);
            if (sync_offset < 0) {
                ESP_LOGW(TAG, "No MP3 sync word found in %d bytes, clearing buffer", bytes_left);
                bytes_left = 0;
                continue;
            }
            
            // Skip to sync position
            if (sync_offset > 0) {
                ESP_LOGD(TAG, "Found MP3 sync at offset %d", sync_offset);
                read_ptr += sync_offset;
                bytes_left -= sync_offset;
            }
            
            // Decode MP3 frame
            int16_t pcm_buffer[2304];
            int decode_result = MP3Decode(mp3_decoder_, &read_ptr, &bytes_left, pcm_buffer, 0);
            
            if (decode_result == 0) {
                // Decode success
                MP3GetLastFrameInfo(mp3_decoder_, &mp3_frame_info_);
                
                if (mp3_frame_info_.samprate == 0 || mp3_frame_info_.nChans == 0) {
                    ESP_LOGW(TAG, "Invalid MP3 frame info: rate=%d, channels=%d", 
                            mp3_frame_info_.samprate, mp3_frame_info_.nChans);
                    continue;
                }
                
                if (mp3_frame_info_.outputSamps > 0) {
                    int16_t* final_pcm_data = pcm_buffer;
                    int final_sample_count = mp3_frame_info_.outputSamps;
                    std::vector<int16_t> mono_buffer;
                    
                    // Convert stereo to mono if needed
                    if (mp3_frame_info_.nChans == 2) {
                        int mono_samples = mp3_frame_info_.outputSamps / 2;
                        mono_buffer.resize(mono_samples);
                        
                        for (int i = 0; i < mono_samples; ++i) {
                            int left = pcm_buffer[i * 2];
                            int right = pcm_buffer[i * 2 + 1];
                            mono_buffer[i] = (int16_t)((left + right) / 2);
                        }
                        
                        final_pcm_data = mono_buffer.data();
                        final_sample_count = mono_samples;
                    }
                    
                    // Create AudioStreamPacket
                    AudioStreamPacket packet;
                    packet.sample_rate = mp3_frame_info_.samprate;
                    packet.frame_duration = 60;
                    packet.timestamp = 0;
                    
                    size_t pcm_size_bytes = final_sample_count * sizeof(int16_t);
                    packet.payload.resize(pcm_size_bytes);
                    memcpy(packet.payload.data(), final_pcm_data, pcm_size_bytes);

                    // Save for FFT display
                    if (final_pcm_data_fft == nullptr) {
                        final_pcm_data_fft = (int16_t*)heap_caps_malloc(
                            final_sample_count * sizeof(int16_t), MALLOC_CAP_SPIRAM);
                    }
                    if (final_pcm_data_fft) {
                        memcpy(final_pcm_data_fft, final_pcm_data, pcm_size_bytes);
                    }
                    
                    app.AddAudioData(std::move(packet));
                    total_played += pcm_size_bytes;
                    
                    if (total_played % (128 * 1024) == 0) {
                        ESP_LOGI(TAG, "MP3: Played %d bytes, buffer size: %d", total_played, buffer_size_);
                    }
                }
            } else {
                // MP3 decode failed
                ESP_LOGW(TAG, "MP3 decode failed with error: %d", decode_result);
                if (bytes_left > 1) {
                    read_ptr++;
                    bytes_left--;
                } else {
                    bytes_left = 0;
                }
            }
        } // End of MP3/AAC decoder if-else
    }
    
    // 清理
    if (input_buffer) {
        heap_caps_free(input_buffer);
    }
    
    // Cleanup AAC decoder if it was initialized for this stream
    if (radio_mode) {
        CleanupAacDecoder();
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

// AAC Simple Decoder methods
bool Esp32Radio::InitializeAacDecoder() {
    if (aac_decoder_initialized_) {
        ESP_LOGW(TAG, "AAC decoder already initialized");
        return true;
    }
    
    ESP_LOGI(TAG, "Initializing AAC Simple Decoder for radio streams");
    
    // Register default decoders
    esp_audio_dec_register_default();
    esp_audio_simple_dec_register_default();
    
    // Configure AAC decoder
    esp_audio_simple_dec_cfg_t aac_cfg = {};
    aac_cfg.dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_AAC;
    aac_cfg.dec_cfg = nullptr;   // Use default config
    aac_cfg.cfg_size = 0;
    
    esp_audio_err_t dec_ret = esp_audio_simple_dec_open(&aac_cfg, &aac_decoder_);
    if (dec_ret != ESP_AUDIO_ERR_OK || !aac_decoder_) {
        ESP_LOGE(TAG, "Failed to open AAC simple decoder, ret=%d", dec_ret);
        esp_audio_simple_dec_unregister_default();
        esp_audio_dec_unregister_default();
        return false;
    }
    
    // Initialize output buffer
    aac_out_buffer_.resize(4096);
    aac_info_ready_ = false;
    aac_decoder_initialized_ = true;
    
    ESP_LOGI(TAG, "AAC Simple Decoder initialized successfully");
    return true;
}

void Esp32Radio::CleanupAacDecoder() {
    if (!aac_decoder_initialized_) {
        return;
    }
    
    if (aac_decoder_) {
        esp_audio_simple_dec_close(aac_decoder_);
        aac_decoder_ = nullptr;
    }
    
    esp_audio_simple_dec_unregister_default();
    esp_audio_dec_unregister_default();
    
    aac_out_buffer_.clear();
    aac_info_ready_ = false;
    aac_decoder_initialized_ = false;
    
    ESP_LOGI(TAG, "AAC Simple Decoder cleaned up");
}

void Esp32Radio::SetDisplayMode(DisplayMode mode) {
    DisplayMode old_mode = display_mode_.load();
    display_mode_ = mode;
    
    ESP_LOGI(TAG, "Display mode changed from %s to %s", 
            (old_mode == DISPLAY_MODE_SPECTRUM) ? "SPECTRUM" : "INFO",
            (mode == DISPLAY_MODE_SPECTRUM) ? "SPECTRUM" : "INFO");
}