#include "radio_player.h"

#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"

#include "board.h"
#include "display.h"
#include "application.h"
#include "mcp_server.h"
#include "audio_codec.h"

#include <esp_log.h>
#include <esp_heap_caps.h>
#include <esp_ae_rate_cvt.h>
#include <cstring>
#include <algorithm>
#include <cctype>

#define TAG "RadioPlayer"

RadioPlayer& RadioPlayer::GetInstance() {
    static RadioPlayer instance;
    return instance;
}

RadioPlayer::RadioPlayer() {
    // Indonesian favorite radio streams
    stations_ = {
        {"prambors", "Prambors FM Jakarta", "http://masima.rastream.com/masima-pramborsjakarta?type=.mp3", "Hits Pop Indo & Mancanegara"},
        {"iradio", "101.4 I-Radio Jakarta", "https://stream.radiojar.com/4ywdgup3bnzuv", "100% Musik Indonesia"},
        {"delta", "Delta FM Indonesia", "http://masima.rastream.com/masima-delta?type=.mp3", "Lagu Enak Hits 90s-2000s"},
        {"rri_pro1", "RRI Pro 1 Jakarta", "http://stream-node1.rri.co.id/streaming/25/9025/rrijakartapro1.mp3", "Berita & Musik Nasional"},
        {"rri_pro2", "RRI Pro 2 Indonesia", "https://stream-node2.rri.co.id/streaming/21/9221/rritjpinangpro2.mp3", "Kreatif & Hits Pop Anak Muda"},
        {"rri_ch5", "RRI Channel 5", "https://stream-node1.rri.co.id/streaming/41/8941/chlima.mp3", "Musik Tradisional Nusantara"},
        {"ardan", "Ardan Radio 105.9 FM Bandung", "https://stream.rcs.revma.com/ugpyzu9n5k3vv", "Hits Musik Pop & RnB"},
        {"geronimo", "Geronimo FM Yogyakarta", "https://ig.idstreamer.com:8090//live", "Top 40 & Hits Indonesia"},
        {"dakta", "Radio Dakta 107 FM", "http://175.103.48.4:9302/stream", "Informasi & Berita Aktual"},
        {"nagaswara", "Nagaswara FM Radiotemen", "http://live.nagaswarafm.com:9900/stream", "Dangdut & Pop Indonesia Hits"},
        {"campursari", "Radio Campursari Jawa", "http://stream.suararadio.com:8000/campursari", "Campursari & Gending Jawa"}
    };
}

RadioPlayer::~RadioPlayer() {
    Stop();
}

std::vector<RadioStation> RadioPlayer::GetStationList() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stations_;
}

std::string RadioPlayer::GetCurrentStation() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_station_name_;
}

bool RadioPlayer::IsPlaying() const {
    return is_playing_.load();
}

std::string RadioPlayer::ResolveStationUrl(const std::string& input, std::string& out_name) {
    if (input.empty()) {
        out_name = stations_[1].name; // Default: I-Radio
        return stations_[1].url;
    }

    // Direct HTTP(S) stream URL
    if (input.rfind("http://", 0) == 0 || input.rfind("https://", 0) == 0) {
        out_name = "Live Stream Web Radio";
        return input;
    }

    // Convert input to lowercase for fuzzy matching
    std::string query = input;
    std::transform(query.begin(), query.end(), query.begin(), [](unsigned char c) { return std::tolower(c); });

    // Check exact id match or substring match
    for (const auto& s : stations_) {
        if (s.id == query || s.name.find(input) != std::string::npos) {
            out_name = s.name;
            return s.url;
        }
    }

    // Smart semantic keyword mapping
    if (query.find("prambor") != std::string::npos || query.find("hits") != std::string::npos) {
        out_name = "Prambors FM";
        return stations_[0].url;
    }
    if (query.find("indo") != std::string::npos || query.find("pop") != std::string::npos || query.find("lagu") != std::string::npos || query.find("musik") != std::string::npos) {
        out_name = "I-Radio (100% Musik Indonesia)";
        return stations_[1].url;
    }
    if (query.find("delta") != std::string::npos || query.find("enak") != std::string::npos || query.find("santai") != std::string::npos) {
        out_name = "Delta FM";
        return stations_[2].url;
    }
    if (query.find("berita") != std::string::npos || query.find("news") != std::string::npos || query.find("nasional") != std::string::npos) {
        out_name = "RRI Pro 1 (Berita Nasional)";
        return stations_[3].url;
    }
    if (query.find("rri 2") != std::string::npos || query.find("pro 2") != std::string::npos || query.find("muda") != std::string::npos) {
        out_name = "RRI Pro 2 (Kreatif & Pop)";
        return stations_[4].url;
    }
    if (query.find("budaya") != std::string::npos || query.find("tradisi") != std::string::npos) {
        out_name = "RRI Channel 5 (Budaya Nusantara)";
        return stations_[5].url;
    }
    if (query.find("ardan") != std::string::npos || query.find("bandung") != std::string::npos) {
        out_name = "Ardan FM Bandung";
        return stations_[6].url;
    }
    if (query.find("jogja") != std::string::npos || query.find("geronimo") != std::string::npos) {
        out_name = "Geronimo FM Yogyakarta";
        return stations_[7].url;
    }
    if (query.find("dangdut") != std::string::npos || query.find("koplo") != std::string::npos || query.find("nagaswara") != std::string::npos) {
        out_name = "Nagaswara FM (Dangdut & Pop)";
        return stations_[9].url;
    }
    if (query.find("campursari") != std::string::npos || query.find("jawa") != std::string::npos) {
        out_name = "Radio Campursari Jawa";
        return stations_[10].url;
    }

    // Default fallback: I-Radio 100% Musik Indonesia
    out_name = stations_[1].name;
    return stations_[1].url;
}

bool RadioPlayer::Play(const std::string& station_or_url) {
    Stop();

    std::string name;
    std::string url = ResolveStationUrl(station_or_url, name);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        current_station_name_ = name;
        current_url_ = url;
        stop_requested_ = false;
        is_playing_ = true;
    }

    BaseType_t res = xTaskCreate(TaskEntry, "radio_stream", 10240, this, 2, &task_handle_);
    if (res != pdPASS) {
        ESP_LOGE(TAG, "Failed to create radio streaming task");
        std::lock_guard<std::mutex> lock(mutex_);
        is_playing_ = false;
        task_handle_ = nullptr;
        return false;
    }

    return true;
}

void RadioPlayer::Stop() {
    if (!is_playing_.load() && task_handle_ == nullptr) {
        return;
    }

    stop_requested_ = true;

    // Wait briefly for task to terminate itself
    for (int i = 0; i < 30; ++i) {
        if (!is_playing_.load()) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    task_handle_ = nullptr;
    is_playing_ = false;
}

void RadioPlayer::TaskEntry(void* arg) {
    auto* self = static_cast<RadioPlayer*>(arg);
    self->PlayerTask();
    vTaskDelete(nullptr);
}

void RadioPlayer::PlayerTask() {
    std::string url;
    std::string station_name;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        url = current_url_;
        station_name = current_station_name_;
    }

    ESP_LOGI(TAG, "Opening radio stream: %s (%s)", station_name.c_str(), url.c_str());

    auto network = Board::GetInstance().GetNetwork();
    if (!network) {
        ESP_LOGE(TAG, "Network interface not available");
        is_playing_ = false;
        return;
    }

    auto http = network->CreateHttp(0);
    if (!http) {
        ESP_LOGE(TAG, "Failed to create HTTP client");
        is_playing_ = false;
        return;
    }

    http->SetTimeout(10000);
    http->SetHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) XiaoZhi-Radio/1.0");
    http->SetHeader("Accept", "*/*");
    http->SetHeader("Icy-MetaData", "0");

    auto opened = http->Open("GET", url);
    if (!opened) {
        ESP_LOGE(TAG, "Failed to open stream URL: %s", opened.error().ToString().c_str());
        is_playing_ = false;
        return;
    }

    auto status = http->GetStatusCode();
    if (!status || *status < 200 || *status >= 400) {
        ESP_LOGE(TAG, "Stream returned HTTP status: %d", status ? *status : -1);
        http->Close();
        is_playing_ = false;
        return;
    }

    auto codec = Board::GetInstance().GetAudioCodec();
    if (!codec) {
        ESP_LOGE(TAG, "Audio codec unavailable");
        http->Close();
        is_playing_ = false;
        return;
    }

    codec->EnableOutput(true);

    mp3dec_t mp3d;
    mp3dec_init(&mp3d);

    // 16KB stream ring/sliding buffer in PSRAM
    constexpr size_t kStreamBufferSize = 16384;
    uint8_t* stream_buffer = (uint8_t*)heap_caps_malloc(kStreamBufferSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!stream_buffer) {
        stream_buffer = (uint8_t*)malloc(kStreamBufferSize);
    }
    if (!stream_buffer) {
        ESP_LOGE(TAG, "Failed to allocate memory for streaming buffer");
        http->Close();
        is_playing_ = false;
        return;
    }

    size_t buffer_len = 0;
    mp3d_sample_t pcm_frame[MINIMP3_MAX_SAMPLES_PER_FRAME];
    mp3dec_frame_info_t info;

    esp_ae_rate_cvt_handle_t resampler = nullptr;
    int current_sample_rate = 0;
    const int target_sample_rate = codec->output_sample_rate();

    auto display = Board::GetInstance().GetDisplay();
    if (display) {
        std::string label = "📻 " + station_name;
        display->SetChatMessage("assistant", label.c_str());
        display->SetEmotion("happy");
    }

    ESP_LOGI(TAG, "Radio stream connected! Beginning playback...");

    while (!stop_requested_.load()) {
        // Automatically pause audio output when AI is listening or speaking
        DeviceState dev_state = Application::GetInstance().GetDeviceState();
        if (dev_state == kDeviceStateListening || dev_state == kDeviceStateSpeaking) {
            vTaskDelay(pdMS_TO_TICKS(150));
            continue;
        }

        // Fill buffer from network
        if (buffer_len < kStreamBufferSize / 2) {
            auto read_res = http->Read(reinterpret_cast<char*>(stream_buffer + buffer_len), kStreamBufferSize - buffer_len);
            if (!read_res) {
                ESP_LOGW(TAG, "HTTP read error: %s", read_res.error().ToString().c_str());
                break;
            }
            if (*read_res > 0) {
                buffer_len += *read_res;
            } else if (*read_res == 0 && buffer_len == 0) {
                ESP_LOGI(TAG, "Stream reached end");
                break;
            }
        }

        if (buffer_len < 128) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // Decode one MP3 frame
        int samples = mp3dec_decode_frame(&mp3d, stream_buffer, buffer_len, pcm_frame, &info);
        if (info.frame_bytes > 0) {
            if ((size_t)info.frame_bytes <= buffer_len) {
                memmove(stream_buffer, stream_buffer + info.frame_bytes, buffer_len - info.frame_bytes);
                buffer_len -= info.frame_bytes;
            } else {
                buffer_len = 0;
            }
        }

        if (samples > 0) {
            // Reconfigure rate converter if stream sample rate changed
            if (info.hz != current_sample_rate) {
                if (resampler) {
                    esp_ae_rate_cvt_close(resampler);
                    resampler = nullptr;
                }
                current_sample_rate = info.hz;
                if (current_sample_rate != target_sample_rate) {
                    esp_ae_rate_cvt_cfg_t cvt_cfg = {
                        .src_rate = (uint32_t)current_sample_rate,
                        .dest_rate = (uint32_t)target_sample_rate,
                        .channel = ESP_AUDIO_MONO,
                        .bits_per_sample = ESP_AUDIO_BIT16,
                        .complexity = 2,
                        .perf_type = ESP_AE_RATE_CVT_PERF_TYPE_SPEED,
                    };
                    esp_ae_rate_cvt_open(&cvt_cfg, &resampler);
                }
            }

            // Downmix to mono
            std::vector<int16_t> mono_pcm(samples);
            if (info.channels == 2) {
                for (int i = 0; i < samples; ++i) {
                    mono_pcm[i] = (int16_t)(((int32_t)pcm_frame[i * 2] + (int32_t)pcm_frame[i * 2 + 1]) / 2);
                }
            } else {
                for (int i = 0; i < samples; ++i) {
                    mono_pcm[i] = pcm_frame[i];
                }
            }

            // Resample to target output sample rate if needed, then output
            if (resampler && current_sample_rate != target_sample_rate) {
                uint32_t max_out = 0;
                esp_ae_rate_cvt_get_max_out_sample_num(resampler, samples, &max_out);
                std::vector<int16_t> out_pcm(max_out);
                uint32_t out_samples = 0;
                esp_ae_rate_cvt_process(resampler, (esp_ae_sample_t)mono_pcm.data(), samples, (esp_ae_sample_t)out_pcm.data(), &out_samples);
                out_pcm.resize(out_samples);
                codec->OutputData(out_pcm);
            } else {
                codec->OutputData(mono_pcm);
            }
        } else if (info.frame_bytes == 0) {
            // Need more data for a frame
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    if (resampler) {
        esp_ae_rate_cvt_close(resampler);
        resampler = nullptr;
    }

    free(stream_buffer);
    http->Close();

    if (display) {
        display->SetChatMessage("assistant", "");
        display->SetEmotion("neutral");
    }

    is_playing_ = false;
    ESP_LOGI(TAG, "Radio streaming task terminated cleanly");
}

// -------------------------------------------------------------
// MCP Tools Controller for Radio
// -------------------------------------------------------------
RadioController::RadioController() {
    auto& mcp_server = McpServer::GetInstance();

    mcp_server.AddTool("self.radio.play",
        "Putar siaran radio online Indonesia (musik, lagu pop, dangdut, berita, campursari) langsung di ESP32. "
        "Gunakan tool ini ketika pengguna meminta memutar musik, lagu, atau radio. "
        "Parameter 'station': nama stasiun ('prambors', 'iradio', 'delta', 'rri_pro1', 'rri_pro2', 'rri_ch5', 'ardan', 'geronimo', 'nagaswara', 'campursari') atau URL streaming langsung.",
        PropertyList({Property("station", kPropertyTypeString, std::string("iradio"))}),
        [](const PropertyList& properties) -> ReturnValue {
            auto station = properties["station"].value<std::string>();
            bool ok = RadioPlayer::GetInstance().Play(station);
            if (ok) {
                return "Memutar radio: " + RadioPlayer::GetInstance().GetCurrentStation();
            } else {
                return "Gagal memutar stasiun radio.";
            }
        });

    mcp_server.AddTool("self.radio.stop",
        "Hentikan pemutaran siaran radio atau musik streaming yang sedang aktif.",
        PropertyList(),
        [](const PropertyList& properties) -> ReturnValue {
            RadioPlayer::GetInstance().Stop();
            return "Radio telah dihentikan.";
        });

    mcp_server.AddTool("self.radio.get_status",
        "Periksa apakah radio sedang aktif dan stasiun apa yang sedang diputar.",
        PropertyList(),
        [](const PropertyList& properties) -> ReturnValue {
            if (RadioPlayer::GetInstance().IsPlaying()) {
                return "{\"playing\": true, \"station\": \"" + RadioPlayer::GetInstance().GetCurrentStation() + "\"}";
            }
            return "{\"playing\": false}";
        });

    mcp_server.AddTool("self.radio.list_stations",
        "Dapatkan daftar nama stasiun radio Indonesia yang tersedia di perangkat.",
        PropertyList(),
        [](const PropertyList& properties) -> ReturnValue {
            auto list = RadioPlayer::GetInstance().GetStationList();
            std::string result = "Daftar stasiun radio:\n";
            for (const auto& s : list) {
                result += "- " + s.name + " (" + s.genre + ")\n";
            }
            return result;
        });
}
