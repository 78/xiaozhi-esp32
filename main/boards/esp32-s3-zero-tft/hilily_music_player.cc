#include "hilily_music_player.h"
#include "hilily_display.h"
#include "application.h"
#include "board.h"
#include "audio_codec.h"
#include "esp_audio_simple_dec.h"
#include "esp_audio_dec.h"
#include "esp_mp3_dec.h"

#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <cJSON.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {
constexpr const char* TAG = "HiLilyMusic";
constexpr size_t kMetadataLimit = 8192;
constexpr size_t kLyricLimit = 32768;
constexpr size_t kMaxEncoded = 12288;
constexpr int kOutputHz = 24000;
constexpr size_t kInputBlock = 2048;

struct Buffer {
    std::string bytes;
    size_t limit;
    bool overflow = false;
};
esp_err_t CaptureData(esp_http_client_event_t* e) {
    auto* b = static_cast<Buffer*>(e->user_data);
    if (!b || e->event_id != HTTP_EVENT_ON_DATA || !e->data || e->data_len <= 0)
        return ESP_OK;
    if (b->bytes.size() + static_cast<size_t>(e->data_len) > b->limit) {
        b->overflow = true;
        return ESP_FAIL;
    }
    b->bytes.append(static_cast<const char*>(e->data), e->data_len);
    return ESP_OK;
}
std::string UrlEncode(const std::string& input) {
    static const char hex[] = "0123456789ABCDEF";
    std::string result;
    for (unsigned char c : input) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            result += static_cast<char>(c);
        } else {
            result += '%';
            result += hex[c >> 4];
            result += hex[c & 15];
        }
    }
    return result;
}
bool ProxyPathAllowed(const std::string& path, const char* allowed) {
    if (path.empty() || path[0] != '/' || path.rfind("//", 0) == 0 ||
        path.find('#') != std::string::npos || path.find('\\') != std::string::npos ||
        path.find("://") != std::string::npos) return false;
    std::string base = path.substr(0, path.find('?'));
    return base == allowed;
}
std::string GetJsonText(cJSON* object, const char* name) {
    cJSON* item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsString(item) && item->valuestring ? item->valuestring : "";
}
bool FetchSmall(const std::string& url, size_t limit, std::string& result) {
    Buffer b{{}, limit, false};
    esp_http_client_config_t cfg = {};
    cfg.url = url.c_str();
    cfg.method = HTTP_METHOD_GET;
    cfg.timeout_ms = 13000;
    cfg.crt_bundle_attach = esp_crt_bundle_attach;
    cfg.user_data = &b;
    cfg.event_handler = CaptureData;
    cfg.buffer_size = 1024;
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return false;
    const esp_err_t ret = esp_http_client_perform(client);
    const int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    if (ret != ESP_OK || status != 200 || b.overflow) {
        ESP_LOGW(TAG, "HTTP metadata failed %d err=%s", status, esp_err_to_name(ret));
        return false;
    }
    result = std::move(b.bytes);
    return !result.empty();
}
struct Track {
    std::string title, artist, audio, lyric;
    uint32_t duration_ms = 0;
};
bool ResolveTrack(const std::string& base, const std::string& song,
                  const std::string& artist, Track& track) {
    std::string response;
    if (!FetchSmall(base + "/stream_pcm?song=" + UrlEncode(song) +
                    "&artist=" + UrlEncode(artist), kMetadataLimit, response)) return false;
    cJSON* data = cJSON_ParseWithLength(response.data(), response.size());
    if (!data) return false;
    const cJSON* ok = cJSON_GetObjectItemCaseSensitive(data, "ok");
    cJSON* info = data;
    // stream_pcm normally emits {ok:true,songs:[], ...item}; fallback to songs[0].
    if (ok && !cJSON_IsTrue(ok)) { cJSON_Delete(data); return false; }
    if (!cJSON_IsString(cJSON_GetObjectItemCaseSensitive(info, "audio_url"))) {
        cJSON* arr = cJSON_GetObjectItemCaseSensitive(data, "songs");
        if (cJSON_IsArray(arr)) info = cJSON_GetArrayItem(arr, 0);
    }
    track.title = GetJsonText(info, "title");
    track.artist = GetJsonText(info, "artist");
    track.audio = GetJsonText(info, "audio_url");
    track.lyric = GetJsonText(info, "lyric_url");
    cJSON* length = cJSON_GetObjectItemCaseSensitive(info, "duration");
    if (cJSON_IsNumber(length) && length->valuedouble > 0 && length->valuedouble < 86400)
        track.duration_ms = static_cast<uint32_t>(length->valuedouble * 1000);
    cJSON_Delete(data);
    if (track.title.empty()) track.title = song;
    if (track.artist.empty()) track.artist = artist;
    return ProxyPathAllowed(track.audio, "/proxy_audio") &&
           (track.lyric.empty() || ProxyPathAllowed(track.lyric, "/proxy_lyric"));
}
std::string ReadLyrics(const std::string& base, const std::string& path) {
    if (!ProxyPathAllowed(path, "/proxy_lyric")) return {};
    std::string raw;
    if (!FetchSmall(base + path, kLyricLimit, raw)) return {};
    if (!raw.empty() && raw[0] == '{') {
        cJSON* root = cJSON_ParseWithLength(raw.data(), raw.size());
        if (!root) return {};
        std::string lyric = GetJsonText(root, "lyric");
        if (lyric.empty()) lyric = GetJsonText(root, "lyrics");
        cJSON* data = cJSON_GetObjectItemCaseSensitive(root, "data");
        if (lyric.empty() && cJSON_IsObject(data)) lyric = GetJsonText(data, "lyric");
        cJSON_Delete(root);
        return lyric.size() <= kLyricLimit ? lyric : std::string{};
    }
    return raw;
}
// Resample a decoded MP3 frame to 24kHz/mono. Persist the fractional input
// index across frames; the timeline counts PCM submitted to I2S, not fetch time.
struct Resampler {
    int input_hz = 0, channels = 0;
    double source_position = 0;
    uint64_t input_frames = 0;
    int16_t previous_sample = 0;
    bool has_previous = false;
    uint32_t elapsed_samples = 0;
    void Reset(int hz, int ch) {
        input_hz = hz; channels = ch; source_position = 0;
        input_frames = 0; has_previous = false;
    }
    void Convert(const int16_t* pcm, size_t frames, int hz, int ch, std::vector<int16_t>& output) {
        output.clear();
        if (!frames || (ch != 1 && ch != 2) || hz < 8000 || hz > 96000) return;
        if (hz != input_hz || ch != channels) Reset(hz, ch);
        const double step = static_cast<double>(hz) / kOutputHz;
        const auto sample = [&](size_t i) -> int16_t {
            if (ch == 1) return pcm[i];
            return static_cast<int16_t>((static_cast<int32_t>(pcm[2*i]) +
                                         static_cast<int32_t>(pcm[2*i+1])) / 2);
        };
        const uint64_t start = input_frames;
        const uint64_t finish = start + frames;
        // Endpoint samples for interpolation never read past this frame.
        while (source_position < static_cast<double>(finish - 1)) {
            uint64_t idx = static_cast<uint64_t>(source_position);
            double frac = source_position - static_cast<double>(idx);
            int16_t left = idx < start && has_previous ? previous_sample : sample(idx < start ? 0 : idx - start);
            int16_t right = sample(std::min<uint64_t>(frames-1, idx+1 < start ? 0 : idx+1-start));
            output.push_back(static_cast<int16_t>(std::clamp(
                static_cast<int32_t>(std::lround(left + (right-left)*frac)), -32768, 32767)));
            source_position += step;
        }
        previous_sample = sample(frames-1);
        has_previous = true;
        input_frames = finish;
    }
};
}  // namespace

bool HiLilyMusicPlayer::HandleRecognizedSpeech(const std::string& recognized_text) {
    // This is a deterministic local shortcut, not a claim that the upstream
    // XiaoZhi backend has disabled its LLM. The WSS backend may still respond.
    if (recognized_text.empty() || recognized_text.size() > 300) return false;
    std::string text = recognized_text;
    while (!text.empty() && (text.back()=='.' || text.back()=='!' ||
           text.back()=='?' || text.back()==' ')) text.pop_back();

    if (text.find("tạm dừng nhạc") != std::string::npos ||
        text.find("tạm dừng bài hát") != std::string::npos) return Pause();
    if (text.find("dừng nhạc") != std::string::npos ||
        text.find("tắt nhạc") != std::string::npos ||
        text.find("dừng bài hát") != std::string::npos) return Stop();
    if (text.find("tiếp tục nhạc") != std::string::npos ||
        text.find("phát tiếp nhạc") != std::string::npos) return Resume();

    const char* prefixes[] = {"phát bài ", "mở bài ", "nghe bài ",
                              "bật bài ", "phát nhạc ", "mở nhạc "};
    for (const char* prefix : prefixes) {
        const auto pos=text.find(prefix);
        if (pos==std::string::npos || pos>75) continue;
        auto song=text.substr(pos+std::strlen(prefix));
        if (song.empty() || song.size()>100) return false;
        std::string artist;
        const auto of=song.find(" của ");
        if (of!=std::string::npos) {
            artist=song.substr(of+5);
            song=song.substr(0,of);
        }
        if(song.empty()||song.size()>100||artist.size()>100)return false;
        return Play(song,artist);
    }
    return false;
}

HiLilyMusicPlayer& HiLilyMusicPlayer::GetInstance() {
    static HiLilyMusicPlayer player;
    return player;
}
void HiLilyMusicPlayer::Configure(HiLilyDisplay* display, const char* worker_base) {
    std::lock_guard<std::mutex> guard(mutex_);
    display_ = display;
    worker_base_ = worker_base ? worker_base : "";
}
bool HiLilyMusicPlayer::Play(std::string title, std::string artist) {
    if (title.empty() || title.size() > 100 || artist.size() > 100) return false;
    std::lock_guard<std::mutex> guard(mutex_);
    if (!display_ || worker_base_.rfind("https://",0)!=0) return false;
    requested_title_ = std::move(title);
    requested_artist_ = std::move(artist);
    pending_ = true; paused_ = false;
    position_ms_ = 0;
    playback_error_.clear();
    ++generation_;
    if (!task_) {
        // MP3 decode, TLS and LVGL share scarce internal RAM on N4R2.
        // Allocate the music worker stack in external PSRAM when available.
        TaskHandle_t created = nullptr;
        BaseType_t ok = xTaskCreatePinnedToCoreWithCaps(
            WorkerEntry, "hilily_mp3", 12288, this, 3, &created, tskNO_AFFINITY,
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (ok != pdPASS) {
            ESP_LOGW(TAG, "PSRAM music task unavailable; retrying internal stack");
            ok = xTaskCreate(WorkerEntry, "hilily_mp3", 12288, this, 3, &created);
        }
        if (ok != pdPASS) {
            task_ = nullptr;
            pending_ = false;
            return false;
        }
        task_ = created;
    }
    return true;
}
bool HiLilyMusicPlayer::Pause() {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!active_ && !pending_) return false;
    paused_ = true;
    return true;
}
bool HiLilyMusicPlayer::Resume() {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!active_ && !pending_) return false;
    paused_ = false;
    return true;
}
bool HiLilyMusicPlayer::Stop() {
    std::lock_guard<std::mutex> guard(mutex_);
    ++generation_; pending_ = false; paused_ = false;
    return true;
}
bool HiLilyMusicPlayer::Cancelled(uint32_t generation) {
    std::lock_guard<std::mutex> guard(mutex_);
    return generation != generation_;
}
bool HiLilyMusicPlayer::AwaitPlay(uint32_t generation) {
    while (true) {
        {
            std::lock_guard<std::mutex> guard(mutex_);
            if (generation != generation_) return false;
            if (!paused_) return true;
        }
        vTaskDelay(pdMS_TO_TICKS(45));
    }
}
std::string HiLilyMusicPlayer::StatusJson() {
    std::lock_guard<std::mutex> guard(mutex_);
    const char* state = pending_ ? "loading" : active_ ? (paused_ ? "paused" : "playing") : "idle";
    cJSON* root = cJSON_CreateObject();
    if (!root) return "{\"state\":\"error\"}";
    cJSON_AddStringToObject(root, "state", state);
    cJSON_AddStringToObject(root, "requested_song", requested_title_.c_str());
    cJSON_AddStringToObject(root, "requested_artist", requested_artist_.c_str());
    cJSON_AddNumberToObject(root, "position_ms", position_ms_);
    cJSON_AddStringToObject(root, "last_error", playback_error_.c_str());
    char* raw = cJSON_PrintUnformatted(root);
    std::string json = raw ? raw : "{}";
    if (raw) cJSON_free(raw);
    cJSON_Delete(root);
    return json;
}
void HiLilyMusicPlayer::UpdatePosition(uint32_t generation, uint32_t elapsed_ms) {
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (generation != generation_) return;
        position_ms_ = elapsed_ms;
    }
    display_->SetMusicPosition(elapsed_ms, true);
}
void HiLilyMusicPlayer::WorkerEntry(void* arg) {
    static_cast<HiLilyMusicPlayer*>(arg)->Worker();
    vTaskDelete(nullptr);
}
void HiLilyMusicPlayer::Worker() {
    while (true) {
        std::string song,artist;
        uint32_t generation = 0;
        {
            std::lock_guard<std::mutex> guard(mutex_);
            if (!pending_) { task_ = nullptr; return; }
            song = requested_title_;
            artist = requested_artist_;
            generation = generation_;
            pending_ = false;
            active_ = true;
        }
        const bool completed = StreamOne(generation,song,artist);
        {
            std::lock_guard<std::mutex> guard(mutex_);
            active_ = false;
            if (!completed && generation == generation_) playback_error_ = "stream_failed";
            // Keep the previous screen until the replacement track takes over.
            if (!pending_ && generation == generation_ && display_) display_->ClearMusic();
        }
    }
}
bool HiLilyMusicPlayer::StreamOne(uint32_t generation, const std::string& title,
                                  const std::string& artist) {
    auto& service = Application::GetInstance().GetAudioService();
    auto* codec = Board::GetInstance().GetAudioCodec();
    if (!codec || !display_) return false;
    Track track;
    std::string base;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        base = worker_base_;
    }
    if (!ResolveTrack(base, title, artist, track) || Cancelled(generation)) {
        ESP_LOGW(TAG, "No playable Worker track");
        return false;
    }
    const std::string lrc = ReadLyrics(base,track.lyric);
    if (Cancelled(generation)) return false;
    // Starting a new song updates metadata but keeps the audio clock at 0.
    display_->SetMusicTrack(track.title, track.artist, track.duration_ms, lrc);
    // Register once and tolerate an already-registered MP3 decoder.
    if (esp_audio_dec_check_audio_type(ESP_AUDIO_TYPE_MP3) != ESP_AUDIO_ERR_OK) {
        esp_audio_err_t reg = esp_mp3_dec_register();
        if (reg != ESP_AUDIO_ERR_OK &&
            esp_audio_dec_check_audio_type(ESP_AUDIO_TYPE_MP3) != ESP_AUDIO_ERR_OK) {
            ESP_LOGE(TAG,"MP3 decoder register failed %d",reg);
            return false;
        }
    }
    esp_audio_simple_dec_cfg_t decoder_cfg = {};
    decoder_cfg.dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_MP3;
    esp_audio_simple_dec_handle_t decoder = nullptr;
    if (esp_audio_simple_dec_open(&decoder_cfg,&decoder) != ESP_AUDIO_ERR_OK || !decoder) {
        ESP_LOGE(TAG,"MP3 decoder open failed");
        return false;
    }
    auto http = Board::GetInstance().GetNetwork()->CreateHttp(0);
    if (!http) { esp_audio_simple_dec_close(decoder); return false; }
    http->SetTimeout(12000);
    http->SetHeader("Accept", "audio/mpeg, audio/mp3, application/octet-stream");
    http->SetHeader("Accept-Encoding", "identity");
    if (!http->Open("GET",base+track.audio)) {
        ESP_LOGE(TAG,"Could not open Worker MP3 stream");
        http->Close();
        esp_audio_simple_dec_close(decoder);
        return false;
    }
    const auto status = http->GetStatusCode();
    if (!status || *status != 200) {
        ESP_LOGE(TAG,"Music stream failed HTTP %d",status ? *status : -1);
        http->Close(); esp_audio_simple_dec_close(decoder);
        return false;
    }
    service.BeginExternalPlayback();
    codec->EnableOutput(true);
    auto release = [&]() {
        http->Close();
        esp_audio_simple_dec_close(decoder);
        service.EndExternalPlayback();
    };
    std::array<uint8_t,kInputBlock> input = {};
    std::vector<uint8_t> pending;
    pending.reserve(kMaxEncoded);
    std::vector<uint8_t> pcm(16384);
    Resampler resampler;
    std::vector<int16_t> playback;
    bool ended = false,success = false,decode_error=false;
    unsigned consecutive_read_failures = 0;
    uint32_t frames_decoded = 0;
    bool pause_was_reported = false;
    while (!Cancelled(generation)) {
        // Update LVGL on pause without advancing the audio clock.
        bool paused_now = false;
        {
            std::lock_guard<std::mutex> guard(mutex_);
            paused_now = paused_;
        }
        if (paused_now != pause_was_reported) {
            display_->SetMusicPosition(static_cast<uint32_t>(
                static_cast<uint64_t>(resampler.elapsed_samples)*1000/kOutputHz), !paused_now);
            pause_was_reported = paused_now;
        }
        if (!AwaitPlay(generation)) break;
        // When paused, don't read the TCP stream. Keep microphone wake detection alive.
        if (!ended && pending.size() < kMaxEncoded-kInputBlock) {
            auto count = http->Read(reinterpret_cast<char*>(input.data()),input.size());
            if (!count) {
                // A transient timeout must not tear down the decoder. The
                // provider may recover without requiring a new MP3 session.
                if (++consecutive_read_failures > 3) {
                    ESP_LOGW(TAG,"MP3 stream interrupted after retries");
                    break;
                }
                ESP_LOGW(TAG,"MP3 read timeout (%u/3)",consecutive_read_failures);
                vTaskDelay(pdMS_TO_TICKS(250 * consecutive_read_failures));
                continue;
            }
            consecutive_read_failures = 0;
            if (*count == 0) ended = true;
            else pending.insert(pending.end(),input.begin(),input.begin()+*count);
        }
        if (pending.empty()) { if (ended) {success=frames_decoded>0; break;} continue; }
        esp_audio_simple_dec_raw_t raw={};
        raw.buffer=pending.data();
        raw.len=static_cast<uint32_t>(pending.size());
        raw.eos=ended;
        esp_audio_simple_dec_out_t frame={};
        frame.buffer=pcm.data();
        frame.len=static_cast<uint32_t>(pcm.size());
        esp_audio_err_t ret = esp_audio_simple_dec_process(decoder,&raw,&frame);
        if (ret == ESP_AUDIO_ERR_BUFF_NOT_ENOUGH && frame.needed_size>pcm.size() &&
            frame.needed_size<=65536) {
            pcm.resize(frame.needed_size);
            continue;
        }
        if (ret != ESP_AUDIO_ERR_OK) {
            ESP_LOGW(TAG,"MP3 decode failed: %d",ret);
            decode_error=true;
            break;
        }
        if (raw.consumed > pending.size()) {decode_error=true;break;}
        if (raw.consumed) pending.erase(pending.begin(),pending.begin()+raw.consumed);
        if (frame.decoded_size > 0) {
            esp_audio_simple_dec_info_t info={};
            if (esp_audio_simple_dec_get_info(decoder,&info)!=ESP_AUDIO_ERR_OK ||
                info.bits_per_sample!=16 || info.channel<1 || info.channel>2 ||
                info.sample_rate<8000 || info.sample_rate>96000 ||
                frame.decoded_size % (info.channel*sizeof(int16_t)) != 0) {
                decode_error=true;break;
            }
            size_t decoded_frames = frame.decoded_size/(info.channel*sizeof(int16_t));
            resampler.Convert(reinterpret_cast<int16_t*>(pcm.data()),decoded_frames,
                              info.sample_rate,info.channel,playback);
            if (!playback.empty()) {
                if (!AwaitPlay(generation))break;
                // OutputData is synchronous: stop/pause is checked again
                // immediately before submitting each decoded PCM block.
                if (!AwaitPlay(generation)) break;
                codec->OutputData(playback);
                resampler.elapsed_samples+=playback.size();
                ++frames_decoded;
                UpdatePosition(generation,static_cast<uint32_t>(
                    static_cast<uint64_t>(resampler.elapsed_samples)*1000/kOutputHz));
            }
        }
        if (!raw.consumed && !frame.decoded_size) {
            if (ended) {success=frames_decoded>0;break;}
            if (pending.size() >= kMaxEncoded-kInputBlock) {decode_error=true;break;}
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }
    release();
    if (decode_error) ESP_LOGW(TAG,"Unsupported/damaged MP3 stream");
    ESP_LOGI(TAG,"Music finished: frames=%lu ok=%d",(unsigned long)frames_decoded,success);
    return success;
}
