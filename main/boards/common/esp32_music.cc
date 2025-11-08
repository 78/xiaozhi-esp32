#include "esp32_music.h"
#include "board.h"
#include "system_info.h"
#include "audio/audio_codec.h"
#include "application.h"
#include "protocols/protocol.h"
#include "display/display.h"

#include <esp_log.h>
#include <esp_heap_caps.h>
#include <esp_pthread.h>
#include <cJSON.h>
#include <cstring>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <thread>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "Esp32Music"

// URL encoding function
static std::string url_encode(const std::string& str) {
    std::string encoded;
    char hex[4];
    
    for (size_t i = 0; i < str.length(); i++) {
        unsigned char c = str[i];
        
        if ((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += c;
        } else if (c == ' ') {
            encoded += '+';  // Encode space as '+' or '%20'
        } else {
            snprintf(hex, sizeof(hex), "%%%02X", c);
            encoded += hex;
        }
    }
    return encoded;
}



// AudioChunkPool implementation
std::unique_ptr<uint8_t[], void(*)(void*)> AudioChunkPool::acquire(size_t size) {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    if (!free_chunks_.empty() && size <= CHUNK_SIZE) {
        auto chunk = std::move(free_chunks_.front());
        free_chunks_.pop();
        return chunk;
    }
    
    // Allocate new chunk
    uint8_t* raw_ptr = static_cast<uint8_t*>(heap_caps_malloc(size, MALLOC_CAP_SPIRAM));
    if (!raw_ptr) {
        ESP_LOGE(TAG, "Failed to allocate audio chunk of size %zu", size);
        return std::unique_ptr<uint8_t[], void(*)(void*)>(nullptr, heap_caps_free);
    }
    
    return std::unique_ptr<uint8_t[], void(*)(void*)>(raw_ptr, heap_caps_free);
}

void AudioChunkPool::release(std::unique_ptr<uint8_t[], void(*)(void*)> chunk) {
    if (!chunk) return;
    
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    if (free_chunks_.size() < MAX_POOL_SIZE) {
        free_chunks_.push(std::move(chunk));
    }
    // else: chunk will be automatically freed when unique_ptr goes out of scope
}

void AudioChunkPool::clear() {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    while (!free_chunks_.empty()) {
        free_chunks_.pop();
    }
}

Esp32Music::Esp32Music() 
    : app_(Application::GetInstance())
    , song_name_displayed_(false)
    , current_lyric_index_(-1)
    , is_lyric_running_(false)
    , display_mode_(DISPLAY_MODE_LYRICS)
    , is_playing_(false)
    , is_downloading_(false)
    , player_state_(PlayerState::IDLE)
    , current_play_time_ms_(0)
    , total_frames_decoded_(0)
    , buffer_size_(0)
    , mp3_decoder_(nullptr)
    , mp3_decoder_initialized_(false) {
    ESP_LOGI(TAG, "Music player initialized with streaming state management");
    InitializeMp3Decoder();
}

Esp32Music::~Esp32Music() {
    ESP_LOGI(TAG, "Destroying music player - stopping all operations");
    
    // Stop all operations
    is_downloading_ = false;
    is_playing_ = false;
    is_lyric_running_ = false;
    
    // Notify all waiting threads
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        buffer_cv_.notify_all();
    }
    
    // Use helper function to stop threads safely
    StopThreadSafely(download_thread_, is_downloading_, "download", 5000);
    StopThreadSafely(play_thread_, is_playing_, "playback", 3000);
    StopThreadSafely(lyric_thread_, is_lyric_running_, "lyric", 2000);
    
    // Clean up buffer and MP3 decoder
    ClearAudioBuffer();
    CleanupMp3Decoder();
    chunk_pool_.clear();
    
    player_state_ = PlayerState::IDLE;
    ESP_LOGI(TAG, "Music player destroyed successfully");
}

// Helper function to stop threads safely
void Esp32Music::StopThreadSafely(std::thread& thread, std::atomic<bool>& flag, 
                                  const char* thread_name, int timeout_ms) {
    if (!thread.joinable()) {
        return;
    }
    
    ESP_LOGI(TAG, "Stopping %s thread (timeout: %dms)", thread_name, timeout_ms);
    
    flag = false;
    
    // Notify condition variable
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        buffer_cv_.notify_all();
    }
    
    // Try to join with timeout
    auto start = std::chrono::steady_clock::now();
    bool joined = false;
    
    while (!joined && std::chrono::duration_cast<std::chrono::milliseconds>(
           std::chrono::steady_clock::now() - start).count() < timeout_ms) {
        
        if (!thread.joinable()) {
            joined = true;
            break;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        // Notify again
        {
            std::lock_guard<std::mutex> lock(buffer_mutex_);
            buffer_cv_.notify_all();
        }
    }
    
    if (thread.joinable()) {
        if (joined) {
            thread.join();
            ESP_LOGI(TAG, "%s thread stopped successfully", thread_name);
        } else {
            ESP_LOGW(TAG, "%s thread timeout, detaching (potential resource leak)", thread_name);
            thread.detach();
        }
    }
}

bool Esp32Music::Download(const std::string& song_name, const std::string& artist_name) {
    ESP_LOGI(TAG, "Starting to get music details for: %s", song_name.c_str());
    
    auto network = Board::GetInstance().GetNetwork();
    if (!network) {
        ESP_LOGE(TAG, "Network interface not available");
        return false;
    }
    
    const std::string base_streaming_url = "http://music.iotforce.io.vn:8080";
    
    last_downloaded_data_.clear();
    current_song_name_ = song_name;
    
    // Use loop instead of recursion to avoid stack overflow
    const int max_processing_retries = 3;
    for (int retry = 0; retry <= max_processing_retries; retry++) {
        if (retry > 0) {
            ESP_LOGI(TAG, "Retry attempt %d/%d for processing song", retry, max_processing_retries);
            vTaskDelay(pdMS_TO_TICKS(3000));
        }
        
        std::string full_url = base_streaming_url + "/stream_pcm?query=" + url_encode(song_name);
        ESP_LOGI(TAG, "Request URL: %s", full_url.c_str());
        
        int status_code = -1;
        if (!FetchMusicMetadata(full_url, status_code)) {
            // Don't retry if status is 404 (not found)
            if (status_code == 404) {
                ESP_LOGE(TAG, "Song not found (404), will not retry");
                return false;
            }
            if (retry < max_processing_retries) {
                continue;  // Retry on network error
            }
            return false;
        }
        
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d", status_code, last_downloaded_data_.length());
        
        if (last_downloaded_data_.empty()) {
            ESP_LOGE(TAG, "Empty response from music API");
            if (retry < max_processing_retries) {
                continue;
            }
            return false;
        }
        
        cJSON* response_json = cJSON_Parse(last_downloaded_data_.c_str());
        if (!response_json) {
            ESP_LOGE(TAG, "Failed to parse JSON response");
            if (retry < max_processing_retries) {
                continue;
            }
            return false;
        }
        
        cJSON* status = cJSON_GetObjectItem(response_json, "status");
        cJSON* artist = cJSON_GetObjectItem(response_json, "artist");
        cJSON* title = cJSON_GetObjectItem(response_json, "title");
        cJSON* audio_url = cJSON_GetObjectItem(response_json, "audio_url");
        cJSON* lyric_url = cJSON_GetObjectItem(response_json, "lyric_url");
        cJSON* message = cJSON_GetObjectItem(response_json, "message");
        
        if (cJSON_IsString(artist)) ESP_LOGI(TAG, "Artist: %s", artist->valuestring);
        if (cJSON_IsString(title)) ESP_LOGI(TAG, "Title: %s", title->valuestring);
        if (cJSON_IsString(message)) ESP_LOGI(TAG, "Message: %s", message->valuestring);
        
        // Update current song name with the title from response
        if (cJSON_IsString(title) && title->valuestring && strlen(title->valuestring) > 0) {
            current_song_name_ = title->valuestring;
            ESP_LOGI(TAG, "Updated song name from response: %s", current_song_name_.c_str());
        }
        
        bool result = false;
        bool should_retry = false;
        
        if (cJSON_IsString(status)) {
            std::string status_str = status->valuestring;
            if (!HandleMusicStatus(status_str, song_name, artist_name)) {
                cJSON_Delete(response_json);
                return false;
            }
        }
        
        if (should_retry) {
            cJSON_Delete(response_json);
            if (retry < max_processing_retries) {
                continue;
            } else {
                ESP_LOGE(TAG, "Song processing timeout after %d retries", max_processing_retries);
                return false;
            }
        }
        
        if (cJSON_IsString(audio_url) && audio_url->valuestring && strlen(audio_url->valuestring) > 0) {
            result = ProcessAudioUrl(audio_url->valuestring, song_name);
            
            if (result && cJSON_IsString(lyric_url) && lyric_url->valuestring && strlen(lyric_url->valuestring) > 0) {
                ProcessLyricUrl(lyric_url->valuestring, song_name);
            } else if (result) {
                ESP_LOGW(TAG, "No lyric URL found for this song");
            }
        } else {
            ESP_LOGE(TAG, "Audio URL not found or empty for song: %s", song_name.c_str());
        }
        
        cJSON_Delete(response_json);
        return result;
    }
    
    return false;
}

bool Esp32Music::FetchMusicMetadata(const std::string& url, int& status_code) {
    auto network = Board::GetInstance().GetNetwork();
    auto http = network->CreateHttp(0);
    http->SetHeader("User-Agent", "ESP32-Music-Player/1.0");
    http->SetHeader("Accept", "application/json");
    http->SetHeader("Connection", "close");
    
    if (!http->Open("GET", url)) {
        ESP_LOGE(TAG, "Failed to connect to music API");
        return false;
    }
    
    status_code = http->GetStatusCode();
    if (status_code != 200) {
        ESP_LOGE(TAG, "HTTP GET failed with status code: %d", status_code);
        http->Close();
        return false;
    }
    
    last_downloaded_data_ = http->ReadAll();
    http->Close();
    return true;
}

bool Esp32Music::HandleMusicStatus(const std::string& status, const std::string& song_name, const std::string& artist_name) {
    if (status != "success") {
        ESP_LOGE(TAG, "Server error processing song: %s", song_name.c_str());
        return false;
    }

    ESP_LOGI(TAG, "Song is ready for download");
    
    return true;
}

bool Esp32Music::ProcessAudioUrl(const std::string& audio_url, const std::string& song_name) {
    ESP_LOGI(TAG, "Audio URL: %s", audio_url.c_str());
    
    current_music_url_ = audio_url;
    
    ESP_LOGI(TAG, "Starting streaming playback for: %s", song_name.c_str());
    song_name_displayed_ = false;
    StartStreaming(current_music_url_);
    
    return true;
}

void Esp32Music::ProcessLyricUrl(const std::string& lyric_url, const std::string& song_name) {
    current_lyric_url_ = lyric_url;
    
    if (display_mode_ == DISPLAY_MODE_LYRICS) {
        ESP_LOGI(TAG, "Loading lyrics for: %s (lyrics display mode)", song_name.c_str());
        
        // Stop previous lyric thread safely
        if (is_lyric_running_) {
            is_lyric_running_ = false;
            if (lyric_thread_.joinable()) {
                lyric_thread_.join();
            }
        }
        
        // Clear previous lyrics
        {
            std::unique_lock<std::shared_mutex> lock(lyrics_mutex_);
            lyrics_.clear();
        }
        
        current_lyric_index_ = -1;
        is_lyric_running_ = true;
        
        // Start lyric thread with delay to ensure music is stable
        lyric_thread_ = std::thread([this]() {
            // Wait a bit for music to stabilize
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            LyricDisplayThread();
        });
    } else {
        ESP_LOGI(TAG, "Lyric URL found but spectrum display mode is active, skipping lyrics");
    }
}


std::string Esp32Music::GetDownloadResult() {
    return last_downloaded_data_;
}

// Start streaming playback
bool Esp32Music::StartStreaming(const std::string& music_url) {
    if (music_url.empty()) {
        ESP_LOGE(TAG, "Music URL is empty");
        player_state_ = PlayerState::ERROR;
        return false;
    }
    
    ESP_LOGD(TAG, "Starting streaming for URL: %s", music_url.c_str());
    
    // Check current state
    PlayerState expected = PlayerState::IDLE;
    if (!player_state_.compare_exchange_strong(expected, PlayerState::ACTIVE)) {
        ESP_LOGW(TAG, "Cannot start streaming, player not in IDLE state");
        return false;
    }
    
    // Stop previous playback and download
    is_downloading_ = false;
    is_playing_ = false;
    
    // Wait for previous threads to completely finish
    StopThreadSafely(download_thread_, is_downloading_, "previous download", 2000);
    StopThreadSafely(play_thread_, is_playing_, "previous playback", 2000);
    
    // Clear buffer
    ClearAudioBuffer();
    
    is_downloading_ = true;
    is_playing_ = true;
    
    // Configure thread stack size to avoid stack overflow
    esp_pthread_cfg_t cfg = esp_pthread_get_default_config();
    cfg.stack_size = 16384;  // 16KB stack size (increased)
    cfg.prio = 5;           // Medium priority
    cfg.thread_name = "audio_stream";
    esp_pthread_set_cfg(&cfg);
    
    // Start download thread first
    download_thread_ = std::thread(&Esp32Music::DownloadAudioStream, this, music_url);
    
    vTaskDelay(pdMS_TO_TICKS(50));
    
    play_thread_ = std::thread(&Esp32Music::PlayAudioStream, this);
    
    ESP_LOGI(TAG, "Streaming threads started successfully");
    
    return true;
}

// Stop streaming playback
bool Esp32Music::StopStreaming() {
    ESP_LOGI(TAG, "Stopping music streaming - current state: downloading=%d, playing=%d", 
            is_downloading_.load(), is_playing_.load());

    // Reset sample rate to original value
    ResetSampleRate();
    
    // Check if streaming is in progress
    if (!is_playing_ && !is_downloading_) {
        ESP_LOGW(TAG, "No streaming in progress");
        player_state_ = PlayerState::IDLE;
        return true;
    }
    
    // Prevent multiple stop calls
    PlayerState expected = PlayerState::ACTIVE;
    if (!player_state_.compare_exchange_strong(expected, PlayerState::IDLE)) {
        ESP_LOGW(TAG, "Already stopping or stopped");
        return true;
    }
    
    // Stop download and playback flags
    is_downloading_ = false;
    is_playing_ = false;
    
    // Clear song name display
    auto& board = Board::GetInstance();
    auto display = board.GetDisplay();
    if (display) {
        display->SetMusicInfo("");  // Clear song name display
        ESP_LOGI(TAG, "Cleared song name display");
    }
    
    // Notify all waiting threads
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        buffer_cv_.notify_all();
    }
    
    // Use helper function to stop threads safely with shorter timeout
    StopThreadSafely(download_thread_, is_downloading_, "download", 500);
    StopThreadSafely(play_thread_, is_playing_, "playback", 500);
    
    // After threads completely finish, only stop FFT display in spectrum mode
    if (display && display_mode_ == DISPLAY_MODE_SPECTRUM) {
        display->stopFft();
        ESP_LOGI(TAG, "Stopped FFT display in StopStreaming (spectrum mode)");
    } else if (display) {
        ESP_LOGI(TAG, "Not in spectrum mode, skipping FFT stop in StopStreaming");
    }
    
    player_state_ = PlayerState::IDLE;
    ESP_LOGI(TAG, "Music streaming stopped");
    return true;
}

// Stream download audio data
void Esp32Music::DownloadAudioStream(const std::string& music_url) {
    ESP_LOGD(TAG, "Starting audio stream download from: %s", music_url.c_str());
    
    // Validate URL
    if (music_url.empty() || music_url.find("http") != 0) {
        ESP_LOGE(TAG, "Invalid URL format: %s", music_url.c_str());
        is_downloading_ = false;
        player_state_ = PlayerState::ERROR;
        return;
    }
    
    auto network = Board::GetInstance().GetNetwork();
    auto http = network->CreateHttp(0);
    
    http->SetTimeout(30000);
    
    http->SetHeader("User-Agent", "ESP32-AudioPlayer/1.0");
    http->SetHeader("Accept", "*/*");
    http->SetHeader("Connection", "close");    
    ESP_LOGI(TAG, "Attempting to connect to music stream...");
    if (!http->Open("GET", music_url)) {
        ESP_LOGE(TAG, "Failed to connect to music stream URL: %s", music_url.c_str());
        ESP_LOGE(TAG, "Possible causes: network connectivity, DNS resolution, or server unavailable");
        is_downloading_ = false;
        player_state_ = PlayerState::ERROR;
        return;
    }
    
    app_.SetDeviceState(kDeviceStateStreaming);
    
    int status_code = http->GetStatusCode();
    ESP_LOGI(TAG, "Music stream HTTP status: %d", status_code);
    
    if (status_code != 200 && status_code != 206) {  // 206 for partial content
        ESP_LOGE(TAG, "HTTP GET failed with status code: %d", status_code);
        ESP_LOGE(TAG, "Expected 200 (OK) or 206 (Partial Content), got %d", status_code);
        http->Close();
        is_downloading_ = false;
        player_state_ = PlayerState::ERROR;
        return;
    }
    
    ESP_LOGI(TAG, "Started downloading audio stream, status: %d", status_code);
    
    // Read audio data in chunks - use SPIRAM to avoid stack overflow
    const size_t chunk_size = CHUNK_SIZE;  // 8KB per chunk
    char* buffer = (char*)heap_caps_malloc(chunk_size, MALLOC_CAP_SPIRAM);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate download buffer from SPIRAM");
        http->Close();
        is_downloading_ = false;
        return;
    }
    
    size_t total_downloaded = 0;
    int consecutive_errors = 0;
    const int max_consecutive_errors = 3;
    
    while (is_downloading_ && is_playing_) {
        int bytes_read = http->Read(buffer, chunk_size);
        if (bytes_read < 0) {
            consecutive_errors++;
            ESP_LOGE(TAG, "Failed to read audio data: error code %d (consecutive errors: %d/%d)", 
                    bytes_read, consecutive_errors, max_consecutive_errors);
            
            if (consecutive_errors >= max_consecutive_errors) {
                ESP_LOGE(TAG, "Too many consecutive read errors, aborting download");
                break;
            }
            
            // Wait a bit before retrying
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        
        // Reset error counter on successful read
        consecutive_errors = 0;
        
        if (bytes_read == 0) {
            ESP_LOGI(TAG, "Audio stream download completed, total: %d bytes", total_downloaded);
            break;
        }
        
        // Print chunk information
        // ESP_LOGI(TAG, "Downloaded chunk: %d bytes at offset %d", bytes_read, total_downloaded);
        
        // Safely print hexadecimal content of chunk (first 16 bytes)
        if (bytes_read >= 16) {
            // ESP_LOGI(TAG, "Data: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X ...", 
            //         (unsigned char)buffer[0], (unsigned char)buffer[1], (unsigned char)buffer[2], (unsigned char)buffer[3],
            //         (unsigned char)buffer[4], (unsigned char)buffer[5], (unsigned char)buffer[6], (unsigned char)buffer[7],
            //         (unsigned char)buffer[8], (unsigned char)buffer[9], (unsigned char)buffer[10], (unsigned char)buffer[11],
            //         (unsigned char)buffer[12], (unsigned char)buffer[13], (unsigned char)buffer[14], (unsigned char)buffer[15]);
        } else {
            ESP_LOGI(TAG, "Data chunk too small: %d bytes", bytes_read);
        }
        
        // Try to detect file format (check file header)
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
        
        // Create audio data chunk using smart pointer - FIX BUG #1: Memory leak
        auto chunk_data = chunk_pool_.acquire(bytes_read);
        if (!chunk_data) {
            ESP_LOGE(TAG, "Failed to allocate memory for audio chunk");
            break;
        }
        memcpy(chunk_data.get(), buffer, bytes_read);
        
        // Wait for buffer space
        {
            std::unique_lock<std::mutex> lock(buffer_mutex_);
            buffer_cv_.wait(lock, [this] { 
                return buffer_size_.load() < MAX_BUFFER_SIZE || !is_downloading_; 
            });
            
            if (!is_downloading_) {
                break;
            }
            
            // Move chunk into buffer - no memory leak possible
            audio_buffer_.push(AudioChunk(chunk_data.release(), bytes_read));
            buffer_size_ += bytes_read;
            total_downloaded += bytes_read;
            
            // Notify playback thread of new data
            buffer_cv_.notify_one();
            
            if (total_downloaded % (256 * 1024) == 0) {  // Print progress every 256KB
                ESP_LOGD(TAG, "Downloaded %d bytes, buffer size: %zu", 
                        total_downloaded, buffer_size_.load());
            }
        }
    }
    
    http->Close();
    heap_caps_free(buffer);  // Free SPIRAM buffer
    
    is_downloading_ = false;
    
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        buffer_cv_.notify_all();
    }
    
    ESP_LOGI(TAG, "Audio stream download thread finished, total downloaded: %zu bytes", total_downloaded);
}

// Stream playback audio data
void Esp32Music::PlayAudioStream() {
    ESP_LOGI(TAG, "Starting audio stream playback");
    
    // Initialize time tracking variables
    current_play_time_ms_ = 0;
    total_frames_decoded_ = 0;
    
    auto codec = Board::GetInstance().GetAudioCodec();
    if (!codec) {
        ESP_LOGE(TAG, "Audio codec not available");
        is_playing_ = false;
        return;
    }
    
    if (!mp3_decoder_initialized_) {
        ESP_LOGE(TAG, "MP3 decoder not initialized");
        is_playing_ = false;
        return;
    }
    
    
    {
        std::unique_lock<std::mutex> lock(buffer_mutex_);
        buffer_cv_.wait(lock, [this] { 
            return buffer_size_.load() >= MIN_BUFFER_SIZE || 
                   (!is_downloading_ && !audio_buffer_.empty()); 
        });
    }
    
    ESP_LOGI(TAG, "XiaoZhi Open Source Music Firmware QQ Group: 826072986");
    ESP_LOGI(TAG, "Starting playback with buffer size: %zu", buffer_size_.load());
    
    size_t total_played = 0;
    
    // Use class constant for MP3 buffer size
    auto mp3_input_buffer = chunk_pool_.acquire(MP3_BUFFER_SIZE);
    if (!mp3_input_buffer) {
        ESP_LOGE(TAG, "Failed to allocate MP3 input buffer");
        is_playing_ = false;
        player_state_ = PlayerState::ERROR;
        return;
    }
    
    int bytes_left = 0;
    uint8_t* read_ptr = mp3_input_buffer.get();
    
    // Mark whether ID3 tag has been processed
    bool id3_processed = false;
    
    while (is_playing_) {
        vTaskDelay(pdMS_TO_TICKS(10));
        

        
        if (!is_downloading_.load() && audio_buffer_.empty()) {
            ESP_LOGI(TAG, "Playback finished: download complete, buffer empty, bytes_left: %d", bytes_left);
            is_playing_ = false;
            break;
        }
        
        // Display current playing song name
        if (!song_name_displayed_ && !current_song_name_.empty()) {
            auto& board = Board::GetInstance();
            auto display = board.GetDisplay();
            if (display) {
                std::string formatted_song_name = "《" + current_song_name_ + "》Playing...";
                display->SetMusicInfo(formatted_song_name.c_str());
                ESP_LOGI(TAG, "Displaying song name: %s", formatted_song_name.c_str());
                song_name_displayed_ = true;
            }

            // Start corresponding display function based on display mode
            if (display) {
                if (display_mode_ == DISPLAY_MODE_SPECTRUM) {
                    display->start();
                    ESP_LOGI(TAG, "Display start() called for spectrum visualization");
                } else {
                    ESP_LOGI(TAG, "Lyrics display mode active, FFT visualization disabled");
                }
            }
        }
        
        if (bytes_left < MP3_DECODE_THRESHOLD) {  // Keep at least 4KB data for decoding
            AudioChunk chunk;
            
            // Get audio data from buffer
            {
                std::unique_lock<std::mutex> lock(buffer_mutex_);
                if (audio_buffer_.empty()) {
                    // Wait for new data with timeout to prevent indefinite blocking
                    buffer_cv_.wait_for(lock, std::chrono::milliseconds(100), [this] { 
                        return !audio_buffer_.empty() || !is_downloading_; 
                    });
                    if (audio_buffer_.empty()) {
                        continue;  // Let the main loop check exit condition
                    }
                }
                
                chunk = std::move(audio_buffer_.front());
                audio_buffer_.pop();
                buffer_size_ -= chunk.size;
                
                // Notify download thread that buffer has space
                buffer_cv_.notify_one();
            }
            
            if (chunk.data && chunk.size > 0) {
                if (bytes_left > 0 && read_ptr != mp3_input_buffer.get()) {
                    memmove(mp3_input_buffer.get(), read_ptr, bytes_left);
                }
                
                if (bytes_left >= static_cast<int>(MP3_BUFFER_SIZE)) {
                    ESP_LOGE(TAG, "MP3 buffer overflow detected: bytes_left=%d >= %zu", 
                            bytes_left, MP3_BUFFER_SIZE);
                    break;
                }
                
                // Check buffer space
                size_t space_available = MP3_BUFFER_SIZE - bytes_left;
                size_t copy_size = std::min(chunk.size, space_available);
                
                // Copy new data
                memcpy(mp3_input_buffer.get() + bytes_left, chunk.data.get(), copy_size);
                bytes_left += copy_size;
                read_ptr = mp3_input_buffer.get();
                
                // Check and skip ID3 tag (only process once at start)
                if (!id3_processed && bytes_left >= 10) {
                    size_t id3_skip = SkipId3Tag(read_ptr, bytes_left);
                    if (id3_skip > 0) {
                        read_ptr += id3_skip;
                        bytes_left -= id3_skip;
                        ESP_LOGI(TAG, "Skipped ID3 tag: %u bytes", (unsigned int)id3_skip);
                    }
                    id3_processed = true;
                }
                
            }
        }
        
        int sync_offset = MP3FindSyncWord(read_ptr, bytes_left);
        if (sync_offset < 0) {
            ESP_LOGW(TAG, "No MP3 sync word found, skipping %d bytes", bytes_left);
            bytes_left = 0;
            continue;
        }
        
        // Skip to sync position
        if (sync_offset > 0) {
            read_ptr += sync_offset;
            bytes_left -= sync_offset;
        }
        
        // Decode MP3 frame
        int16_t pcm_buffer[MAX_PCM_SAMPLES];
        int decode_result = MP3Decode(mp3_decoder_, &read_ptr, &bytes_left, pcm_buffer, 0);
        
        if (decode_result == 0) {
            // Decode successful, get frame info
            MP3GetLastFrameInfo(mp3_decoder_, &mp3_frame_info_);
            total_frames_decoded_++;
            
            // Basic frame info validity check to prevent division by zero
            if (mp3_frame_info_.samprate == 0 || mp3_frame_info_.nChans == 0) {
                ESP_LOGW(TAG, "Invalid frame info: rate=%d, channels=%d, skipping", 
                        mp3_frame_info_.samprate, mp3_frame_info_.nChans);
                continue;
            }
            
            // Calculate current frame duration (milliseconds)
            int frame_duration_ms = (mp3_frame_info_.outputSamps * 1000) / 
                                  (mp3_frame_info_.samprate * mp3_frame_info_.nChans);
            
            // Update current playback time
            current_play_time_ms_ += frame_duration_ms;
            
            ESP_LOGD(TAG, "Frame %d: time=%lldms, duration=%dms, rate=%d, ch=%d", 
                    total_frames_decoded_, current_play_time_ms_, frame_duration_ms,
                    mp3_frame_info_.samprate, mp3_frame_info_.nChans);
            
            // Update lyric display
            UpdateLyricDisplay(current_play_time_ms_ + BUFFER_LATENCY_MS);
            
            // Send PCM data to Application's audio decode queue
            if (mp3_frame_info_.outputSamps > 0) {
                int16_t* final_pcm_data = pcm_buffer;
                int final_sample_count = mp3_frame_info_.outputSamps;
                std::vector<int16_t> mono_buffer;
                
                // If stereo, convert to mono mix
                if (mp3_frame_info_.nChans == 2) {
                    // Stereo to mono: mix left and right channels
                    int stereo_samples = mp3_frame_info_.outputSamps;  // Total samples including left and right channels
                    int mono_samples = stereo_samples / 2;  // Actual mono sample count
                    
                    mono_buffer.resize(mono_samples);
                    
                    for (int i = 0; i < mono_samples; ++i) {
                        // Mix left and right channels (L + R) / 2
                        int left = pcm_buffer[i * 2];      // Left channel
                        int right = pcm_buffer[i * 2 + 1]; // Right channel
                        mono_buffer[i] = (int16_t)((left + right) / 2);
                    }
                    
                    final_pcm_data = mono_buffer.data();
                    final_sample_count = mono_samples;

                    ESP_LOGD(TAG, "Converted stereo to mono: %d -> %d samples", 
                            stereo_samples, mono_samples);
                } else if (mp3_frame_info_.nChans == 1) {
                    // Already mono, no conversion needed
                    ESP_LOGD(TAG, "Already mono audio: %d samples", final_sample_count);
                } else {
                    ESP_LOGW(TAG, "Unsupported channel count: %d, treating as mono", 
                            mp3_frame_info_.nChans);
                }
                
                // Create AudioStreamPacket
                AudioStreamPacket packet;
                packet.sample_rate = mp3_frame_info_.samprate;
                packet.frame_duration = 60;  // Use Application default frame duration
                packet.timestamp = 0;
                
                // Convert int16_t PCM data to uint8_t byte array
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
                
                ESP_LOGD(TAG, "Sending %d PCM samples (%d bytes, rate=%d, channels=%d->1) to Application", 
                        final_sample_count, pcm_size_bytes, mp3_frame_info_.samprate, mp3_frame_info_.nChans);
                
                // Send to Application's audio decode queue
                app_.AddAudioData(std::move(packet));
                total_played += pcm_size_bytes;
                
                // Print playback progress (reduced logging)
                if (total_played % (128 * 1024) == 0) {
                    ESP_LOGD(TAG, "Played %zu bytes, buffer size: %zu", 
                            total_played, buffer_size_.load());
                }
            }
            
        } else {
            // Decode failed
            ESP_LOGW(TAG, "MP3 decode failed with error: %d", decode_result);
            
            // Skip some bytes and continue trying
            if (bytes_left > 1) {
                read_ptr++;
                bytes_left--;
            } else {
                bytes_left = 0;
            }
        }
    }
        
    // Perform basic cleanup when playback ends
    ESP_LOGI(TAG, "Audio stream playback finished, total played: %zu bytes", total_played);
    
    // Schedule state change to Listening when music ends naturally
    ESP_LOGI(TAG, "Music playback finished, scheduling state change to Listening");
    // Stop playback flag
    is_playing_ = false;
    
    player_state_.store(PlayerState::IDLE);

    // Only stop FFT display in spectrum display mode
    if (display_mode_ == DISPLAY_MODE_SPECTRUM) {
        auto& board = Board::GetInstance();
        auto display = board.GetDisplay();
        if (display) {
            display->stopFft();
            ESP_LOGI(TAG, "Stopped FFT display from play thread (spectrum mode)");
        }
    } else {
        ESP_LOGI(TAG, "Not in spectrum mode, skipping FFT stop");
    }

    app_.Schedule([this]() {
        SetIdleStateAfterMusic();
    });
}

// Clear audio buffer
void Esp32Music::ClearAudioBuffer() {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    
    while (!audio_buffer_.empty()) {
        // AudioChunk with unique_ptr will automatically free memory
        audio_buffer_.pop();
    }
    
    buffer_size_ = 0;
    ESP_LOGI(TAG, "Audio buffer cleared");
}

// Initialize MP3 decoder
bool Esp32Music::InitializeMp3Decoder() {
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

// Cleanup MP3 decoder
void Esp32Music::CleanupMp3Decoder() {
    if (mp3_decoder_ != nullptr) {
        MP3FreeDecoder(mp3_decoder_);
        mp3_decoder_ = nullptr;
    }
    mp3_decoder_initialized_ = false;
    ESP_LOGI(TAG, "MP3 decoder cleaned up");
}

void Esp32Music::ResetSampleRate() {
    auto& board = Board::GetInstance();
    auto codec = board.GetAudioCodec();
    if (codec && codec->original_output_sample_rate() > 0 && 
        codec->output_sample_rate() != codec->original_output_sample_rate()) {
        ESP_LOGI(TAG, "Resetting sample rate: from %d Hz to original value %d Hz", 
                codec->output_sample_rate(), codec->original_output_sample_rate());
        if (codec->SetOutputSampleRate(-1)) {  // -1 means reset to original value
            ESP_LOGI(TAG, "Successfully reset sample rate to original value: %d Hz", codec->output_sample_rate());
        } else {
            ESP_LOGW(TAG, "Unable to reset sample rate to original value");
        }
    }
}

// Skip ID3 tag at the beginning of MP3 file
size_t Esp32Music::SkipId3Tag(uint8_t* data, size_t size) {
    if (!data || size < 10) {
        return 0;
    }
    
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
    
    // Ensure not exceeding available data size
    if (total_skip > size) {
        total_skip = size;
    }
    
    ESP_LOGI(TAG, "Found ID3v2 tag, skipping %u bytes", (unsigned int)total_skip);
    return total_skip;
}

// Download lyrics
bool Esp32Music::DownloadLyrics(const std::string& lyric_url) {
    ESP_LOGI(TAG, "Downloading lyrics from: %s", lyric_url.c_str());
    
    // Check if URL is empty
    if (lyric_url.empty()) {
        ESP_LOGE(TAG, "Lyric URL is empty!");
        return false;
    }
    
    // Add retry logic
    const int max_retries = 3;
    int retry_count = 0;
    bool success = false;
    std::string lyric_content;
    std::string current_url = lyric_url;
    int redirect_count = 0;
    const int max_redirects = 5;  // Allow maximum 5 redirects
    
    while (retry_count < max_retries && !success && redirect_count < max_redirects) {
        if (retry_count > 0) {
            ESP_LOGI(TAG, "Retrying lyric download (attempt %d of %d)", retry_count + 1, max_retries);
            // 重试前暂停一下
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        
        // Use Board provided HTTP client
        auto network = Board::GetInstance().GetNetwork();
        auto http = network->CreateHttp(0);
        if (!http) {
            ESP_LOGE(TAG, "Failed to create HTTP client for lyric download");
            retry_count++;
            continue;
        }
        
        http->SetTimeout(180000);
        
        http->SetHeader("User-Agent", "ESP32-Music-Player/1.0");
        http->SetHeader("Accept", "text/plain");
        
        // Open GET connection
        ESP_LOGI(TAG, "XiaoZhi Open Source Music Firmware QQ Group: 826072986");
        if (!http->Open("GET", current_url)) {
            ESP_LOGE(TAG, "Failed to open HTTP connection for lyrics");
            // FIX BUG #6: Consistent error handling - http is unique_ptr, auto cleanup
            http.reset();
            retry_count++;
            continue;
        }
        
        // Check HTTP status code
        int status_code = http->GetStatusCode();
        ESP_LOGI(TAG, "Lyric download HTTP status code: %d", status_code);
        
        // Handle redirect - follow redirect manually
        if (status_code == 301 || status_code == 302 || status_code == 303 || status_code == 307 || status_code == 308) {
            std::string location = http->GetResponseHeader("Location");
            http->Close();
            
            if (location.empty()) {
                ESP_LOGE(TAG, "Redirect status %d but no Location header found", status_code);
                retry_count++;
                continue;
            }
            
            ESP_LOGI(TAG, "Following lyric redirect to: %s", location.c_str());
            current_url = location;
            redirect_count++;
            
            // FIX BUG #5: Cleanup before retry
            http.reset();
            
            // Retry with new URL (don't increment retry_count for redirects)
            continue;
        }
        
        // Non-200 series status codes are treated as errors
        if (status_code < 200 || status_code >= 300) {
            ESP_LOGE(TAG, "HTTP GET failed with status code: %d", status_code);
            http->Close();
            retry_count++;
            continue;
        }
        
        // Read response
        lyric_content.clear();
        char buffer[1024];
        int bytes_read;
        bool read_error = false;
        int total_read = 0;
        
        // Since we can't get Content-Length and Content-Type headers, we don't know expected size and content type
        ESP_LOGD(TAG, "Starting to read lyric content");
        
        while (true) {
            bytes_read = http->Read(buffer, sizeof(buffer) - 1);
            // ESP_LOGD(TAG, "Lyric HTTP read returned %d bytes", bytes_read); // Commented out to reduce log output
            
            if (bytes_read > 0) {
                buffer[bytes_read] = '\0';
                lyric_content += buffer;
                total_read += bytes_read;
                
                // Periodically print download progress - changed to DEBUG level to reduce output
                if (total_read % 4096 == 0) {
                    ESP_LOGD(TAG, "Downloaded %d bytes so far", total_read);
                }
            } else if (bytes_read == 0) {
                // Normal end, no more data
                ESP_LOGD(TAG, "Lyric download completed, total bytes: %d", total_read);
                success = true;
                break;
            } else {
                // bytes_read < 0, possibly a known ESP-IDF issue
                // If some data has already been read, consider download successful
                if (!lyric_content.empty()) {
                    ESP_LOGW(TAG, "HTTP read returned %d, but we have data (%d bytes), continuing", bytes_read, lyric_content.length());
                    success = true;
                    break;
                } else {
                    ESP_LOGE(TAG, "Failed to read lyric data: error code %d", bytes_read);
                    read_error = true;
                    break;
                }
            }
        }
        
        http->Close();
        
        if (read_error) {
            retry_count++;
            continue;
        }
        
        // If data was successfully read, break out of retry loop
        if (success) {
            break;
        }
    }
    
    // Check if maximum retry count was exceeded
    if (retry_count >= max_retries) {
        ESP_LOGE(TAG, "Failed to download lyrics after %d attempts", max_retries);
        return false;
    }
    
    // Log first few bytes of data to help debugging
    if (!lyric_content.empty()) {
        size_t preview_size = std::min(lyric_content.size(), size_t(50));
        std::string preview = lyric_content.substr(0, preview_size);
        ESP_LOGD(TAG, "Lyric content preview (%d bytes): %s", lyric_content.length(), preview.c_str());
    } else {
        ESP_LOGE(TAG, "Failed to download lyrics or lyrics are empty");
        return false;
    }
    
    ESP_LOGI(TAG, "Lyrics downloaded successfully, size: %d bytes", lyric_content.length());
    return ParseLyrics(lyric_content);
}

// Parse lyrics
bool Esp32Music::ParseLyrics(const std::string& lyric_content) {
    ESP_LOGI(TAG, "Parsing lyrics content");
    
    // FIX BUG #4: Use unique_lock for writer access
    std::unique_lock<std::shared_mutex> lock(lyrics_mutex_);
    
    lyrics_.clear();
    
    // Split lyric content by lines
    std::istringstream stream(lyric_content);
    std::string line;
    
    while (std::getline(stream, line)) {
        // Remove carriage return at end of line
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        // Skip empty lines
        if (line.empty()) {
            continue;
        }
        
        // Parse LRC format: [mm:ss.xx]lyric text
        if (line.length() > 10 && line[0] == '[') {
            size_t close_bracket = line.find(']');
            if (close_bracket != std::string::npos) {
                std::string tag_or_time = line.substr(1, close_bracket - 1);
                std::string content = line.substr(close_bracket + 1);
                
                // Check if it's a metadata tag instead of timestamp
                // Metadata tags are usually [ti:title], [ar:artist], [al:album], etc.
                size_t colon_pos = tag_or_time.find(':');
                if (colon_pos != std::string::npos) {
                    std::string left_part = tag_or_time.substr(0, colon_pos);
                    
                    // Check if left side of colon is time (digits)
                    bool is_time_format = true;
                    for (char c : left_part) {
                        if (!isdigit(c)) {
                            is_time_format = false;
                            break;
                        }
                    }
                    
                    // If not time format, skip this line (metadata tag)
                    if (!is_time_format) {
                        // Can process metadata here, e.g., extract title, artist, etc.
                        ESP_LOGD(TAG, "Skipping metadata tag: [%s]", tag_or_time.c_str());
                        continue;
                    }
                    
                    // Is time format, parse timestamp
                    try {
                        int minutes = std::stoi(tag_or_time.substr(0, colon_pos));
                        float seconds = std::stof(tag_or_time.substr(colon_pos + 1));
                        int timestamp_ms = minutes * 60 * 1000 + (int)(seconds * 1000);
                        
                        // Safely handle lyric text, ensure UTF-8 encoding is correct
                        std::string safe_lyric_text;
                        if (!content.empty()) {
                            // Create safe copy and validate string
                            safe_lyric_text = content;
                            // Ensure string is null-terminated
                            safe_lyric_text.shrink_to_fit();
                        }
                        
                        lyrics_.push_back(std::make_pair(timestamp_ms, safe_lyric_text));
                        
                        if (!safe_lyric_text.empty()) {
                            // Limit log output length to avoid Chinese character truncation issues
                            size_t log_len = std::min(safe_lyric_text.length(), size_t(50));
                            std::string log_text = safe_lyric_text.substr(0, log_len);
                            ESP_LOGD(TAG, "Parsed lyric: [%d ms] %s", timestamp_ms, log_text.c_str());
                        } else {
                            ESP_LOGD(TAG, "Parsed lyric: [%d ms] (empty)", timestamp_ms);
                        }
                    } catch (const std::exception& e) {
                        ESP_LOGW(TAG, "Failed to parse time: %s", tag_or_time.c_str());
                    }
                }
            }
        }
    }
    
    // Sort by timestamp
    std::sort(lyrics_.begin(), lyrics_.end());
    
    ESP_LOGI(TAG, "Parsed %d lyric lines", lyrics_.size());
    return !lyrics_.empty();
}

// Lyric display thread
void Esp32Music::LyricDisplayThread() {
    ESP_LOGI(TAG, "Lyric display thread started");
    
    if (!DownloadLyrics(current_lyric_url_)) {
        ESP_LOGE(TAG, "Failed to download or parse lyrics");
        
        // Automatically switch to spectrum mode when lyrics download fails
        ESP_LOGI(TAG, "Switching to spectrum display mode due to lyric download failure");
        SetDisplayMode(DISPLAY_MODE_SPECTRUM);
        
        // Start spectrum display
        auto& board = Board::GetInstance();
        auto display = board.GetDisplay();
        if (display) {
            display->start();
            ESP_LOGI(TAG, "Started spectrum display after lyric failure");
        }
        
        is_lyric_running_ = false;
        return;
    }
    
    // Periodically check if display needs updating (frequency can be reduced)
    while (is_lyric_running_ && is_playing_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    ESP_LOGI(TAG, "Lyric display thread finished");
}

void Esp32Music::UpdateLyricDisplay(int64_t current_time_ms) {
    // FIX BUG #4: Use shared_lock for reader access (better concurrency)
    std::shared_lock<std::shared_mutex> lock(lyrics_mutex_);
    
    if (lyrics_.empty()) {
        return;
    }
    
    // Find lyric that should be displayed currently
    int new_lyric_index = -1;
    
    // Start searching from current lyric index for efficiency
    int start_index = (current_lyric_index_.load() >= 0) ? current_lyric_index_.load() : 0;
    
    // Forward search: find the last lyric with timestamp less than or equal to current time
    for (int i = start_index; i < (int)lyrics_.size(); i++) {
        if (lyrics_[i].first <= current_time_ms) {
            new_lyric_index = i;
        } else {
            break;  // Timestamp has exceeded current time
        }
    }
    
    // If not found (possibly current time is earlier than first lyric), display empty
    if (new_lyric_index == -1) {
        new_lyric_index = -1;
    }
    
    // If lyric index changed, update display
    if (new_lyric_index != current_lyric_index_) {
        current_lyric_index_ = new_lyric_index;
        
        auto& board = Board::GetInstance();
        auto display = board.GetDisplay();
        if (display) {
            std::string lyric_text;
            
            if (current_lyric_index_ >= 0 && current_lyric_index_ < (int)lyrics_.size()) {
                lyric_text = lyrics_[current_lyric_index_].second;
            }
            
            // Display lyric
            display->SetChatMessage("lyric", lyric_text.c_str());
            
            ESP_LOGD(TAG, "Lyric update at %lldms: %s", 
                    current_time_ms, 
                    lyric_text.empty() ? "(no lyric)" : lyric_text.c_str());
        }
    }
}

// Display mode control method implementation
void Esp32Music::SetDisplayMode(DisplayMode mode) {
    DisplayMode old_mode = display_mode_.load();
    display_mode_ = mode;
    
    ESP_LOGI(TAG, "Display mode changed from %s to %s", 
            (old_mode == DISPLAY_MODE_SPECTRUM) ? "SPECTRUM" : "LYRICS",
            (mode == DISPLAY_MODE_SPECTRUM) ? "SPECTRUM" : "LYRICS");
}

void Esp32Music::SetIdleStateAfterMusic() {
    vTaskDelay(pdMS_TO_TICKS(100));
    
    app_.SetDeviceState(kDeviceStateIdle);
    
    ESP_LOGI(TAG, "Music finished, device set to idle state with wake word detection enabled");
}
