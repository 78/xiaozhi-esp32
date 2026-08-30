#include "telegram_bot.h"

#include "esp32_camera.h"
#include "config.h"
#include "application.h"

#include <esp_log.h>
#include <esp_heap_caps.h>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <esp_system.h>
#include <esp_mac.h>
#include <esp_wifi.h>
#include <esp_timer.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <cJSON.h>
#include <cstring>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cmath>

#define TAG "TelegramBot"

static std::string ToLower(const std::string &str) {
    std::string out = str;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return std::tolower(c); });
    return out;
}

static bool ContainsWord(const std::string &text, const char *word) {
    return text.find(word) != std::string::npos;
}

TelegramBot::TelegramBot(Esp32Camera *camera)
    : camera_(camera) {
    // Read Telegram configuration from NVS namespace "telegram"
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("telegram", NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        char buf[256] = {0};
        size_t len = sizeof(buf);
        if (nvs_get_str(nvs_handle, "token", buf, &len) == ESP_OK) {
            token_ = buf;
        }
        len = sizeof(buf);
        if (nvs_get_str(nvs_handle, "chat_id", buf, &len) == ESP_OK) {
            chat_id_ = buf;
        }
        nvs_close(nvs_handle);
    }

    if (token_.empty()) {
        token_ = TELEGRAM_BOT_TOKEN;
    }
    if (chat_id_.empty()) {
        chat_id_ = TELEGRAM_CHAT_ID;
    }

    LoadReplyMode();

    out_queue_ = xQueueCreate(20, sizeof(Message *));
    photo_queue_ = xQueueCreate(4, sizeof(PhotoMessage *));
}

TelegramBot::~TelegramBot() {
    running_ = false;
    if (task_handle_ != nullptr) {
        vTaskDelete(task_handle_);
        task_handle_ = nullptr;
    }
    if (out_queue_ != nullptr) {
        vQueueDelete(out_queue_);
        out_queue_ = nullptr;
    }
    if (photo_queue_ != nullptr) {
        vQueueDelete(photo_queue_);
        photo_queue_ = nullptr;
    }
}

void TelegramBot::Start() {
    if (running_) return;
    if (token_.empty() || token_ == "YOUR_TELEGRAM_BOT_TOKEN_HERE") {
        ESP_LOGW(TAG, "Telegram Bot Token not configured. Telegram bot paused.");
        return;
    }
    running_ = true;

    auto &app = Application::GetInstance();
    app.GetAudioService().SetPlaybackMute(reply_mode_ == ReplyMode::CHAT);
    ESP_LOGI(TAG, "Telegram Bot background task started. Reply mode: %d", (int)reply_mode_);

    xTaskCreatePinnedToCore(Task, "telegram_bot", 16384, this, 3, &task_handle_, 1);
}

void TelegramBot::LoadReplyMode() {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("telegram", NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        int8_t mode = 0;
        if (nvs_get_i8(nvs_handle, "reply_mode", &mode) == ESP_OK) {
            if (mode >= (int8_t)ReplyMode::BOTH && mode <= (int8_t)ReplyMode::VOICE) {
                reply_mode_ = (ReplyMode)mode;
            }
        }
        nvs_close(nvs_handle);
    }
}

void TelegramBot::SaveReplyMode(ReplyMode mode) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("telegram", NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        int8_t value = (int8_t)mode;
        nvs_set_i8(nvs_handle, "reply_mode", value);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
    }
}

void TelegramBot::PushConversation(const char *role, const std::string &text) {
    if (!running_ || text.empty()) return;
    if (reply_mode_ == ReplyMode::VOICE) return;
    bool is_user = role != nullptr && (strcmp(role, "user") == 0 || strcmp(role, "Human") == 0);
    bool is_assistant = role != nullptr && strcmp(role, "assistant") == 0;
    if (!is_user && !is_assistant) return;
    std::string formatted = is_user ? "🎤 *You*:\n" + text : "🤖 *Lily*:\n" + text;
    Enqueue(formatted);
}

void TelegramBot::PushSystem(const std::string &text) {
    if (!running_ || text.empty()) return;
    Enqueue("⚙️ *System*:\n" + text);
}

void TelegramBot::PushPhoto(const uint8_t *jpeg_data, size_t jpeg_len) {
    if (!running_ || jpeg_data == nullptr || jpeg_len == 0 || photo_queue_ == nullptr) return;
    uint8_t *copy = (uint8_t *)heap_caps_malloc(jpeg_len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (copy == nullptr) {
        ESP_LOGW(TAG, "Failed to allocate memory for photo upload, dropped photo.");
        return;
    }
    memcpy(copy, jpeg_data, jpeg_len);
    PhotoMessage *photo = new PhotoMessage{copy, jpeg_len};
    if (xQueueSend(photo_queue_, &photo, pdMS_TO_TICKS(100)) != pdTRUE) {
        heap_caps_free(copy);
        delete photo;
        ESP_LOGW(TAG, "Telegram photo queue full, dropped photo.");
    }
}

void TelegramBot::Enqueue(const std::string &text) {
    if (out_queue_ == nullptr) return;
    Message *msg = new Message{text};
    if (xQueueSend(out_queue_, &msg, pdMS_TO_TICKS(100)) != pdTRUE) {
        delete msg;
        ESP_LOGW(TAG, "Telegram outbound queue full, dropped message.");
    }
}

void TelegramBot::Task(void *arg) {
    TelegramBot *bot = static_cast<TelegramBot *>(arg);
    bot->Run();
    vTaskDelete(NULL);
}

void TelegramBot::Run() {
    int64_t last_update_id = 0;
    while (running_) {
        // Wait until Wi-Fi is connected and network stack is ready before making HTTP requests
        esp_netif_ip_info_t ip_info;
        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        bool wifi_connected = (netif != nullptr && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK && ip_info.ip.addr != 0);

        if (!wifi_connected) {
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }
        if (!reported_polling_) {
            ESP_LOGI(TAG, "WiFi connected, Telegram polling started");
            reported_polling_ = true;
        }
        poll_count_++;
        if (poll_count_ % 15 == 0) {
            ESP_LOGI(TAG, "Telegram bot alive, polls=%d", poll_count_);
        }

        // 1. Flush outbound message queue
        Message *msg = nullptr;
        while (xQueueReceive(out_queue_, &msg, 0) == pdTRUE) {
            if (msg != nullptr) {
                if (!chat_id_.empty() && chat_id_ != "YOUR_TELEGRAM_CHAT_ID_HERE") {
                    SendMessage(chat_id_, msg->text);
                }
                delete msg;
            }
        }

        // 2. Flush outbound photo queue
        PhotoMessage *photo = nullptr;
        while (xQueueReceive(photo_queue_, &photo, 0) == pdTRUE) {
            if (photo != nullptr) {
                if (!chat_id_.empty() && chat_id_ != "YOUR_TELEGRAM_CHAT_ID_HERE") {
                    SendPhoto(chat_id_, photo->data, photo->len);
                }
                heap_caps_free(photo->data);
                delete photo;
            }
        }

        // 2. Poll Telegram for updates (getUpdates)
        std::string updates = GetUpdates(last_update_id + 1, 2);
        if (!updates.empty()) {
            cJSON *root = cJSON_Parse(updates.c_str());
            if (root != nullptr) {
                cJSON *ok = cJSON_GetObjectItem(root, "ok");
                if (cJSON_IsTrue(ok)) {
                    cJSON *result = cJSON_GetObjectItem(root, "result");
                    if (cJSON_IsArray(result)) {
                        int size = cJSON_GetArraySize(result);
                        for (int i = 0; i < size; i++) {
                            cJSON *item = cJSON_GetArrayItem(result, i);
                            cJSON *update_id = cJSON_GetObjectItem(item, "update_id");
                            if (cJSON_IsNumber(update_id)) {
                                if (update_id->valueint >= last_update_id) {
                                    last_update_id = update_id->valueint;
                                }
                            }

                            cJSON *msg_obj = cJSON_GetObjectItem(item, "message");
                            if (msg_obj == nullptr) {
                                msg_obj = cJSON_GetObjectItem(item, "channel_post");
                            }
                            if (msg_obj != nullptr) {
                                cJSON *chat = cJSON_GetObjectItem(msg_obj, "chat");
                                cJSON *text = cJSON_GetObjectItem(msg_obj, "text");
                                cJSON *from = cJSON_GetObjectItem(msg_obj, "from");

                                std::string chat_id_str;
                                if (chat != nullptr) {
                                    cJSON *id = cJSON_GetObjectItem(chat, "id");
                                    if (cJSON_IsNumber(id)) {
                                        chat_id_str = std::to_string((int64_t)id->valuedouble);
                                    }
                                }

                                std::string from_user = "User";
                                if (from != nullptr) {
                                    cJSON *first_name = cJSON_GetObjectItem(from, "first_name");
                                    if (cJSON_IsString(first_name)) {
                                        from_user = first_name->valuestring;
                                    }
                                }

                                if (cJSON_IsString(text) && !chat_id_str.empty()) {
                                    HandleCommand(text->valuestring, chat_id_str, from_user);
                                }
                            }
                        }
                    }
                }
                cJSON_Delete(root);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

std::string TelegramBot::GetUpdates(int64_t offset, int timeout_sec) {
    ESP_LOGI(TAG, "getUpdates begin, offset=%lld", (long long)offset);
    std::string url = "https://api.telegram.org/bot" + token_ + "/getUpdates?offset=" + std::to_string(offset) + "&timeout=" + std::to_string(timeout_sec);
    
    char *response_buf = (char *)malloc(4096);
    if (!response_buf) return "";
    memset(response_buf, 0, 4096);

    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.timeout_ms = 15000;
    config.crt_bundle_attach = esp_crt_bundle_attach;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    std::string result = "";
    if (client != nullptr) {
        esp_err_t err = esp_http_client_open(client, 0);
        if (err == ESP_OK) {
            esp_http_client_fetch_headers(client);
            int data_read = esp_http_client_read(client, response_buf, 4095);
            if (data_read > 0) {
                response_buf[data_read] = 0;
                result = response_buf;
            } else {
                ESP_LOGE(TAG, "getUpdates read failed, data_read=%d", data_read);
            }
        } else {
            ESP_LOGE(TAG, "getUpdates open failed: %s", esp_err_to_name(err));
        }
        esp_http_client_cleanup(client);
    } else {
        ESP_LOGE(TAG, "getUpdates http client init failed");
    }
    free(response_buf);
    return result;
}

bool TelegramBot::SendMessage(const std::string &target_chat_id, const std::string &text) {
    std::string url = "https://api.telegram.org/bot" + token_ + "/sendMessage";
    
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "chat_id", target_chat_id.c_str());
    cJSON_AddStringToObject(json, "text", text.c_str());
    cJSON_AddStringToObject(json, "parse_mode", "Markdown");
    char *post_data = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    if (!post_data) return false;

    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.method = HTTP_METHOD_POST;
    config.timeout_ms = 15000;
    config.crt_bundle_attach = esp_crt_bundle_attach;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    bool success = false;
    if (client != nullptr) {
        esp_http_client_set_header(client, "Content-Type", "application/json");
        esp_http_client_set_post_field(client, post_data, strlen(post_data));
        esp_err_t err = esp_http_client_perform(client);
        if (err == ESP_OK) {
            int status = esp_http_client_get_status_code(client);
            if (status == 200) {
                success = true;
            } else {
                ESP_LOGE(TAG, "sendMessage HTTP status: %d", status);
            }
        } else {
            ESP_LOGE(TAG, "sendMessage HTTP failed: %s", esp_err_to_name(err));
        }
        esp_http_client_cleanup(client);
    }
    free(post_data);
    return success;
}

bool TelegramBot::SendPhoto(const std::string &target_chat_id, const uint8_t *jpeg_data, size_t jpeg_len, const std::string &filename) {
    if (!jpeg_data || jpeg_len == 0) return false;

    std::string url = "https://api.telegram.org/bot" + token_ + "/sendPhoto";
    std::string boundary = "----ESP32TelegramBoundary" + std::to_string(esp_random());

    std::string header_chat = "--" + boundary + "\r\n"
        "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n" + target_chat_id + "\r\n";
    std::string header_photo = "--" + boundary + "\r\n"
        "Content-Disposition: form-data; name=\"photo\"; filename=\"" + filename + "\"\r\n"
        "Content-Type: image/jpeg\r\n\r\n";
    std::string footer = "\r\n--" + boundary + "--\r\n";

    size_t total_len = header_chat.length() + header_photo.length() + jpeg_len + footer.length();

    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.method = HTTP_METHOD_POST;
    config.timeout_ms = 15000;
    config.crt_bundle_attach = esp_crt_bundle_attach;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    bool success = false;
    if (client != nullptr) {
        std::string content_type = "multipart/form-data; boundary=" + boundary;
        esp_http_client_set_header(client, "Content-Type", content_type.c_str());
        
        esp_err_t err = esp_http_client_open(client, total_len);
        if (err == ESP_OK) {
            esp_http_client_write(client, header_chat.c_str(), header_chat.length());
            esp_http_client_write(client, header_photo.c_str(), header_photo.length());
            esp_http_client_write(client, (const char *)jpeg_data, jpeg_len);
            esp_http_client_write(client, footer.c_str(), footer.length());

            esp_http_client_fetch_headers(client);
            int status = esp_http_client_get_status_code(client);
            if (status == 200) {
                success = true;
                ESP_LOGI(TAG, "Photo successfully uploaded to Telegram!");
            } else {
                ESP_LOGE(TAG, "sendPhoto HTTP status error: %d", status);
            }
        } else {
            ESP_LOGE(TAG, "sendPhoto open failed: %s", esp_err_to_name(err));
        }
        esp_http_client_cleanup(client);
    }
    return success;
}

bool TelegramBot::SendAudioWav(const std::string &target_chat_id, const uint8_t *wav_data, size_t wav_len) {
    if (!wav_data || wav_len == 0) return false;

    std::string url = "https://api.telegram.org/bot" + token_ + "/sendAudio";
    std::string boundary = "----ESP32TelegramBoundary" + std::to_string(esp_random());

    std::string header_chat = "--" + boundary + "\r\n"
        "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n" + target_chat_id + "\r\n";
    std::string header_audio = "--" + boundary + "\r\n"
        "Content-Disposition: form-data; name=\"audio\"; filename=\"mic.wav\"\r\n"
        "Content-Type: audio/wav\r\n\r\n";
    std::string footer = "\r\n--" + boundary + "--\r\n";

    size_t total_len = header_chat.length() + header_audio.length() + wav_len + footer.length();

    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.method = HTTP_METHOD_POST;
    config.timeout_ms = 20000;
    config.crt_bundle_attach = esp_crt_bundle_attach;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    bool success = false;
    if (client != nullptr) {
        std::string content_type = "multipart/form-data; boundary=" + boundary;
        esp_http_client_set_header(client, "Content-Type", content_type.c_str());

        esp_err_t err = esp_http_client_open(client, total_len);
        if (err == ESP_OK) {
            esp_http_client_write(client, header_chat.c_str(), header_chat.length());
            esp_http_client_write(client, header_audio.c_str(), header_audio.length());
            esp_http_client_write(client, (const char *)wav_data, wav_len);
            esp_http_client_write(client, footer.c_str(), footer.length());

            esp_http_client_fetch_headers(client);
            int status = esp_http_client_get_status_code(client);
            if (status == 200) {
                success = true;
                ESP_LOGI(TAG, "Audio successfully uploaded to Telegram!");
            } else {
                ESP_LOGE(TAG, "sendAudio HTTP status error: %d", status);
            }
        } else {
            ESP_LOGE(TAG, "sendAudio open failed: %s", esp_err_to_name(err));
        }
        esp_http_client_cleanup(client);
    }
    return success;
}

void TelegramBot::RecordAndSend(const std::string &target_chat_id) {
    const int sample_rate = 16000;
    const int duration_ms = 5000;
    const size_t total_samples = (size_t)sample_rate * duration_ms / 1000;

    int16_t *pcm = (int16_t *)heap_caps_malloc(total_samples * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (pcm == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate recording buffer");
        SendMessage(target_chat_id, "❌ Failed to allocate recording buffer.");
        return;
    }

    auto &audio = Application::GetInstance().GetAudioService();
    audio.EnableVoiceProcessing(false);
    audio.EnableWakeWordDetection(false);
    vTaskDelay(pdMS_TO_TICKS(300));

    size_t recorded = 0;
    const int chunk_samples = 320;
    std::vector<int16_t> chunk;
    uint64_t start_us = esp_timer_get_time();
    while (recorded < total_samples && (esp_timer_get_time() - start_us) < (uint64_t)(duration_ms + 3000) * 1000) {
        if (audio.ReadAudioData(chunk, sample_rate, chunk_samples) && !chunk.empty()) {
            size_t n = std::min(chunk.size(), total_samples - recorded);
            memcpy(pcm + recorded, chunk.data(), n * sizeof(int16_t));
            recorded += n;
        } else {
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }

    audio.EnableWakeWordDetection(true);
    audio.EnableVoiceProcessing(true);

    std::string first_samples;
    for (size_t i = 0; i < 24 && i < recorded; i++) {
        first_samples += std::to_string(pcm[i]) + " ";
    }
    ESP_LOGI(TAG, "recording first samples: %s", first_samples.c_str());
    int64_t sum = 0;
    int32_t peak = 0;
    int64_t sum_sq = 0;
    int saturated = 0;
    for (size_t i = 0; i < recorded; i++) {
        int32_t v = pcm[i];
        sum += v;
        if (v < 0) v = -v;
        if (v > peak) peak = v;
        sum_sq += (int64_t)v * v;
        if (v > 30000) saturated++;
    }
    double mean = (double)sum / (double)recorded;
    double rms = sqrt((double)sum_sq / (double)recorded);
    ESP_LOGI(TAG, "recording stats: samples=%d peak=%d rms=%.0f dc=%.1f saturated=%d",
             (int)recorded, peak, rms, mean, saturated);

    double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;
    for (size_t i = 0; i < recorded; i++) {
        double x = pcm[i];
        double y = 0.894884 * x - 1.789768 * x2 + 0.894884 * x1 + 1.778631 * y1 - 0.800802 * y2;
        int32_t v = (int32_t)y;
        pcm[i] = (v > INT16_MAX) ? INT16_MAX : (v < -INT16_MAX) ? -INT16_MAX : (int16_t)v;
        x2 = x1;
        x1 = x;
        y2 = y1;
        y1 = y;
    }
    int64_t fsum = 0;
    int32_t fpeak = 0;
    int64_t fsum_sq = 0;
    int fsat = 0;
    for (size_t i = 0; i < recorded; i++) {
        int32_t v = pcm[i];
        fsum += v;
        if (v < 0) v = -v;
        if (v > fpeak) fpeak = v;
        fsum_sq += (int64_t)v * v;
        if (v > 30000) fsat++;
    }
    double frms = sqrt((double)fsum_sq / (double)recorded);
    ESP_LOGI(TAG, "recording filtered: samples=%d peak=%d rms=%.0f saturated=%d",
             (int)recorded, fpeak, frms, fsat);

    if (recorded < sample_rate) {
        ESP_LOGE(TAG, "Not enough audio captured, recorded=%d samples", (int)recorded);
        SendMessage(target_chat_id, "❌ Not enough audio captured. Please try again.");
        heap_caps_free(pcm);
        return;
    }

    const uint32_t data_size = (uint32_t)recorded * sizeof(int16_t);
    const uint32_t byte_rate = (uint32_t)sample_rate * 2;
    uint8_t *wav = (uint8_t *)heap_caps_malloc(44 + data_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (wav == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate WAV buffer");
        SendMessage(target_chat_id, "❌ Failed to allocate WAV buffer.");
        heap_caps_free(pcm);
        return;
    }

    memcpy(wav, "RIFF", 4);
    uint32_t chunk_size = 36 + data_size;
    memcpy(wav + 4, &chunk_size, 4);
    memcpy(wav + 8, "WAVE", 4);
    memcpy(wav + 12, "fmt ", 4);
    uint32_t fmt_size = 16;
    memcpy(wav + 16, &fmt_size, 4);
    uint16_t audio_format = 1;
    memcpy(wav + 20, &audio_format, 2);
    uint16_t num_channels = 1;
    memcpy(wav + 22, &num_channels, 2);
    memcpy(wav + 24, &sample_rate, 4);
    memcpy(wav + 28, &byte_rate, 4);
    uint16_t block_align = 2;
    memcpy(wav + 32, &block_align, 2);
    uint16_t bits_per_sample = 16;
    memcpy(wav + 34, &bits_per_sample, 2);
    memcpy(wav + 36, "data", 4);
    memcpy(wav + 40, &data_size, 4);
    memcpy(wav + 44, pcm, data_size);
    heap_caps_free(pcm);

    ESP_LOGI(TAG, "Recording captured %d samples, uploading WAV (%u bytes)", (int)recorded, 44 + data_size);
    bool ok = SendAudioWav(target_chat_id, wav, 44 + data_size);
    if (!ok) {
        SendMessage(target_chat_id, "⚠️ Recording captured but upload to Telegram failed.");
    }
    heap_caps_free(wav);
}

void TelegramBot::CheckMicLevels() {
    auto &audio = Application::GetInstance().GetAudioService();
    auto codec = Board::GetInstance().GetAudioCodec();
    audio.EnableVoiceProcessing(false);
    audio.EnableWakeWordDetection(false);
    vTaskDelay(pdMS_TO_TICKS(200));

    auto measure = [&](const char *label) -> void {
        std::vector<int16_t> chunk;
        std::vector<int16_t> samples;
        int64_t sum = 0;
        int64_t sum_sq = 0;
        int32_t peak = 0;
        int saturated = 0;
        size_t n = 0;
        int crossings = 0;
        uint64_t start_us = esp_timer_get_time();
        while ((esp_timer_get_time() - start_us) < 1200ULL * 1000ULL) {
            if (audio.ReadAudioData(chunk, 16000, 320) && !chunk.empty()) {
                for (auto v : chunk) {
                    if (n > 0 && ((samples.back() < 0 && v >= 0) || (samples.back() >= 0 && v < 0))) {
                        crossings++;
                    }
                    sum += v;
                    int32_t a = v < 0 ? -v : v;
                    sum_sq += (int64_t)a * a;
                    if (a > peak) peak = a;
                    if (a > 30000) saturated++;
                    n++;
                    samples.push_back(v);
                }
            } else {
                vTaskDelay(pdMS_TO_TICKS(5));
            }
        }
        double rms = n > 0 ? sqrt((double)sum_sq / (double)n) : 0.0;
        double dc = n > 0 ? (double)sum / (double)n : 0.0;
        double zcr = n > 0 ? (double)crossings * 16000.0 / (double)n : 0.0;
        double best_lag = 0.0;
        if (n > 600) {
            size_t max_lag = std::min<size_t>(1600, n / 2);
            size_t best = 1;
            int64_t best_sum = 0;
            for (size_t lag = 40; lag <= max_lag; lag++) {
                int64_t corr = 0;
                size_t cnt = 0;
                for (size_t i = 0; i + lag < n && cnt < 800; i += 4, cnt++) {
                    corr += (int64_t)samples[i] * samples[i + lag];
                }
                if (corr > best_sum) {
                    best_sum = corr;
                    best = lag;
                }
            }
            best_lag = (double)best;
        }
        double freq = best_lag > 0 ? 16000.0 / best_lag : 0.0;
        ESP_LOGI(TAG, "mic %s: peak=%d rms=%.0f dc=%.1f sat=%d zcr=%.0fHz lagFreq=%.0fHz samples=%d",
            label, peak, rms, dc, saturated, zcr, freq, (int)n);
        std::string dump;
        for (size_t i = 0; i < samples.size() && i < 40; i++) {
            if (i > 0) dump += ",";
            dump += std::to_string(samples[i]);
        }
        ESP_LOGI(TAG, "mic %s first: %s", label, dump.c_str());
    };

    codec->EnableOutput(false);
    vTaskDelay(pdMS_TO_TICKS(300));
    measure("spk-off");

    codec->EnableOutput(true);
    vTaskDelay(pdMS_TO_TICKS(300));
    measure("spk-on");

    audio.EnableWakeWordDetection(true);
    audio.EnableVoiceProcessing(true);
}

void TelegramBot::CaptureAndSendPhotos(const std::string &chat_id) {
    if (camera_ != nullptr) {
        camera_->EnsureInitialized();
    }
    camera_fb_t *fb = nullptr;
    for (int i = 0; i < 15; i++) {
        if (fb != nullptr) {
            esp_camera_fb_return(fb);
        }
        fb = esp_camera_fb_get();
    }
    if (fb == nullptr) {
        SendMessage(chat_id, "❌ Failed to capture image frame from camera sensor.");
        return;
    }
    ESP_LOGI(TAG, "Photo frame: format=%d w=%d h=%d len=%u", fb->format, fb->width, fb->height, (unsigned)fb->len);

    if (fb->format == PIXFORMAT_JPEG) {
        SendPhoto(chat_id, fb->buf, fb->len, "photo.jpg");
        esp_camera_fb_return(fb);
        return;
    }

    if (fb->format == PIXFORMAT_YUV422 || fb->format == PIXFORMAT_RGB565) {
        uint8_t *jpg_buf = nullptr;
        size_t jpg_len = 0;
        bool ok = frame2jpg(fb, 80, &jpg_buf, &jpg_len);
        if (ok && jpg_buf != nullptr && jpg_len > 0) {
            SendPhoto(chat_id, jpg_buf, jpg_len, "photo.jpg");
            free(jpg_buf);
        } else {
            SendMessage(chat_id, "❌ Failed to convert frame to JPEG format.");
        }
    } else {
        SendMessage(chat_id, "❌ Unsupported frame format: " + std::to_string(fb->format));
    }
    esp_camera_fb_return(fb);
}

void TelegramBot::HandleCommand(const std::string &command, const std::string &sender_chat_id, const std::string &from_user) {
    ESP_LOGI(TAG, "Received message '%s' from %s (%s)", command.c_str(), from_user.c_str(), sender_chat_id.c_str());

    if (command == "/start" || command == "/help") {
        std::string help_txt = "👋 *Lily AI Assistant - Commands*:\n\n"
                               "• `/status` - Device status\n"
                               "• `/photo` - Take a photo\n"
                               "• `/chat` - Text replies only (no speaker)\n"
                               "• `/voice` - Voice replies only\n"
                               "• `/both` - Voice and text replies\n\n"
                               "You can also just type things like:\n"
                               "• \"take a photo\"\n"
                               "• \"hi there\" (AI chat)";
        SendMessage(sender_chat_id, help_txt);
    } else if (command == "/status") {
        wifi_ap_record_t ap_info;
        int rssi = -100;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            rssi = ap_info.rssi;
        }

        esp_netif_ip_info_t ip_info;
        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        std::string ip_str = "0.0.0.0";
        if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
            char ip_buf[32];
            esp_ip4addr_ntoa(&ip_info.ip, ip_buf, sizeof(ip_buf));
            ip_str = ip_buf;
        }

        uint8_t mac[6];
        esp_efuse_mac_get_default(mac);
        char mac_buf[32];
        snprintf(mac_buf, sizeof(mac_buf), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        const char *mode_str = reply_mode_ == ReplyMode::CHAT ? "Chat only"
                             : reply_mode_ == ReplyMode::VOICE ? "Voice only"
                                                               : "Both";

        std::stringstream ss;
        ss << "📊 *Lily ESP32-S3 Status*:\n"
           << "• *IP Address*: `" << ip_str << "`\n"
           << "• *MAC*: `" << mac_buf << "`\n"
           << "• *Wi-Fi RSSI*: `" << rssi << " dBm`\n"
           << "• *Reply Mode*: `" << mode_str << "`\n"
           << "• *Free Heap*: `" << esp_get_free_heap_size() << " bytes`";
        SendMessage(sender_chat_id, ss.str());
    } else if (command == "/chat") {
        reply_mode_ = ReplyMode::CHAT;
        SaveReplyMode(reply_mode_);
        Application::GetInstance().GetAudioService().SetPlaybackMute(true);
        SendMessage(sender_chat_id, "🔕 *Chat mode enabled*: Lily replies with text messages only, speaker is muted.");
    } else if (command == "/voice") {
        reply_mode_ = ReplyMode::VOICE;
        SaveReplyMode(reply_mode_);
        Application::GetInstance().GetAudioService().SetPlaybackMute(false);
        SendMessage(sender_chat_id, "🔊 *Voice mode enabled*: Lily replies with voice only, no Telegram transcripts.");
    } else if (command == "/both") {
        reply_mode_ = ReplyMode::BOTH;
        SaveReplyMode(reply_mode_);
        Application::GetInstance().GetAudioService().SetPlaybackMute(false);
        SendMessage(sender_chat_id, "🔊💬 *Both mode enabled*: Lily replies with voice and Telegram text.");
    } else if (command == "/record" || command == "/testmic") {
        SendMessage(sender_chat_id, "🎙 Recording 5 seconds of microphone audio, please speak now...");
        RecordAndSend(sender_chat_id);
    } else if (command == "/photo") {
        SendMessage(sender_chat_id, "📷 Capturing photo from camera...");
        CaptureAndSendPhotos(sender_chat_id);
    } else {
        // Natural language commands and AI chat
        std::string lower = ToLower(command);

        bool take_photo = ContainsWord(lower, "photo") || ContainsWord(lower, "picture") ||
                          ContainsWord(lower, "capture") || ContainsWord(lower, "camera");
        if (take_photo) {
            SendMessage(sender_chat_id, "📷 Capturing photo from camera...");
            CaptureAndSendPhotos(sender_chat_id);
            return;
        }

        bool wake_up = lower == "hi, lily" || lower == "hi lily" || lower == "hey lily" || lower == "wake up";
        if (wake_up) {
            if (reply_mode_ != ReplyMode::VOICE) {
                PushConversation("user", "Hi, Lily");
            }
            SendMessage(sender_chat_id, "✨ Lily is awake and listening!");
            Application::GetInstance().WakeWordInvoke("Hi, Lily");
            return;
        }

        // Fall through: treat as an AI conversation message.
        if (reply_mode_ != ReplyMode::VOICE) {
            PushConversation("user", command);
        }
        SendMessage(sender_chat_id, "💬 I'll reply shortly...");
        Application::GetInstance().WakeWordInvoke(command);
    }
}
