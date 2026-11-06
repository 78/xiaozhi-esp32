/**
 * @file audio_stream_player.cc
 * @brief Implementation of the base audio stream player.
 *
 * Supports both HTTP streams and SD card files via virtual SourceDataLoop().
 * Handles MP3/AAC/FLAC decoding and WAV raw PCM passthrough.
 */

#include "audio_stream_player.h"
#include "board.h"
#include "audio/audio_codec.h"

#include <esp_log.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <cstring>
#include <algorithm>
#include <cmath>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "AudioStreamPlayer";

/* ================================================================== */
/*  Constructor / Destructor                                          */
/* ================================================================== */

AudioStreamPlayer::AudioStreamPlayer()
    : wav_info_{},
      is_playing_(false),
      is_source_active_(false),
      is_paused_(false),
      display_mode_(DISPLAY_MODE_SPECTRUM),
      volume_factor_(AUDIO_DEFAULT_VOLUME),
      source_task_handle_(nullptr),
#if AUDIO_STREAM_STATIC_TASK_CREATION == 1
      source_task_buffer_(nullptr),
      source_task_stack_(nullptr),
#endif
      play_task_handle_(nullptr),
#if AUDIO_STREAM_STATIC_TASK_CREATION == 1
      play_task_buffer_(nullptr),
      play_task_stack_(nullptr),
#endif
      pause_sem_(nullptr),
      buffer_mutex_(nullptr),
      buffer_data_sem_(nullptr),
      buffer_space_sem_(nullptr),
      buffer_size_(0),
      decoder_(nullptr),
      dec_info_{},
      decoder_initialized_(false),
      dec_info_ready_(false),
      decoder_type_(AudioDecoderType::MP3),
      pcm_out_buffer_(nullptr),
      pcm_out_buffer_size_(0),
      input_buffer_(nullptr),
      input_bytes_left_(0),
      current_play_time_ms_(0),
      total_frames_decoded_(0)
{
    buffer_mutex_     = xSemaphoreCreateMutex();
    buffer_data_sem_  = xSemaphoreCreateCounting(128, 0);
    buffer_space_sem_ = xSemaphoreCreateCounting(128, 128);
    pause_sem_        = xSemaphoreCreateBinary();
}

AudioStreamPlayer::~AudioStreamPlayer()
{
    ESP_LOGI(TAG, "Destroying AudioStreamPlayer");
    StopStream();

    if (buffer_mutex_)     vSemaphoreDelete(buffer_mutex_);
    if (buffer_data_sem_)  vSemaphoreDelete(buffer_data_sem_);
    if (buffer_space_sem_) vSemaphoreDelete(buffer_space_sem_);
    if (pause_sem_)        vSemaphoreDelete(pause_sem_);
}

/* ================================================================== */
/*  Public: StartStream / StopStream                                  */
/* ================================================================== */

bool AudioStreamPlayer::StartStream(const std::string& source, AudioDecoderType type)
{
    if (source.empty()) {
        ESP_LOGE(TAG, "Stream source is empty");
        return false;
    }

    ESP_LOGI(TAG, "StartStream: source=%s, type=%d", source.c_str(), (int)type);

    /* Stop previous session */
    StopStream();

    SetPlayerState(AudioPlayerState::Loading);

    stream_url_           = source;
    decoder_type_         = type;
    is_paused_            = false;
    current_play_time_ms_ = 0;
    total_frames_decoded_ = 0;
    buffer_size_          = 0;
    content_length_       = 0;

    ClearAudioBuffer();

    /* Launch source task */
    is_source_active_ = true;
#if AUDIO_STREAM_STATIC_TASK_CREATION == 1
    if (source_task_stack_ == nullptr) {
        source_task_stack_ = (StackType_t*)heap_caps_malloc(AUDIO_SOURCE_TASK_STACK, MALLOC_CAP_SPIRAM);
        assert(source_task_stack_ != nullptr);
    }
    if (source_task_buffer_ == nullptr) {
        source_task_buffer_ = (StaticTask_t*)heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL);
        assert(source_task_buffer_ != nullptr);
    }
    source_task_handle_ = xTaskCreateStaticPinnedToCore(
        SourceTaskEntry, "stream_src",
        AUDIO_SOURCE_TASK_STACK, this,
        AUDIO_SOURCE_TASK_PRIO, source_task_stack_, source_task_buffer_,
        AUDIO_SOURCE_TASK_CORE);

    if (source_task_handle_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create source task");
        is_source_active_ = false;
        return false;
    }
#else
    BaseType_t ret = xTaskCreatePinnedToCore(
        SourceTaskEntry, "stream_src",
        AUDIO_SOURCE_TASK_STACK, this,
        AUDIO_SOURCE_TASK_PRIO, &source_task_handle_,
        AUDIO_SOURCE_TASK_CORE);
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create source task");
        is_source_active_ = false;
        return false;
    }
#endif

    /* Launch playback task */
    is_playing_ = true;
#if AUDIO_STREAM_STATIC_TASK_CREATION == 1
    if (play_task_stack_ == nullptr) {
        play_task_stack_ = (StackType_t*)heap_caps_malloc(AUDIO_PLAY_TASK_STACK, MALLOC_CAP_SPIRAM);
        assert(play_task_stack_ != nullptr);
    }
    if (play_task_buffer_ == nullptr) {
        play_task_buffer_ = (StaticTask_t*)heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL);
        assert(play_task_buffer_ != nullptr);
    }
    play_task_handle_ = xTaskCreateStaticPinnedToCore(
        PlayTaskEntry, "stream_play",
        AUDIO_PLAY_TASK_STACK, this,
        AUDIO_PLAY_TASK_PRIO, play_task_stack_, play_task_buffer_,
        AUDIO_PLAY_TASK_CORE);
    
    if (play_task_handle_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create playback task");
        is_playing_       = false;
        is_source_active_ = false;
        return false;
    }
#else
    ret = xTaskCreatePinnedToCore(
        PlayTaskEntry, "stream_play",
        AUDIO_PLAY_TASK_STACK, this,
        AUDIO_PLAY_TASK_PRIO, &play_task_handle_,
        AUDIO_PLAY_TASK_CORE);

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create playback task");
        is_playing_       = false;
        is_source_active_ = false;
        return false;
    }
#endif

    ESP_LOGI(TAG, "Stream tasks started");
    return true;
}

bool AudioStreamPlayer::StopStream()
{
    if (!is_playing_ && !is_source_active_) {
        return true;
    }

    ESP_LOGI(TAG, "StopStream: src=%d, play=%d",
             is_source_active_.load(), is_playing_.load());

    SetPlayerState(AudioPlayerState::Stopping);

    is_source_active_ = false;
    is_playing_       = false;
    is_paused_        = false;

    /* Wake up paused playback */
    if (pause_sem_) xSemaphoreGive(pause_sem_);

    /* Signal semaphores so tasks can exit */
    if (buffer_data_sem_)  xSemaphoreGive(buffer_data_sem_);
    if (buffer_space_sem_) xSemaphoreGive(buffer_space_sem_);

    /* Wait for source task */
    if (source_task_handle_) {
        for (int i = 0; i < 50 && source_task_handle_; ++i) {
            xSemaphoreGive(buffer_space_sem_);
            vTaskDelay(pdMS_TO_TICKS(100));
            if (source_task_handle_ == nullptr) break;
            if (eTaskGetState(source_task_handle_) == eDeleted) break;
        }
        source_task_handle_ = nullptr;
#if AUDIO_STREAM_STATIC_TASK_CREATION == 1
        if (source_task_buffer_) {
            heap_caps_free(source_task_buffer_);
            source_task_buffer_ = nullptr;
        }
        if (source_task_stack_) {
            heap_caps_free(source_task_stack_);
            source_task_stack_ = nullptr;
        }
#endif
    }

    /* Wait for play task */
    if (play_task_handle_) {
        for (int i = 0; i < 50 && play_task_handle_; ++i) {
            xSemaphoreGive(buffer_data_sem_);
            if (pause_sem_) xSemaphoreGive(pause_sem_);
            vTaskDelay(pdMS_TO_TICKS(100));
            if (play_task_handle_ == nullptr) break;
            if (eTaskGetState(play_task_handle_) == eDeleted) break;
        }
        play_task_handle_ = nullptr;
#if AUDIO_STREAM_STATIC_TASK_CREATION == 1
        if (play_task_buffer_) {
            heap_caps_free(play_task_buffer_);
            play_task_buffer_ = nullptr;
        }
        if (play_task_stack_) {
            heap_caps_free(play_task_stack_);
            play_task_stack_ = nullptr;
        }
#endif
    }

    ClearAudioBuffer();
    CleanupDecoder();
    ResetSampleRate();
    bool continue_playing = OnPlaybackFinishedAndContinue();

    /* Notify end callback */
    if (end_callback_) {
        continue_playing = end_callback_(stream_url_);
    }

    if (!continue_playing) 
    {
        SetPlayerState(AudioPlayerState::Idle);
    }

    ESP_LOGI(TAG, "Stream stopped");
    return true;
}

/* ================================================================== */
/*  Pause / Resume                                                    */
/* ================================================================== */

void AudioStreamPlayer::PauseStream()
{
    if (!is_playing_ || is_paused_) return;
    ESP_LOGI(TAG, "Pausing stream");
    is_paused_ = true;
    SetPlayerState(AudioPlayerState::Paused);
    OnPauseStateChanged(true);
}

void AudioStreamPlayer::ResumeStream()
{
    if (!is_playing_ || !is_paused_) return;
    ESP_LOGI(TAG, "Resuming stream");
    is_paused_ = false;
    if (pause_sem_) xSemaphoreGive(pause_sem_);
    SetPlayerState(AudioPlayerState::Playing);
    OnPauseStateChanged(false);
}

void AudioStreamPlayer::SetDisplayMode(DisplayMode mode)
{
    display_mode_ = mode;
}

/* ================================================================== */
/*  FreeRTOS task entry points                                        */
/* ================================================================== */

void AudioStreamPlayer::SourceTaskEntry(void* param)
{
    auto* self = static_cast<AudioStreamPlayer*>(param);
    self->SourceDataLoop(self->stream_url_);
    self->is_source_active_ = false;
    self->source_task_handle_ = nullptr;
    /* Wake playback in case it is waiting for data */
    if (self->buffer_data_sem_) xSemaphoreGive(self->buffer_data_sem_);
    vTaskDelete(nullptr);
}

void AudioStreamPlayer::PlayTaskEntry(void* param)
{
    auto* self = static_cast<AudioStreamPlayer*>(param);
    self->PlayLoop();
    self->play_task_handle_ = nullptr;
    vTaskDelete(nullptr);
}

/* ================================================================== */
/*  Default SourceDataLoop -- HTTP streaming with reconnect           */
/* ================================================================== */

void AudioStreamPlayer::SourceDataLoop(const std::string& source)
{
    ESP_LOGI(TAG, "HTTP source started: %s", source.c_str());

    if (source.empty() || source.find("http") != 0) {
        ESP_LOGE(TAG, "Invalid URL: %s", source.c_str());
        return;
    }

    auto network = Board::GetInstance().GetNetwork();
    std::unique_ptr<Http> http;
    std::string current_url = source;
    int redirects = 0;
    const int max_redirects = 5;

    while (redirects <= max_redirects) {
        http = std::unique_ptr<Http>(network->CreateHttp(0));
        http->SetHeader("User-Agent", "ESP32-Music-Player/1.0");
        http->SetHeader("Accept", "*/*");
        http->SetHeader("Range", "bytes=0-");

        OnPrepareHttp(http.get());

        if (!http->Open("GET", current_url)) {
            ESP_LOGE(TAG, "Failed to connect: %s", current_url.c_str());
            return;
        }

        int status = http->GetStatusCode();
        if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
            std::string location = http->GetResponseHeader("Location");
            http->Close();
            if (location.empty()) {
                ESP_LOGE(TAG, "HTTP %d redirect without Location header", status);
                return;
            }
            ESP_LOGI(TAG, "HTTP %d redirect -> %s", status, location.c_str());
            current_url = location;
            redirects++;
            continue;
        }

        if (status != 200 && status != 206) {
            ESP_LOGE(TAG, "HTTP status %d for: %s", status, current_url.c_str());
            http->Close();
            return;
        }

        break;
    }

    if (redirects > max_redirects) {
        ESP_LOGE(TAG, "Too many redirects (%d)", redirects);
        return;
    }

    ESP_LOGI(TAG, "HTTP connected, status=%d", http->GetStatusCode());

    /* Capture content length for duration estimation */
    content_length_ = http->GetBodyLength();
    ESP_LOGI(TAG, "Content-Length: %zu bytes", content_length_);

    char* buf = (char*)heap_caps_malloc(AUDIO_HTTP_CHUNK_SIZE, MALLOC_CAP_SPIRAM);
    if (!buf) {
        ESP_LOGE(TAG, "Read buffer alloc failed");
        http->Close();
        return;
    }

    size_t total = 0;
    size_t log_counter = 0;
    int reconnects = 0;

    while (is_source_active_ && is_playing_) {
        int n = http->Read(buf, AUDIO_HTTP_CHUNK_SIZE);

        if (n <= 0) {
            if (n == 0 && reconnects == 0) {
                ESP_LOGI(TAG, "HTTP stream ended after %zu bytes", total);
                break;
            }
            reconnects++;
            ESP_LOGW(TAG, "Read failed (%d), reconnect %d/%d",
                     n, reconnects, AUDIO_MAX_RECONNECT);
            if (reconnects > AUDIO_MAX_RECONNECT) break;

            vTaskDelay(pdMS_TO_TICKS(AUDIO_RECONNECT_DELAY_MS));
            http->Close();
            if (!http->Open("GET", source)) continue;
            OnPrepareHttp(http.get());
            ESP_LOGI(TAG, "Reconnected at attempt %d", reconnects);
            continue;
        }

        reconnects = 0;

        /* Format detection on first chunk */
        if (total == 0 && n >= 4) {
            const uint8_t* d = (const uint8_t*)buf;
            if (memcmp(d, "ID3", 3) == 0) {
                ESP_LOGI(TAG, "Detected MP3 (ID3 tag)");
            } else if ((d[0] & 0xFF) == 0xFF && (d[1] & 0xE0) == 0xE0) {
                ESP_LOGI(TAG, "Detected raw MP3 frame");
            } else {
                ESP_LOGI(TAG, "Header: %02X %02X %02X %02X",
                         d[0], d[1], d[2], d[3]);
            }
        }

        if (!PushToBuffer(buf, n)) break;

        total += n;
        log_counter += n;
        if (log_counter >= AUDIO_LOG_INTERVAL) {
            log_counter = 0;
            ESP_LOGI(TAG, "Downloaded %zu bytes, buf=%zu", total, buffer_size_);
        }
    }

    heap_caps_free(buf);
    http->Close();

    ESP_LOGI(TAG, "HTTP source finished. Total: %zu bytes", total);
}

/* ================================================================== */
/*  PushToBuffer -- thread-safe helper for subclasses                 */
/* ================================================================== */

bool AudioStreamPlayer::PushToBuffer(const void* data, size_t size)
{
    if (!data || size == 0) return false;

    uint8_t* chunk = (uint8_t*)heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (!chunk) {
        ESP_LOGE(TAG, "PSRAM alloc failed for %zu bytes", size);
        return false;
    }
    memcpy(chunk, data, size);

    /* Back-pressure: wait for space */
    while (is_source_active_ && is_playing_ && buffer_size_ >= AUDIO_BUF_MAX_SIZE) {
        xSemaphoreTake(buffer_space_sem_, pdMS_TO_TICKS(100));
    }

    if (!is_source_active_ || !is_playing_) {
        heap_caps_free(chunk);
        return false;
    }

    if (xSemaphoreTake(buffer_mutex_, pdMS_TO_TICKS(500)) == pdTRUE) {
        audio_buffer_.push(StreamAudioChunk(chunk, size));
        buffer_size_ += size;
        xSemaphoreGive(buffer_mutex_);
        xSemaphoreGive(buffer_data_sem_);
        return true;
    }

    heap_caps_free(chunk);
    ESP_LOGW(TAG, "Buffer mutex timeout in PushToBuffer");
    return false;
}

/* ================================================================== */
/*  PlayLoop -- dispatch to compressed or WAV path                    */
/* ================================================================== */

void AudioStreamPlayer::PlayLoop()
{
    ESP_LOGI(TAG, "PlayLoop started, type=%d", (int)decoder_type_);

    current_play_time_ms_ = 0;
    total_frames_decoded_ = 0;

    // NOTE: Device idle transition is now handled externally by
    // Application::EnsureIdleForMedia() before starting playback.
    SetPlayerState(AudioPlayerState::Playing);

    if (!audio_codec_) {
        ESP_LOGE(TAG, "Audio codec not available");
        is_playing_ = false;
        return;
    }
    if (!audio_codec_->output_enabled()) {
        ESP_LOGW(TAG, "%s Enabling audio output for playback", __func__);
        audio_codec_->EnableOutput(true);
    }

    /* Choose path based on decoder type */
    if (decoder_type_ == AudioDecoderType::WAV) {
        PlayLoopWav();
    } else {
        PlayLoopCompressed();
    }

    /* Common cleanup */
    if (input_buffer_) {
        heap_caps_free(input_buffer_);
        input_buffer_ = nullptr;
    }
    input_bytes_left_ = 0;

    bool was_playing = is_playing_.load();
    is_playing_ = false;

    CleanupDecoder();

    /* Notify subclass if playback ended naturally */
    if (was_playing) {
        ClearAudioBuffer();
        ResetSampleRate();

        bool continue_playing = OnPlaybackFinishedAndContinue();
        /* Notify end callback */
        if (end_callback_) {
            continue_playing = end_callback_(stream_url_);
        }

        if (!continue_playing) 
        {
            SetPlayerState(AudioPlayerState::Idle);
        }
    }

    ESP_LOGI(TAG, "PlayLoop finished");
}

/* ================================================================== */
/*  Common helpers: pause check                                       */
/* ================================================================== */

bool AudioStreamPlayer::HandlePause()
{
    /* Pause check */
    if (is_paused_) {
        xSemaphoreTake(pause_sem_, portMAX_DELAY);
        if (!is_playing_) return false;
    }
    return true;
}

/* ================================================================== */
/*  OutputPcmFrame -- mono downmix, volume amp, callbacks, audio out  */
/* ================================================================== */

void AudioStreamPlayer::OutputPcmFrame(int16_t* pcm_in, int total_samples,
                                        int channels, int sample_rate,
                                        int frame_duration_ms)
{
    int16_t* final_pcm   = pcm_in;
    int      final_count = total_samples;

    /* Mono downmix */
    std::vector<int16_t> mono_buf;
    if (channels == 2) {
        int spc = total_samples / 2;
        mono_buf.resize(spc);
        for (int i = 0; i < spc; ++i) {
            mono_buf[i] = (int16_t)((pcm_in[i * 2] + pcm_in[i * 2 + 1]) / 2);
        }
        final_pcm   = mono_buf.data();
        final_count = spc;
    }

    /* Volume amplification */
    std::vector<int16_t> amp_buf(final_count);
    for (int i = 0; i < final_count; ++i) {
        int32_t s = (int32_t)(final_pcm[i] * volume_factor_);
        if (s > INT16_MAX)      s = INT16_MAX;
        else if (s < INT16_MIN) s = INT16_MIN;
        amp_buf[i] = (int16_t)s;
    }

    /* Notify FFT callback (handled externally by Application) */
    size_t pcm_bytes = final_count * sizeof(int16_t);
    if (fft_callback_) {
        fft_callback_(amp_buf.data(), pcm_bytes);
    }

    /* Notify PCM callback for custom processing */
    if (pcm_callback_) {
        pcm_callback_(amp_buf.data(), final_count, 1, sample_rate);
    }

    /* Output audio through direct codec path */
    if (audio_codec_) {
        OutputPcmDirect(amp_buf.data(), final_count, 1, sample_rate);
    } else {
        ESP_LOGW(TAG, "No audio codec set, PCM frame dropped");
    }
}

/* ================================================================== */
/*  OutputPcmDirect -- direct codec output                            */
/* ================================================================== */

void AudioStreamPlayer::OutputPcmDirect(int16_t* pcm_in, int total_samples,
                                         int channels, int sample_rate)
{
    if (!audio_codec_ || !pcm_in || total_samples == 0) return;

    /* Ensure codec output is enabled */
    if (!audio_codec_->output_enabled()) {
        ESP_LOGW(TAG, "%s Enabling audio output for playback", __func__);
        audio_codec_->EnableOutput(true);
    }

    /* Set sample rate if different from current codec rate */
    if (audio_codec_->output_sample_rate() != sample_rate && sample_rate > 0) {
        ESP_LOGI(TAG, "Setting codec sample rate: %d Hz", sample_rate);
        audio_codec_->SetOutputSampleRate(sample_rate);
    }

    int codec_channels = audio_codec_->output_channels();
    std::vector<int16_t> audio_out;

    if (channels == 1 && codec_channels >= 2) {
        /* Mono → Stereo: duplicate each sample */
        audio_out.resize(total_samples * 2);
        for (int i = 0; i < total_samples; i++) {
            audio_out[i * 2]     = pcm_in[i];
            audio_out[i * 2 + 1] = pcm_in[i];
        }
    } else if (channels == 2 && codec_channels == 1) {
        /* Stereo → Mono: average channels */
        int frames = total_samples / 2;
        audio_out.resize(frames);
        for (int i = 0; i < frames; i++) {
            audio_out[i] = (int16_t)(((int32_t)pcm_in[i * 2] + pcm_in[i * 2 + 1]) / 2);
        }
    } else {
        /* Same channel count: pass through */
        audio_out.assign(pcm_in, pcm_in + total_samples);
    }

    /* Blocking write to I2S DMA via AudioCodec */
    audio_codec_->OutputData(audio_out);
}

/* ================================================================== */
/*  Player state management                                           */
/* ================================================================== */

void AudioStreamPlayer::SetPlayerState(AudioPlayerState new_state)
{
    AudioPlayerState old_state = player_state_.exchange(new_state);
    if (old_state != new_state) {
        ESP_LOGW(TAG, "AudioPlayerState: %d -> %d",
                 static_cast<int>(old_state), static_cast<int>(new_state));
        if (state_callback_) {
            state_callback_(old_state, new_state);
        }
    }
}

/* ================================================================== */
/*  PlayLoopCompressed -- MP3 / AAC / FLAC decode path                */
/* ================================================================== */

void AudioStreamPlayer::PlayLoopCompressed()
{
    ESP_LOGI(TAG, "Compressed playback path");

    /* Wait for minimum buffer fill */
    size_t min_buf = AUDIO_BUF_MIN_SIZE;
    while (is_playing_ && buffer_size_ < min_buf && is_source_active_) {
        xSemaphoreTake(buffer_data_sem_, pdMS_TO_TICKS(100));
    }

    if (!InitDecoder(decoder_type_)) {
        ESP_LOGE(TAG, "Decoder init failed");
        is_playing_ = false;
        return;
    }

    /* Allocate decoder input buffer */
    input_buffer_ = (uint8_t*)heap_caps_malloc(AUDIO_DEC_INPUT_BUF_SIZE,
                                                MALLOC_CAP_SPIRAM);
    if (!input_buffer_) {
        ESP_LOGE(TAG, "Input buffer alloc failed");
        is_playing_ = false;
        return;
    }
    input_bytes_left_ = 0;

    size_t total_played = 0;
    size_t log_counter  = 0;

    while (is_playing_) {
        if (!HandlePause()) break;

        /* Fill input buffer from audio buffer */
        if (input_bytes_left_ < (AUDIO_DEC_INPUT_BUF_SIZE / 2)) {
            StreamAudioChunk chunk;
            bool got = false;

            if (xSemaphoreTake(buffer_mutex_, pdMS_TO_TICKS(100)) == pdTRUE) {
                if (!audio_buffer_.empty()) {
                    chunk = audio_buffer_.front();
                    audio_buffer_.pop();
                    buffer_size_ -= chunk.size;
                    got = true;
                }
                xSemaphoreGive(buffer_mutex_);
                xSemaphoreGive(buffer_space_sem_);
            }

            if (!got) {
                if (!is_source_active_ && buffer_size_ == 0) {
                    ESP_LOGI(TAG, "Source ended, total=%zu", total_played);
                    break;
                }
                xSemaphoreTake(buffer_data_sem_, pdMS_TO_TICKS(50));
                continue;
            }

            if (chunk.data && chunk.size > 0) {
                size_t space = AUDIO_DEC_INPUT_BUF_SIZE - input_bytes_left_;
                size_t copy  = std::min(chunk.size, space);
                memcpy(input_buffer_ + input_bytes_left_, chunk.data, copy);
                input_bytes_left_ += copy;
                total_played += chunk.size;
                log_counter  += chunk.size;
                heap_caps_free(chunk.data);
            }
        }

        if (input_bytes_left_ <= 0) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        /* Decode */
        bool eos = (!is_source_active_ && buffer_size_ == 0);

        esp_audio_simple_dec_raw_t raw = {};
        raw.buffer   = input_buffer_;
        raw.len      = (uint32_t)input_bytes_left_;
        raw.eos      = eos;
        raw.consumed = 0;
        raw.frame_recover = ESP_AUDIO_SIMPLE_DEC_RECOVERY_NONE;

        esp_audio_simple_dec_out_t out = {};
        if (decoder_type_ == AudioDecoderType::AAC) {
            if (dec_out_vec_.empty()) dec_out_vec_.resize(AUDIO_PCM_OUT_BUF_SIZE);
            out.buffer = dec_out_vec_.data();
            out.len    = dec_out_vec_.size();
        } else {
            out.buffer = pcm_out_buffer_;
            out.len    = (uint32_t)pcm_out_buffer_size_;
        }
        out.decoded_size = 0;
        out.needed_size  = 0;

        esp_audio_err_t ret = esp_audio_simple_dec_process(decoder_, &raw, &out);

        if (ret == ESP_AUDIO_ERR_BUFF_NOT_ENOUGH) {
            ESP_LOGI(TAG, "Decoder needs bigger output buffer: %zu bytes",
                     (size_t)out.needed_size);
            if (decoder_type_ == AudioDecoderType::AAC) {
                dec_out_vec_.resize(out.needed_size);
            } else {
                heap_caps_free(pcm_out_buffer_);
                pcm_out_buffer_size_ = out.needed_size;
                pcm_out_buffer_ = (uint8_t*)heap_caps_malloc(
                    pcm_out_buffer_size_, MALLOC_CAP_SPIRAM);
                if (!pcm_out_buffer_) { ESP_LOGE(TAG, "PCM realloc fail"); break; }
            }
            continue;
        }

        if (raw.consumed > 0) {
            input_bytes_left_ -= raw.consumed;
            if (input_bytes_left_ > 0)
                memmove(input_buffer_, input_buffer_ + raw.consumed, input_bytes_left_);
        }

        if (ret == ESP_AUDIO_ERR_DATA_LACK) { 
            ESP_LOGI(TAG, "Decoder needs more data to continue");
            vTaskDelay(pdMS_TO_TICKS(20)); continue; 
        }

        if (ret == ESP_AUDIO_ERR_CONTINUE) {
            ESP_LOGI(TAG, "Decoder requests to continue without new input");
            continue;
        }
        
        if (ret != ESP_AUDIO_ERR_OK) {
            ESP_LOGE(TAG, "Decoder error: %d", ret);
            if (input_bytes_left_ > 0) {
                input_bytes_left_--;
                memmove(input_buffer_, input_buffer_ + 1, input_bytes_left_);
            }
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        /* Successful decode */
        ret = esp_audio_simple_dec_get_info(decoder_, &dec_info_);
        if (ret != ESP_AUDIO_ERR_OK) {
            ESP_LOGE(TAG, "Failed to get decoder info: %d", ret);
            continue;
        }

        if (!dec_info_ready_ && dec_info_.sample_rate > 0) {
            dec_info_ready_ = true;
            ESP_LOGI(TAG, "Stream: %d Hz, %d bit, %d ch, %d kbps, %d frame size",
                     (int)dec_info_.sample_rate, (int)dec_info_.bits_per_sample, (int)dec_info_.channel, (int)dec_info_.bitrate, (int)dec_info_.frame_size);
            OnStreamInfoReady(dec_info_.sample_rate, dec_info_.bits_per_sample, dec_info_.channel, dec_info_.bitrate, dec_info_.frame_size);
        }

        total_frames_decoded_++;
        if (dec_info_.sample_rate == 0 || dec_info_.channel == 0) continue;

        int bits  = (dec_info_.bits_per_sample > 0) ? dec_info_.bits_per_sample : 16;
        int chans = (dec_info_.channel > 0) ? dec_info_.channel : 1;
        int bps   = bits / 8;
        int total_samples    = out.decoded_size / bps;
        int samples_per_chan = total_samples / chans;
        int frame_ms = (samples_per_chan * 1000) / dec_info_.sample_rate;
        current_play_time_ms_ += frame_ms;

        OnPcmFrame(current_play_time_ms_, dec_info_.sample_rate, chans);

        OutputPcmFrame(reinterpret_cast<int16_t*>(out.buffer),
                       total_samples, chans, dec_info_.sample_rate, frame_ms);

        if (log_counter >= AUDIO_LOG_INTERVAL) {
            log_counter = 0;
            ESP_LOGI(TAG, "Played %zu bytes, buf=%zu", total_played, buffer_size_);
        }

        if (eos && input_bytes_left_ == 0) {
            ESP_LOGI(TAG, "EOS reached");
            break;
        }
    }

    ESP_LOGI(TAG, "Compressed loop done, total=%zu", total_played);
}

/* ================================================================== */
/*  PlayLoopWav -- raw PCM passthrough for WAV files                  */
/* ================================================================== */

void AudioStreamPlayer::PlayLoopWav()
{
    ESP_LOGI(TAG, "WAV passthrough path");

    int sr = wav_info_.sample_rate;
    int ch = wav_info_.channels;

    if (sr == 0 || ch == 0) {
        ESP_LOGE(TAG, "Invalid WAV info (sr=%d, ch=%d)", sr, ch);
        is_playing_ = false;
        return;
    }

    /* Set codec sample rate */
    if (audio_codec_ && audio_codec_->output_sample_rate() != sr) {
        ESP_LOGI(TAG, "Set sample rate -> %d Hz", sr);
        audio_codec_->SetOutputSampleRate(sr);
    }

    /* Notify subclass */
    OnStreamInfoReady(sr, wav_info_.bits_per_sample, ch, 0, 0);

    /* Wait for some data */
    while (is_playing_ && buffer_size_ < AUDIO_FILE_BUF_MIN_SIZE && is_source_active_) {
        xSemaphoreTake(buffer_data_sem_, pdMS_TO_TICKS(100));
    }

    size_t total_played = 0;

    while (is_playing_) {
        if (!HandlePause()) break;

        /* Get chunk from buffer */
        StreamAudioChunk chunk;
        bool got = false;

        if (xSemaphoreTake(buffer_mutex_, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (!audio_buffer_.empty()) {
                chunk = audio_buffer_.front();
                audio_buffer_.pop();
                buffer_size_ -= chunk.size;
                got = true;
            }
            xSemaphoreGive(buffer_mutex_);
            xSemaphoreGive(buffer_space_sem_);
        }

        if (!got) {
            if (!is_source_active_ && buffer_size_ == 0) {
                ESP_LOGI(TAG, "WAV source ended, total=%zu", total_played);
                break;
            }
            xSemaphoreTake(buffer_data_sem_, pdMS_TO_TICKS(50));
            continue;
        }

        if (!chunk.data || chunk.size == 0) continue;

        /* Interpret as raw PCM */
        int total_samples    = chunk.size / sizeof(int16_t);
        int samples_per_chan = (ch > 0) ? total_samples / ch : total_samples;
        int frame_ms         = (sr > 0) ? (samples_per_chan * 1000) / sr : 0;
        current_play_time_ms_ += frame_ms;

        OnPcmFrame(current_play_time_ms_, sr, ch);

        OutputPcmFrame(reinterpret_cast<int16_t*>(chunk.data),
                       total_samples, ch, sr, frame_ms);

        total_played += chunk.size;
        heap_caps_free(chunk.data);
    }

    ESP_LOGI(TAG, "WAV loop done, total=%zu", total_played);
}

/* ================================================================== */
/*  Buffer helpers                                                    */
/* ================================================================== */

void AudioStreamPlayer::ClearAudioBuffer()
{
    if (xSemaphoreTake(buffer_mutex_, pdMS_TO_TICKS(1000)) == pdTRUE) {
        while (!audio_buffer_.empty()) {
            auto c = audio_buffer_.front();
            audio_buffer_.pop();
            if (c.data) heap_caps_free(c.data);
        }
        buffer_size_ = 0;
        xSemaphoreGive(buffer_mutex_);
    }
}

/* ================================================================== */
/*  Decoder helpers                                                   */
/* ================================================================== */

bool AudioStreamPlayer::InitDecoder(AudioDecoderType type)
{
    if (decoder_initialized_) {
        ESP_LOGW(TAG, "Decoder already initialised");
        return true;
    }

    ESP_LOGI(TAG, "Init decoder type=%d", (int)type);

    esp_audio_dec_register_default();
    esp_audio_simple_dec_register_default();

    esp_audio_simple_dec_cfg_t cfg = {};
    switch (type) {
        case AudioDecoderType::AAC:
            cfg.dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_AAC;
            break;
        case AudioDecoderType::FLAC:
            cfg.dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_FLAC;
            break;
        case AudioDecoderType::MP3:
        default:
            cfg.dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_MP3;
            break;
    }
    cfg.dec_cfg  = nullptr;
    cfg.cfg_size = 0;

    esp_audio_err_t ret = esp_audio_simple_dec_open(&cfg, &decoder_);
    if (ret != ESP_AUDIO_ERR_OK || !decoder_) {
        ESP_LOGE(TAG, "Decoder open failed: %d", ret);
        esp_audio_simple_dec_unregister_default();
        esp_audio_dec_unregister_default();
        return false;
    }

    pcm_out_buffer_size_ = AUDIO_PCM_OUT_BUF_SIZE;
    pcm_out_buffer_ = (uint8_t*)heap_caps_malloc(pcm_out_buffer_size_, MALLOC_CAP_SPIRAM);
    if (!pcm_out_buffer_) {
        ESP_LOGE(TAG, "PCM buffer alloc failed");
        esp_audio_simple_dec_close(decoder_);
        decoder_ = nullptr;
        return false;
    }

    if (type == AudioDecoderType::AAC) {
        dec_out_vec_.resize(AUDIO_PCM_OUT_BUF_SIZE);
    }

    memset(&dec_info_, 0, sizeof(dec_info_));
    dec_info_ready_      = false;
    decoder_initialized_ = true;
    decoder_type_        = type;

    ESP_LOGI(TAG, "Decoder ready (type=%d)", (int)type);
    return true;
}

void AudioStreamPlayer::CleanupDecoder()
{
    if (!decoder_initialized_) return;

    if (decoder_) {
        esp_audio_simple_dec_close(decoder_);
        decoder_ = nullptr;
    }
    if (pcm_out_buffer_) {
        heap_caps_free(pcm_out_buffer_);
        pcm_out_buffer_ = nullptr;
        pcm_out_buffer_size_ = 0;
    }

    dec_out_vec_.clear();
    dec_info_ready_      = false;
    decoder_initialized_ = false;

    esp_audio_simple_dec_unregister_default();
    esp_audio_dec_unregister_default();
}

AudioDecoderType AudioStreamPlayer::DetectStreamType(const uint8_t* data, size_t len)
{
    if (!data || len < 4) return AudioDecoderType::MP3;
    if (memcmp(data, "ID3", 3) == 0) return AudioDecoderType::MP3;
    if ((data[0] & 0xFF) == 0xFF && (data[1] & 0xE0) == 0xE0)
        return AudioDecoderType::MP3;
    if ((data[0] & 0xFF) == 0xFF && (data[1] & 0xF0) == 0xF0)
        return AudioDecoderType::AAC;
    return AudioDecoderType::MP3;
}

/* ================================================================== */
/*  Sample rate reset                                                 */
/* ================================================================== */

void AudioStreamPlayer::ResetSampleRate()
{
    if (audio_codec_ && audio_codec_->original_output_sample_rate() > 0 &&
        audio_codec_->output_sample_rate() != audio_codec_->original_output_sample_rate()) {
        ESP_LOGI(TAG, "Reset sample rate: %d -> %d",
                 audio_codec_->output_sample_rate(),
                 audio_codec_->original_output_sample_rate());
        audio_codec_->SetOutputSampleRate(-1);
    }
}
