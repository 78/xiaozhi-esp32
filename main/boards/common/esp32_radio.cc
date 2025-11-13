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
#include <mbedtls/sha256.h>
#include <cstring>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <thread>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "Esp32Radio"

/**
 * @brief 获取设备MAC地址
 * @return MAC地址字符串
 */
static std::string get_device_mac() {
    return SystemInfo::GetMacAddress();
}

/**
 * @brief 获取设备芯片ID
 * @return 芯片ID字符串
 */
static std::string get_device_chip_id() {
    // 使用MAC地址作为芯片ID，去除冒号分隔符
    std::string mac = SystemInfo::GetMacAddress();
    // 去除所有冒号
    mac.erase(std::remove(mac.begin(), mac.end(), ':'), mac.end());
    return mac;
}

/**
 * @brief 生成动态密钥
 * @param timestamp 时间戳
 * @return 动态密钥字符串
 */
static std::string generate_dynamic_key(int64_t timestamp) {
    // 密钥（请修改为与服务端一致）
    const std::string secret_key = "your-esp32-secret-key-2024";
    
    // 获取设备信息
    std::string mac = get_device_mac();
    std::string chip_id = get_device_chip_id();
    
    // 组合数据：MAC:芯片ID:时间戳:密钥
    std::string data = mac + ":" + chip_id + ":" + std::to_string(timestamp) + ":" + secret_key;
    
    // SHA256哈希
    unsigned char hash[32];
    mbedtls_sha256((unsigned char*)data.c_str(), data.length(), hash, 0);
    
    // 转换为十六进制字符串（前16字节）
    std::string key;
    for (int i = 0; i < 16; i++) {
        char hex[3];
        snprintf(hex, sizeof(hex), "%02X", hash[i]);
        key += hex;
    }
    
    return key;
}

/**
 * @brief 为HTTP请求添加认证头
 * @param http HTTP客户端指针
 */
static void add_auth_headers(Http* http) {
    // 获取当前时间戳
    int64_t timestamp = esp_timer_get_time() / 1000000;  // 转换为秒
    
    // 生成动态密钥
    std::string dynamic_key = generate_dynamic_key(timestamp);
    
    // 获取设备信息
    std::string mac = get_device_mac();
    std::string chip_id = get_device_chip_id();
    
    // 添加认证头
    if (http) {
        http->SetHeader("X-MAC-Address", mac);
        http->SetHeader("X-Chip-ID", chip_id);
        http->SetHeader("X-Timestamp", std::to_string(timestamp));
        http->SetHeader("X-Dynamic-Key", dynamic_key);
        
        ESP_LOGI(TAG, "Added auth headers - MAC: %s, ChipID: %s, Timestamp: %lld", 
                 mac.c_str(), chip_id.c_str(), timestamp);
    }
}

Esp32Radio::Esp32Radio() : current_station_name_(), current_station_url_(),
                         station_name_displayed_(false), radio_stations_(),
                         display_mode_(DISPLAY_MODE_INFO), is_playing_(false), is_downloading_(false), 
                         play_thread_(), download_thread_(), audio_buffer_(), buffer_mutex_(), 
                         buffer_cv_(), buffer_size_(0), aac_decoder_(nullptr), aac_info_(),
                         aac_decoder_initialized_(false), aac_info_ready_(false), aac_out_buffer_() {
    ESP_LOGI(TAG, "VOV Radio player initialized with AAC decoder support");
    InitializeRadioStations();
    // AAC decoder will be initialized on-demand
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
    
    // 清理缓冲区和 AAC 解码器
    ClearAudioBuffer();
    CleanupAacDecoder();
    
    ESP_LOGI(TAG, "Radio player destroyed successfully");
}

void Esp32Radio::InitializeRadioStations() {
    // Vietnamese VOV radio stations - AAC+ format only
    // These streams return Content-Type: audio/aacp and require AAC decoder
    radio_stations_["VOV1"] = RadioStation("VOV 1 - Đài Tiếng nói Việt Nam", "https://stream.vovmedia.vn/vov-1", "Kênh thông tin tổng hợp", "News/Talk");
    radio_stations_["VOV2"] = RadioStation("VOV 2 - Âm thanh Việt Nam", "https://stream.vovmedia.vn/vov-2", "Kênh văn hóa - văn nghệ", "Culture/Music");  
    radio_stations_["VOV3"] = RadioStation("VOV 3 - Tiếng nói Việt Nam", "https://stream.vovmedia.vn/vov-3", "Kênh thông tin - giải trí", "Entertainment");
    radio_stations_["VOV5"] = RadioStation("VOV 5 - Tiếng nói người Việt", "https://stream.vovmedia.vn/vov5", "Kênh dành cho người Việt ở nước ngoài", "Overseas Vietnamese");
    radio_stations_["VOVGT"] = RadioStation("VOV Giao thông Hà Nội", "https://stream.vovmedia.vn/vovgt-hn", "Thông tin giao thông Hà Nội", "Traffic");
    radio_stations_["VOVGT_HCM"] = RadioStation("VOV Giao thông TP.HCM", "https://stream.vovmedia.vn/vovgt-hcm", "Thông tin giao thông TP. Hồ Chí Minh", "Traffic");
    radio_stations_["VOV_ENGLISH"] = RadioStation("VOV Tiếng Anh", "https://stream.vovmedia.vn/vov247", "VOV English Service", "International");
    radio_stations_["VOV_MEKONG"] = RadioStation("VOV Mê Kông", "https://stream.vovmedia.vn/vovmekong", "Kênh vùng Đồng bằng sông Cửu Long", "Regional");
    radio_stations_["VOV_MIENTRUNG"] = RadioStation("VOV Miền Trung", "https://stream.vovmedia.vn/vov4mt", "Kênh vùng miền Trung", "Regional");
    radio_stations_["VOV_TAYBAC"] = RadioStation("VOV Tây Bắc", "https://stream.vovmedia.vn/vov4tb", "Kênh vùng Tây Bắc", "Regional");
    radio_stations_["VOV_DONGBAC"] = RadioStation("VOV Đông Bắc", "https://stream.vovmedia.vn/vov4db", "Kênh vùng Đông Bắc", "Regional");
    radio_stations_["VOV_TAYNGUYEN"] = RadioStation("VOV Tây Nguyên", "https://stream.vovmedia.vn/vov4tn", "Kênh vùng Tây Nguyên", "Regional");
    
    ESP_LOGI(TAG, "Initialized %d VOV radio stations (AAC format only)", radio_stations_.size());
}

bool Esp32Radio::PlayStation(const std::string& station_name) {
    ESP_LOGI(TAG, "Request to play radio station: %s", station_name.c_str());
    
    // Tìm station trong danh sách
    auto it = radio_stations_.find(station_name);
    if (it != radio_stations_.end()) {
        return PlayUrl(it->second.url, it->second.name);
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
    if (!is_playing_ && !is_downloading_) {
        ESP_LOGW(TAG, "No streaming in progress to stop");
        return true;
    }

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
    
    http->SetHeader("User-Agent", "ESP32-Music-Player/1.0");
    http->SetHeader("Accept", "*/*");
    http->SetHeader("Range", "bytes=0-");  // 支持断点续传

    // 添加ESP32认证头
    add_auth_headers(http.get());
    
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
            display->SetMusicInfo("Lỗi kết nối radio");
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
            break;
        }
        if (bytes_read == 0) {
            ESP_LOGI(TAG, "Radio stream ended, total: %d bytes", total_downloaded);
            // 对于直播流，这通常意味着连接中断，尝试重连
            vTaskDelay(pdMS_TO_TICKS(1000));  // 等待1秒后继续
            continue;
        }

        if (bytes_read < 16) {
            ESP_LOGI(TAG, "Data chunk too small: %d bytes", bytes_read);
        }
        
        // VOV streams use AAC+ format - log format detection
        if (total_downloaded == 0 && bytes_read >= 4) {
            if (memcmp(buffer, "ID3", 3) == 0) {
                ESP_LOGI(TAG, "Detected MP3 file with ID3 tag");
            } else if (buffer[0] == 0xFF && (buffer[1] & 0xE0) == 0xE0) {
                ESP_LOGI(TAG, "Detected MP3 file header");
            } else if (memcmp(buffer, "RIFF", 4) == 0) {
                ESP_LOGI(TAG, "Detected WAV file");
            } else if (memcmp(buffer, "fLaC", 4) == 0) {
                ESP_LOGI(TAG, "Detected FLAC file");
            } else if (memcmp(buffer, "OggS", 4) == 0) {
                ESP_LOGI(TAG, "Detected OGG file");
            } else {
                ESP_LOGI(TAG, "Unknown audio format, first 4 bytes: %02X %02X %02X %02X", 
                        (unsigned char)buffer[0], (unsigned char)buffer[1], 
                        (unsigned char)buffer[2], (unsigned char)buffer[3]);
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
    ESP_LOGI(TAG, "Starting VOV radio stream playback with AAC decoder");
    
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
    
    // Initialize AAC decoder
    if (!InitializeAacDecoder()) {
        ESP_LOGE(TAG, "Failed to initialize AAC decoder for VOV streams");
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
                std::string formatted_station_name = "《" + current_station_name_ + "》播放中...";
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
                
                // AAC streams don't need ID3 tag processing
                
                // 释放chunk内存
                heap_caps_free(chunk.data);
            }
        }
        
        // AAC DECODER for VOV streams
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
                    is_playing_ = false;
                    break;
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

    }
    
    // 清理
    if (input_buffer) {
        heap_caps_free(input_buffer);
    }
    
    // Cleanup AAC decoder
    CleanupAacDecoder();
    
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