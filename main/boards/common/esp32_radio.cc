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

Esp32Radio::Esp32Radio() : current_station_name_(), current_station_url_(),
                         station_name_displayed_(false), current_station_volume_(4.5f), radio_stations_(),
                         display_mode_(DISPLAY_MODE_SPECTRUM), is_playing_(false), is_downloading_(false), 
                         play_thread_(), download_thread_(), audio_buffer_(), buffer_mutex_(), 
                         buffer_cv_(), buffer_size_(0), aac_decoder_(nullptr), aac_info_(),
                         aac_decoder_initialized_(false), aac_info_ready_(false), aac_out_buffer_() {
}

Esp32Radio::~Esp32Radio() {
    ESP_LOGI(TAG, "Destroying radio player - stopping all operations");
    
    // Stop all operations
    is_downloading_ = false;
    is_playing_ = false;
    
    // Notify all waiting threads
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        buffer_cv_.notify_all();
    }
    
    // Wait for the download thread to finish
    if (download_thread_.joinable()) {
        ESP_LOGI(TAG, "Waiting for download thread to finish");
        download_thread_.join();
        ESP_LOGI(TAG, "Download thread finished");
    }
    
    // Wait for the playback thread to finish
    if (play_thread_.joinable()) {
        ESP_LOGI(TAG, "Waiting for playback thread to finish");
        play_thread_.join();
        ESP_LOGI(TAG, "Playback thread finished");
    }
    
    // Clear the buffer and clean up the AAC decoder
    ClearAudioBuffer();
    CleanupAacDecoder();
    
    ESP_LOGI(TAG, "Radio player destroyed successfully");
}

void Esp32Radio::Initialize() {
    ESP_LOGI(TAG, "VOV Radio player initialized with AAC decoder support");
    // AAC decoder will be initialized on-demand
    InitializeRadioStations();
}

void Esp32Radio::InitializeRadioStations() {
    // Vietnamese VOV radio stations - AAC/AAC+ format only
    // Volume values: 1.0 = 100%, 2.0 = 200%, etc.

    // === VOV - KÊNH QUỐC GIA ===
    radio_stations_["VOV1"]         = RadioStation("VOV 1 - Thời sự",                   "https://stream.vovmedia.vn/vov-1",       "Tin tức & thời sự quốc gia",               "News/Talk",            4.5f);
    radio_stations_["VOV2"]         = RadioStation("VOV 2 - Văn hóa & Giáo dục",        "https://stream.vovmedia.vn/vov-2",       "Văn hóa - giáo dục - xã hội",              "Culture/Education",    4.0f);
    radio_stations_["VOV3"]         = RadioStation("VOV 3 - Âm nhạc & Giải trí",        "https://stream.vovmedia.vn/vov-3",       "Nhạc & giải trí tổng hợp",                 "Music/Entertainment",  4.4f);
    radio_stations_["VOV5"]         = RadioStation("VOV 5 - Đối ngoại",                 "https://stream.vovmedia.vn/vov5",        "Kênh tiếng Việt & quốc tế",                "International",        4.1f);
    
    // === VOV GIAO THÔNG ===
    radio_stations_["VOV_GT_HN"]    = RadioStation("VOV Giao thông Hà Nội",             "https://stream.vovmedia.vn/vovgt-hn",    "Giao thông & đời sống Hà Nội",             "Traffic",              4.7f);
    radio_stations_["VOV_GT_HCM"]   = RadioStation("VOV Giao thông TP.HCM",             "https://stream.vovmedia.vn/vovgt-hcm",   "Giao thông & đời sống TP.HCM",             "Traffic",              4.7f);

    // === VOV VÙNG MIỀN (VOV4) ===
    radio_stations_["VOV_MEKONG"]       = RadioStation("VOV Mekong FM",                 "https://stream.vovmedia.vn/vovmekong",   "Miền Tây - Đồng bằng sông Cửu Long",       "Regional",             4.6f);
    radio_stations_["VOV4_MIENTRUNG"]   = RadioStation("VOV4 Miền Trung",               "https://stream.vovmedia.vn/vov4mt",      "Dân tộc - Miền Trung",                      "Regional",             4.3f);
    radio_stations_["VOV4_TAYBAC"]      = RadioStation("VOV4 Tây Bắc",                  "https://stream.vovmedia.vn/vov4tb",      "Dân tộc - Tây Bắc",                         "Regional",             4.4f);
    radio_stations_["VOV4_DONGBAC"]     = RadioStation("VOV4 Đông Bắc",                 "https://stream.vovmedia.vn/vov4db",      "Dân tộc - Đông Bắc",                        "Regional",             4.4f);
    radio_stations_["VOV4_TAYNGUYEN"]   = RadioStation("VOV4 Tây Nguyên",               "https://stream.vovmedia.vn/vov4tn",      "Dân tộc - Tây Nguyên",                      "Regional",             4.5f);
    radio_stations_["VOV4_DBSCL"]       = RadioStation("VOV4 ĐBSCL",                    "https://stream.vovmedia.vn/vov4dbscl",   "Dân tộc - Đồng bằng sông Cửu Long",         "Regional",             4.5f);
    radio_stations_["VOV4_HCM"]         = RadioStation("VOV4 TP.HCM",                   "https://stream.vovmedia.vn/vov4hcm",     "Dân tộc - TP.HCM",                          "Regional",             4.5f);

    // === VOV – TIẾNG ANH ===
    radio_stations_["VOV5_ENGLISH"]     = RadioStation("VOV 5 – English 24/7",          "https://stream.vovmedia.vn/vov247",      "Kênh tiếng Anh quốc tế",                   "International",        4.0f);

    ESP_LOGI(TAG, "Initialized %d VN radio stations (AAC format only)", radio_stations_.size());
}

bool Esp32Radio::PlayStation(const std::string& station_name) {
    ESP_LOGI(TAG, "Request to play radio station: %s", station_name.c_str());
    
    // Convert input to lowercase for case-insensitive search
    std::string lower_input = station_name;
    std::transform(lower_input.begin(), lower_input.end(), lower_input.begin(), ::tolower);
    
    // First, try to find by RadioStation.name (display name) - case insensitive partial match
    for (const auto& station : radio_stations_) {
        std::string lower_station_name = station.second.name;
        std::transform(lower_station_name.begin(), lower_station_name.end(), lower_station_name.begin(), ::tolower);
        
        // Check if input matches any part of the station display name
        if (lower_station_name.find(lower_input) != std::string::npos || 
            lower_input.find(lower_station_name) != std::string::npos) {
            ESP_LOGI(TAG, "Found station by display name: '%s' -> %s (volume: %.1fx)", station_name.c_str(), station.second.name.c_str(), station.second.volume);
            current_station_volume_ = station.second.volume;
            return PlayUrl(station.second.url, station.second.name);
        }
    }
    
    // Second, try to find by station key (VOV1, VOV2, etc.) - exact match
    auto it = radio_stations_.find(station_name);
    if (it != radio_stations_.end()) {
        ESP_LOGI(TAG, "Found station by key: '%s' -> %s (volume: %.1fx)", station_name.c_str(), it->second.name.c_str(), it->second.volume);
        current_station_volume_ = it->second.volume;
        return PlayUrl(it->second.url, it->second.name);
    }
    
    // Third, try to find by station key - case insensitive
    for (const auto& station : radio_stations_) {
        std::string lower_key = station.first;
        std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(), ::tolower);
        
        if (lower_key == lower_input) {
            ESP_LOGI(TAG, "Found station by key (case insensitive): '%s' -> %s (volume: %.1fx)", station_name.c_str(), station.second.name.c_str(), station.second.volume);
            current_station_volume_ = station.second.volume;
            return PlayUrl(station.second.url, station.second.name);
        }
    }
    
    // Handle specific regional stations with common search terms
    if (lower_input.find("tây nguyên") != std::string::npos || lower_input.find("tay nguyen") != std::string::npos ||
        lower_input.find("nguyên") != std::string::npos || lower_input.find("nguyen") != std::string::npos) {
        ESP_LOGI(TAG, "Detected Tây Nguyên variant: '%s' -> VOV_TAYNGUYEN (volume: %.1fx)", station_name.c_str(), radio_stations_["VOV_TAYNGUYEN"].volume);
        current_station_volume_ = radio_stations_["VOV_TAYNGUYEN"].volume;
        return PlayUrl(radio_stations_["VOV_TAYNGUYEN"].url, radio_stations_["VOV_TAYNGUYEN"].name);
    }
    
    // Handle mispronunciations of VOV1 - various phonetic variations
    if (lower_input.find("vov") != std::string::npos) {
        // Check common mispronunciations of VOV1
        if (lower_input.find("mộc") != std::string::npos || lower_input.find("mốc") != std::string::npos ||
            lower_input.find("mốt") != std::string::npos || lower_input.find("máu") != std::string::npos ||
            lower_input.find("một") != std::string::npos || lower_input.find("mút") != std::string::npos ||
            lower_input.find("mót") != std::string::npos || lower_input.find("mục") != std::string::npos ||
            lower_input.find("1") != std::string::npos || lower_input.find("một") != std::string::npos) {
            ESP_LOGI(TAG, "Detected VOV1 phonetic variant: '%s' -> VOV1 (volume: %.1fx)", station_name.c_str(), radio_stations_["VOV1"].volume);
            current_station_volume_ = radio_stations_["VOV1"].volume;
            return PlayUrl(radio_stations_["VOV1"].url, radio_stations_["VOV1"].name);
        }
    }
    
    // Last resort: try partial matching with keywords from station names
    std::vector<std::string> keywords = {"tiếng nói", "việt nam", "giao thông", "mê kông", "miền trung", "tây bắc", "đông bắc", "tây nguyên", "tay nguyen", "nguyên", "nguyen"};
    for (const std::string& keyword : keywords) {
        if (lower_input.find(keyword) != std::string::npos) {
            for (const auto& station : radio_stations_) {
                std::string lower_station_name = station.second.name;
                std::transform(lower_station_name.begin(), lower_station_name.end(), lower_station_name.begin(), ::tolower);
                
                if (lower_station_name.find(keyword) != std::string::npos) {
                    ESP_LOGI(TAG, "Found station by keyword '%s': '%s' -> %s (volume: %.1fx)", keyword.c_str(), station_name.c_str(), station.second.name.c_str(), station.second.volume);
                    current_station_volume_ = station.second.volume;
                    return PlayUrl(station.second.url, station.second.name);
                }
            }
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
    
    // Stop previous playback
    Stop();
	
	// --- CLEAN DISPLAY RAM BEFORE STARTING RADIO ---
	auto display = Board::GetInstance().GetDisplay();
	if (display) {
		display->StopFFT();                 // Dừng FFT canvas cũ (nếu có)
		display->ReleaseAudioBuffFFT();    // Giải phóng buffer FFT
		display->SetMusicInfo(nullptr);    // Xóa thông tin nhạc cũ
		ESP_LOGI(TAG, "[PATCH] Display memory released before starting radio");
	}
    
    // Set current station information
    current_station_url_ = radio_url;
    current_station_name_ = station_name.empty() ? "Custom Radio" : station_name;
    station_name_displayed_ = false;
    
    // If current_station_volume_ wasn't set by PlayStation(), use default volume
    if (current_station_volume_ <= 0.0f) {
        current_station_volume_ = 4.5f;  // Default volume for custom URLs
    }
    
    // Clear the buffer
    ClearAudioBuffer();
    
    // Configure thread stack size
    esp_pthread_cfg_t cfg = esp_pthread_get_default_config();
    cfg.stack_size = 1024 * 3 + 512;  // 3.5KB stack size
    cfg.prio = 5;           // Medium priority
    cfg.thread_name = "radio_stream";
    esp_pthread_set_cfg(&cfg);
    
    // Start download thread
    is_downloading_ = true;
    download_thread_ = std::thread(&Esp32Radio::DownloadRadioStream, this, radio_url);
    
    // Start playback thread
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

    // Reset the sample rate to the original value
    ResetSampleRate();
    
    // Check if there is any streaming in progress
    if (!is_playing_ && !is_downloading_) {
        ESP_LOGW(TAG, "No radio streaming in progress");
        return true;
    }
    
    // Stop download and playback flags
    is_downloading_ = false;
    is_playing_ = false;
    
    // Clear the station name display
    auto& board = Board::GetInstance();
    auto display = board.GetDisplay();
    if (display) {
        display->SetMusicInfo("");  // Clear the display
        ESP_LOGI(TAG, "Cleared radio station display");
    }
    
    // Notify all waiting threads
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        buffer_cv_.notify_all();
    }
    
    // Wait for threads to finish
    if (download_thread_.joinable()) {
        download_thread_.join();
        ESP_LOGI(TAG, "Download thread joined in Stop");
    }
    
    if (play_thread_.joinable()) {
        play_thread_.join();
        ESP_LOGI(TAG, "Play thread joined in Stop");
    }
    
    // Stop FFT display
    if (display && display_mode_ == DISPLAY_MODE_SPECTRUM) {
        display->StopFFT();
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

    if (radio_url.empty() || radio_url.find("http") != 0) {
        ESP_LOGE(TAG, "Invalid URL format: %s", radio_url.c_str());
        is_downloading_ = false;
        return;
    }

    auto network = Board::GetInstance().GetNetwork();
    auto http = network->CreateHttp(0);

    http->SetHeader("User-Agent", "ESP32-Music-Player/1.0");
    http->SetHeader("Accept", "*/*");
    http->SetHeader("Range", "bytes=0-");

    bool is_https = (radio_url.find("https://") == 0);
    ESP_LOGI(TAG, "Connecting to %s stream: %s", is_https ? "HTTPS" : "HTTP", radio_url.c_str());

    auto display = Board::GetInstance().GetDisplay();

    if (!http->Open("GET", radio_url)) {
        ESP_LOGE(TAG, "Failed to connect to radio stream URL: %s", radio_url.c_str());
        is_downloading_ = false;
        if (display) display->SetMusicInfo("Radio connection error");
        return;
    }

    int status_code = http->GetStatusCode();
    if (status_code >= 300 && status_code < 400) {
        ESP_LOGW(TAG, "HTTP %d redirect detected but cannot follow", status_code);
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

    const size_t chunk_size = 4096;
    char* buffer = new char[chunk_size];
    size_t total_downloaded = 0;
    size_t total_print_bytes = 0;
    const int kMaxReconnectAttempts = 3;
    int reconnect_attempts = 0;

    while (is_downloading_ && is_playing_) {
        int bytes_read = http->Read(buffer, chunk_size);

        // ---- HANDLE READ ERRORS & RECONNECT ----
        if (bytes_read < 0 || bytes_read == 0) {
            reconnect_attempts++;
            ESP_LOGW(TAG, "Stream lost (bytes_read=%d), trying reconnect (%d/%d)...", bytes_read, reconnect_attempts, kMaxReconnectAttempts);

            if (display) {
                display->SetMusicInfo("🔌 Mất kết nối radio...\n⟳ Đang thử lại...");
            }

            if (reconnect_attempts > kMaxReconnectAttempts) {
                ESP_LOGE(TAG, "Exceeded max reconnect attempts");
                break;
            }

            vTaskDelay(pdMS_TO_TICKS(1500));  // Wait 1.5s
            http->Close();

            if (!http->Open("GET", radio_url)) {
                ESP_LOGE(TAG, "Reconnect failed at attempt %d", reconnect_attempts);
                continue;
            } else {
                ESP_LOGI(TAG, "Reconnect success at attempt %d", reconnect_attempts);
                continue;
            }
        }

        reconnect_attempts = 0;

        if (bytes_read < 16) {
            ESP_LOGI(TAG, "Data chunk too small: %d bytes", bytes_read);
        }

        if (total_downloaded == 0 && bytes_read >= 4) {
            if (memcmp(buffer, "ID3", 3) == 0) {
                ESP_LOGI(TAG, "Detected MP3 with ID3 tag");
            } else if ((buffer[0] & 0xFF) == 0xFF && (buffer[1] & 0xE0) == 0xE0) {
                ESP_LOGI(TAG, "Detected MP3 file header");
            } else if (memcmp(buffer, "RIFF", 4) == 0) {
                ESP_LOGI(TAG, "Detected WAV");
            } else if (memcmp(buffer, "fLaC", 4) == 0) {
                ESP_LOGI(TAG, "Detected FLAC");
            } else if (memcmp(buffer, "OggS", 4) == 0) {
                ESP_LOGI(TAG, "Detected OGG");
            } else {
                ESP_LOGI(TAG, "Unknown format, first 4 bytes: %02X %02X %02X %02X",
                         (unsigned char)buffer[0], (unsigned char)buffer[1],
                         (unsigned char)buffer[2], (unsigned char)buffer[3]);
            }
        }

        uint8_t* chunk_data = (uint8_t*)heap_caps_malloc(bytes_read, MALLOC_CAP_SPIRAM);
        if (!chunk_data) {
            ESP_LOGE(TAG, "Failed to allocate memory for radio chunk");
            break;
        }
        memcpy(chunk_data, buffer, bytes_read);

        {
            std::unique_lock<std::mutex> lock(buffer_mutex_);
            buffer_cv_.wait(lock, [this] { return buffer_size_ < MAX_BUFFER_SIZE || !is_downloading_; });

            if (is_downloading_) {
                audio_buffer_.push(RadioAudioChunk(chunk_data, bytes_read));
                buffer_size_ += bytes_read;
                total_downloaded += bytes_read;
                total_print_bytes += bytes_read;
                buffer_cv_.notify_one();

                if (total_print_bytes >= (128 * 1024)) {
                    total_print_bytes = 0;
                    ESP_LOGI(TAG, "Downloaded %d bytes, buffer size: %d", total_downloaded, buffer_size_);
                }
            } else {
                heap_caps_free(chunk_data);
                break;
            }
        }
    }

    delete[] buffer;
    http->Close();

    if (is_downloading_) {
        ESP_LOGI(TAG, "Radio stream download completed");
    } else {
        ESP_LOGI(TAG, "Radio stream download stopped by user");
    }

    is_downloading_ = false;

    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        buffer_cv_.notify_all();
    }

    if (total_downloaded < 1024 && display) {
        display->SetMusicInfo("❌ Không thể kết nối radio.");
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

    // Ensure audio output is enabled
    if (!codec->output_enabled()) {
        codec->EnableOutput(true);
    }
    
    // Initialize AAC decoder
    if (!InitializeAacDecoder()) {
        ESP_LOGE(TAG, "Failed to initialize AAC decoder for VOV streams");
        is_playing_ = false;
        return;
    }
    
    // Wait for the buffer to have enough data to start playback
    {
        std::unique_lock<std::mutex> lock(buffer_mutex_);
        buffer_cv_.wait(lock, [this] { 
            return buffer_size_ >= MIN_BUFFER_SIZE || (!is_downloading_ && !audio_buffer_.empty()); 
        });
    }
    
    ESP_LOGI(TAG, "Starting radio playback with buffer size: %d", buffer_size_);
    
    size_t total_played_bytes = 0;
    size_t total_print_bytes = 0;
    uint8_t* input_buffer = nullptr;
    int bytes_left = 0;
    uint8_t* read_ptr = nullptr;
    
    // Allocate input buffer (for both MP3 and AAC)
    input_buffer = (uint8_t*)heap_caps_malloc(4096, MALLOC_CAP_SPIRAM);
    if (!input_buffer) {
        ESP_LOGE(TAG, "Failed to allocate input buffer");
        is_playing_ = false;
        return;
    }

    auto& board = Board::GetInstance();
    auto display = board.GetDisplay();
    
    while (is_playing_) {
        // Check device state, only play radio when idle
        auto& app = Application::GetInstance();
        DeviceState current_state = app.GetDeviceState();
        
        if (current_state == kDeviceStateListening || current_state == kDeviceStateSpeaking) {
            if (current_state == kDeviceStateSpeaking) {
                ESP_LOGI(TAG, "Device is in speaking state, switching to listening state for radio playback");
            }
            if (current_state == kDeviceStateListening) {
                ESP_LOGI(TAG, "Device is in listening state, switching to idle state for radio playback");
            }
            // Switch state
            app.ToggleChatState(); // Change to idle state
            vTaskDelay(pdMS_TO_TICKS(300));
            continue;
        } else if (current_state != kDeviceStateIdle) {
            ESP_LOGD(TAG, "Device state is %d, pausing radio playback", current_state);
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

		// Display radio station name
		if (!station_name_displayed_ && !current_station_name_.empty()) {

			// Start appropriate display functionality based on display mode
			if (display) {
				if (display_mode_ == DISPLAY_MODE_SPECTRUM) {
					display->StartFFT();
					ESP_LOGI(TAG, "Display StartFFT() called for spectrum visualization");
				} else {
					ESP_LOGI(TAG, "Info display mode active, FFT visualization disabled");
				}
			}

			if (display) {
				std::string formatted_station_name =
					"Radio 《" + current_station_name_ + "》Đang phát...";
				display->SetMusicInfo(formatted_station_name.c_str());
				ESP_LOGI(TAG, "Displaying radio station: %s", formatted_station_name.c_str());
				station_name_displayed_ = true;
			}
		}
								
        // If more audio data is needed, read from the buffer
        if (bytes_left < 4096) {
            RadioAudioChunk chunk;
            
            // Retrieve audio data from the buffer
            {
                std::unique_lock<std::mutex> lock(buffer_mutex_);
                if (audio_buffer_.empty()) {
                    if (!is_downloading_) {
                        ESP_LOGI(TAG, "Radio stream ended, total played: %d bytes", total_played_bytes);
                        break;
                    }
                    // Wait for new data
                    buffer_cv_.wait(lock, [this] { return !audio_buffer_.empty() || !is_downloading_; });
                    if (audio_buffer_.empty()) {
                        continue;
                    }
                }
                
                chunk = audio_buffer_.front();
                audio_buffer_.pop();
                buffer_size_ -= chunk.size;
                total_played_bytes += chunk.size;
                total_print_bytes += chunk.size;
                
                // Notify download thread that buffer has space
                buffer_cv_.notify_one();
            }
            
            // Add new data to the input buffer
            if (chunk.data && chunk.size > 0) {
                // Move remaining data to the beginning of the buffer
                if (bytes_left > 0 && read_ptr != input_buffer) {
                    memmove(input_buffer, read_ptr, bytes_left);
                }
                
                // Check buffer space
                size_t space_available = 8192 - bytes_left;
                size_t copy_size = std::min(chunk.size, space_available);
                
                // Copy new data
                memcpy(input_buffer + bytes_left, chunk.data, copy_size);
                bytes_left += copy_size;
                read_ptr = input_buffer;
                
                // AAC streams don't need ID3 tag processing
                
                // Free chunk memory
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
							 aac_info_.sample_rate,
							 aac_info_.bits_per_sample,
							 aac_info_.channel);

					// ===============================
					//   HIỂN THỊ THÔNG TIN AAC LÊN LCD
					// ===============================
					auto& board = Board::GetInstance();
					auto display = board.GetDisplay();

					if (display) {
						std::ostringstream oss;

						oss << "RADIO 《" << current_station_name_ << "》\n"
							<< "AAC " << aac_info_.sample_rate << "Hz  "
							<< aac_info_.bits_per_sample << "bit  "
							<< aac_info_.channel << "ch";

						display->SetMusicInfo(oss.str().c_str());

						ESP_LOGI(TAG, "Displayed AAC info on LCD: %s", oss.str().c_str());
					}
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
                
                // Amplify audio using station-specific volume setting
                std::vector<int16_t> amplified_buffer(final_sample_count);
                const float amplification_factor = current_station_volume_; // Station-specific volume
                
                for (int i = 0; i < final_sample_count; ++i) {
                    int32_t amplified_sample = (int32_t)(final_pcm_data[i] * amplification_factor);
                    // Clamp to prevent overflow
                    if (amplified_sample > INT16_MAX) {
                        amplified_sample = INT16_MAX;
                    } else if (amplified_sample < INT16_MIN) {
                        amplified_sample = INT16_MIN;
                    }
                    amplified_buffer[i] = (int16_t)amplified_sample;
                }
                
                // Create AudioStreamPacket with amplified audio
                AudioStreamPacket packet;
                packet.sample_rate = aac_info_.sample_rate;
                packet.frame_duration = 60;
                packet.timestamp = 0;
                
                size_t pcm_size_bytes = final_sample_count * sizeof(int16_t);
                packet.payload.resize(pcm_size_bytes);
                memcpy(packet.payload.data(), amplified_buffer.data(), pcm_size_bytes);

                if (display) {
                    if (display_mode_ == DISPLAY_MODE_SPECTRUM) {
                        // Create or update FFT audio data buffer
                        final_pcm_data_fft = display->MakeAudioBuffFFT(pcm_size_bytes);

                        // Copy amplified data to FFT buffer
                        display->FeedAudioDataFFT(amplified_buffer.data(), pcm_size_bytes);
                    }
                }

                app.AddAudioData(std::move(packet));
                
                if (total_print_bytes >= (128 * 1024)) {
                    total_print_bytes = 0;
                    ESP_LOGI(TAG, "AAC: Played %d bytes, buffer size: %d", total_played_bytes, buffer_size_);
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
    
    // Cleanup
    if (input_buffer) {
        heap_caps_free(input_buffer);
    }
    
    if (is_playing_) {
        ESP_LOGI(TAG, "Radio stream playback finished successfully");
        ClearAudioBuffer();  // Clear remaining buffer data
        // Reset the sample rate to the original value
        ResetSampleRate();
    } else {
        ESP_LOGI(TAG, "Radio stream playback stopped by user");
    }
    
    // Cleanup AAC decoder
    CleanupAacDecoder();

    ESP_LOGI(TAG, "Radio stream playback finished, total played: %d bytes", total_played_bytes);
    is_playing_ = false;
    
    // Stop FFT display
    if (display_mode_ == DISPLAY_MODE_SPECTRUM) {
        if (display) {
            display->StopFFT();
            display->ReleaseAudioBuffFFT();
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
        ESP_LOGI(TAG, "Resetting sample rate: from %d Hz back to original value %d Hz", 
                codec->output_sample_rate(), codec->original_output_sample_rate());
        if (codec->SetOutputSampleRate(-1)) {
            ESP_LOGI(TAG, "Successfully reset sample rate to original value: %d Hz", codec->output_sample_rate());
        } else {
            ESP_LOGW(TAG, "Failed to reset sample rate to original value");
        }
    }
}

size_t Esp32Radio::SkipId3Tag(uint8_t* data, size_t size) {
    if (!data || size < 10) {
        return 0;
    }
    
    // Check for ID3v2 tag header "ID3"
    if (memcmp(data, "ID3", 3) != 0) {
        return 0;
    }
    
    // Calculate tag size (synchsafe integer format)
    uint32_t tag_size = ((uint32_t)(data[6] & 0x7F) << 21) |
                        ((uint32_t)(data[7] & 0x7F) << 14) |
                        ((uint32_t)(data[8] & 0x7F) << 7)  |
                        ((uint32_t)(data[9] & 0x7F));
    
    // ID3v2 header (10 bytes) + tag content
    size_t total_skip = 10 + tag_size;
    
    // Ensure it does not exceed the available data size
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