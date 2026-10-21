#include "face_service.h"

#include <cstring>

#include "application.h"
#include "board.h"
#include "d1001_camera.h"
#include "mcp_server.h"
#include "settings.h"

#include <cJSON.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <mbedtls/base64.h>

#include "human_face_detect.hpp"  // brings esp-dl's V4L2_PIX_FMT_* defines

static const char* TAG = "FaceService";

namespace {
enum class DetectState { kIdle, kValidating, kCooldown };

// The official cloud rejects listen/state=detect texts over ~25 UTF-8 bytes
// with an ERROR alert screen (see commit 2dceb7c: tags <f></f>/<d></d>, 18
// content bytes). Truncate on a character boundary.
std::string TruncateUtf8(const std::string& s, size_t max_bytes) {
    if (s.size() <= max_bytes) {
        return s;
    }
    size_t end = max_bytes;
    while (end > 0 && (static_cast<unsigned char>(s[end]) & 0xC0) == 0x80) {
        --end;
    }
    return s.substr(0, end);
}
}

FaceService::FaceService(D1001Camera* camera) : camera_(camera) {}

void FaceService::LoadConfig() {
    Settings settings("face", false);
    mode_ = settings.GetInt("mode", 0);
    interval_s_ = settings.GetInt("interval_s", 5);
    threshold_ = settings.GetInt("threshold", 60);
    duration_s_ = settings.GetInt("duration_s", 2);
    cooldown_s_ = settings.GetInt("cooldown_s", 8);
    known_only_ = settings.GetInt("known_only", 1);
    api_ = settings.GetInt("api", 0);
    det_thr0_ = settings.GetInt("det_thr0", 5);
    det_thr1_ = settings.GetInt("det_thr1", 50);
    std::lock_guard<std::mutex> lock(mutex_);
    endpoint_ = settings.GetString("endpoint", "");
}

void FaceService::Start() {
    LoadConfig();
    RegisterMcpTools();
    xTaskCreate(
        [](void* arg) {
            static_cast<FaceService*>(arg)->WatchLoop();
            vTaskDelete(nullptr);
        },
        "face_watch", 8192, this, 2, &task_);
    ESP_LOGI(TAG, "started (mode=%d, endpoint %s)", mode_.load(),
             endpoint_.empty() ? "unset" : "set");
}

bool FaceService::RecognizeOnce(std::vector<FaceHit>& hits, std::string* error) {
    std::string endpoint;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        endpoint = endpoint_;
    }
    if (endpoint.empty()) {
        if (error) *error = "no endpoint configured";
        return false;
    }
    if (camera_ == nullptr) {
        if (error) *error = "no camera";
        return false;
    }
    std::vector<uint8_t> jpeg;
    camera_->SetPreviewEnabled(false);
    bool captured = camera_->CaptureToJpeg(jpeg);
    camera_->SetPreviewEnabled(true);
    if (!captured || jpeg.empty()) {
        if (error) *error = "capture failed";
        return false;
    }

    auto network = Board::GetInstance().GetNetwork();
    auto http = network->CreateHttp(3);
    http->SetHeader("Transfer-Encoding", "chunked");
    if (api_.load() == 1) {
        // SenseCraft-style recognizer: {"image_base64": "..."}
        size_t b64_len = 0;
        std::string b64((jpeg.size() + 2) / 3 * 4 + 4, '\0');
        if (mbedtls_base64_encode(reinterpret_cast<unsigned char*>(b64.data()), b64.size(),
                                  &b64_len, jpeg.data(), jpeg.size()) != 0) {
            if (error) *error = "base64 encode failed";
            return false;
        }
        b64.resize(b64_len);
        http->SetHeader("Content-Type", "application/json");
        if (!http->Open("POST", endpoint)) {
            if (error) *error = "connect failed";
            return false;
        }
        static const char kPrefix[] = "{\"image_base64\":\"";
        http->Write(kPrefix, sizeof(kPrefix) - 1);
        http->Write(b64.data(), b64.size());
        http->Write("\"}", 2);
    } else {
        http->SetHeader("Content-Type", "image/jpeg");
        if (!http->Open("POST", endpoint)) {
            if (error) *error = "connect failed";
            return false;
        }
        http->Write(reinterpret_cast<const char*>(jpeg.data()), jpeg.size());
    }
    http->Write("", 0);
    auto status_res = http->GetStatusCode();
    int status = status_res.value_or(0);
    std::string body = http->ReadAll();
    http->Close();
    if (status != 200) {
        if (error) *error = "recognizer status " + std::to_string(status);
        return false;
    }

    cJSON* root = cJSON_Parse(body.c_str());
    if (root == nullptr) {
        if (error) *error = "invalid recognizer JSON";
        return false;
    }
    hits.clear();
    cJSON* faces = cJSON_GetObjectItem(root, "faces");
    if (faces != nullptr) {
        // Generic shape: {"faces":[{"name","score"}]}
        cJSON* face = nullptr;
        cJSON_ArrayForEach(face, faces) {
            cJSON* name = cJSON_GetObjectItem(face, "name");
            cJSON* score = cJSON_GetObjectItem(face, "score");
            FaceHit hit;
            hit.name = cJSON_IsString(name) ? name->valuestring : "unknown";
            double value = cJSON_IsNumber(score) ? score->valuedouble : 0;
            hit.score = static_cast<int>(value <= 1.0 ? value * 100 : value);
            hits.push_back(std::move(hit));
        }
    } else if (cJSON_GetObjectItem(root, "matched") != nullptr) {
        // SenseCraft single-face shape: {matched, name, confidence, live, ...}
        cJSON* matched = cJSON_GetObjectItem(root, "matched");
        if (cJSON_IsTrue(matched)) {
            cJSON* name = cJSON_GetObjectItem(root, "name");
            cJSON* confidence = cJSON_GetObjectItem(root, "confidence");
            FaceHit hit;
            hit.name = cJSON_IsString(name) ? name->valuestring : "unknown";
            double value = cJSON_IsNumber(confidence) ? confidence->valuedouble : 0;
            hit.score = static_cast<int>(value <= 1.0 ? value * 100 : value);
            hits.push_back(std::move(hit));
        }
    }
    cJSON_Delete(root);

    // Record for /face/status
    std::string summary = "[";
    for (size_t i = 0; i < hits.size(); ++i) {
        if (i) summary += ",";
        summary += "{\"name\":\"" + hits[i].name +
                   "\",\"score\":" + std::to_string(hits[i].score) + "}";
    }
    summary += "]";
    {
        std::lock_guard<std::mutex> lock(mutex_);
        last_result_ = summary;
    }
    last_ts_ms_ = esp_timer_get_time() / 1000;
    return true;
}

bool FaceService::AnyQualifiedHit(const std::vector<FaceHit>& hits, std::string* names) {
    bool any = false;
    for (const auto& hit : hits) {
        if (hit.score < threshold_.load()) {
            continue;
        }
        if (known_only_.load() != 0 && hit.name == "unknown") {
            continue;
        }
        if (names != nullptr) {
            if (any) *names += ", ";
            *names += hit.name;
        }
        any = true;
    }
    return any;
}

bool FaceService::DetectFaceLocal() {
    if (camera_ == nullptr) {
        return false;
    }
    if (detector_ == nullptr) {
        detector_ = new HumanFaceDetect();
    }
    // On ESP32-P4 the MSR stage reports scores around 0.06-0.09 for a large
    // frontal face (roughly 10x below the model's documented calibration),
    // so the default 0.5 stage threshold rejects everything. Run MSR nearly
    // open and let the MNP confirm stage (scores saturate near 1.0 for real
    // faces) do the filtering. Both are tunable via /face/config
    // (det_thr0/det_thr1, percent).
    detector_->set_score_thr(det_thr0_.load() / 100.0f, 0);
    detector_->set_score_thr(det_thr1_.load() / 100.0f, 1);
    std::lock_guard<std::recursive_mutex> capture_lock(camera_->capture_mutex());
    D1001Camera::RawFrame frame;
    if (!camera_->CaptureRaw(frame)) {
        return false;
    }
    dl::image::pix_type_t pix_type;
    switch (frame.v4l2_format) {
        case V4L2_PIX_FMT_RGB565:
            pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB565LE;
            break;
        case V4L2_PIX_FMT_RGB24:
            pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB888;
            break;
        case V4L2_PIX_FMT_YUYV:
            pix_type = dl::image::DL_IMAGE_PIX_TYPE_YUYV;
            break;
        case V4L2_PIX_FMT_UYVY:
            pix_type = dl::image::DL_IMAGE_PIX_TYPE_UYVY;
            break;
        case V4L2_PIX_FMT_GREY:
            pix_type = dl::image::DL_IMAGE_PIX_TYPE_GRAY;
            break;
        default:
            ESP_LOGW(TAG, "unsupported frame format 0x%08lx for local detect",
                     (unsigned long)frame.v4l2_format);
            return false;
    }
    dl::image::img_t img = {};
    img.data = const_cast<uint8_t*>(frame.data);
    img.width = frame.width;
    img.height = frame.height;
    img.pix_type = pix_type;
    // Aspect-preserving letterbox: the MSR stage resizes the whole image to
    // its small 4:3 input (160x120). Feeding 16:9 720p directly means an
    // anisotropic ~8x squash that leaves MSR scores marginal (collapses in
    // poor light). Pre-decimate UYVY 2x into a 4:3 gray canvas so the
    // internal resize is uniform. Wake only uses scores, so the letterbox
    // offset needs no box remapping.
    static std::vector<uint8_t> lb;
    if (pix_type == dl::image::DL_IMAGE_PIX_TYPE_UYVY && (frame.width % 8) == 0 &&
        (frame.height % 2) == 0 && frame.width > 640) {
        const uint16_t iw = frame.width / 2, ih = frame.height / 2;
        const uint16_t cw = iw, ch = (uint16_t)((iw * 3 / 4 + 1) & ~1);
        if (ch >= ih) {
            const uint16_t top = (ch - ih) / 2;
            const size_t canvas_bytes = (size_t)cw * ch * 2;
            static uint16_t last_cw = 0, last_ch = 0, last_top = 0;
            if (lb.size() != canvas_bytes || cw != last_cw || ch != last_ch || top != last_top) {
                lb.assign(canvas_bytes, 0x80);  // UYVY mid-gray
                last_cw = cw; last_ch = ch; last_top = top;
            }
            const uint8_t* srcp = frame.data;
            uint8_t* base = lb.data() + (size_t)top * cw * 2;
            for (uint16_t y = 0; y < ih; ++y) {
                const uint8_t* row = srcp + (size_t)(y * 2) * frame.width * 2;
                uint8_t* d = base + (size_t)y * cw * 2;
                for (uint16_t x = 0; x < cw / 2; ++x) {
                    memcpy(d + (size_t)x * 4, row + (size_t)x * 8, 4);
                }
            }
            img.data = lb.data();
            img.width = cw;
            img.height = ch;
        }
    }
    auto& results = detector_->run(img);
    float best = 0;
    for (const auto& result : results) {
        best = std::max(best, result.score);
    }
    // Diagnostic heartbeat (every ~10 runs) plus per-hit detail.
    static int diag_tick = 0;
    if (!results.empty() || ++diag_tick >= 10) {
        diag_tick = 0;
        ESP_LOGI(TAG, "detect: %ux%u results=%d best=%.2f", img.width, img.height,
                 (int)results.size(), best);
    }
    return best >= det_thr1_.load() / 100.0f;
}

void FaceService::WatchLoop() {
    DetectState state = DetectState::kIdle;
    int64_t state_start_us = 0;
    int64_t last_hit_us = 0;
    int64_t last_recog_us = 0;

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(500));
        int mode = mode_.load();
        if (mode == 0) {
            state = DetectState::kIdle;
            continue;
        }
        if (mode == 2 || mode == 3) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (endpoint_.empty()) {
                continue;
            }
        }
        // Only watch while the assistant is idle: it keeps the camera free
        // for take_photo and avoids waking into an active conversation.
        auto& app = Application::GetInstance();
        if (app.GetDeviceState() != kDeviceStateIdle) {
            continue;
        }
        int64_t now = esp_timer_get_time();
        int64_t gap_us =
            (state == DetectState::kValidating ? 1 : (int64_t)interval_s_.load()) * 1000000LL;
        if (now - last_recog_us < gap_us) {
            continue;
        }
        last_recog_us = now;

        // Local esp-dl detection first (fast, on-device). Mode 1 wakes on any
        // face; mode 2 only forwards to the remote recognizer when a face is
        // actually present, and wakes on known identities.
        bool face_present = DetectFaceLocal();
        bool hit = false;
        std::string names = "人脸";
        if (face_present) {
            if (mode == 1) {
                hit = true;
                names = "有人";
            } else if (mode == 2) {
                // 智能问候：熟人叫名字主动问候，陌生人也唤醒打招呼
                std::vector<FaceHit> hits;
                std::string error;
                if (RecognizeOnce(hits, &error)) {
                    names.clear();
                    bool has_known = false;
                    for (const auto& h : hits) {
                        if (h.score >= threshold_.load() && h.name != "unknown") {
                            if (has_known) names += ", ";
                            names += h.name;
                            has_known = true;
                        }
                    }
                    if (has_known) {
                        hit = true;
                    } else if (!hits.empty()) {
                        names = "朋友";
                        hit = true;
                    }
                } else {
                    ESP_LOGD(TAG, "recognize failed: %s", error.c_str());
                }
            } else if (mode == 3) {
                // 哨兵模式：熟人免打扰，陌生人警戒
                std::vector<FaceHit> hits;
                std::string error;
                if (RecognizeOnce(hits, &error)) {
                    bool has_known = false;
                    for (const auto& h : hits) {
                        if (h.score >= threshold_.load() && h.name != "unknown") {
                            has_known = true;
                            break;
                        }
                    }
                    if (has_known) {
                        hit = false;
                        ESP_LOGD(TAG, "sentry mode: familiar face detected, dnd silent");
                    } else {
                        names = "陌生人";
                        hit = true;
                        ESP_LOGI(TAG, "sentry mode: stranger detected, alerting!");
                    }
                } else {
                    ESP_LOGD(TAG, "sentry recognize failed: %s", error.c_str());
                }
            }
        }

        switch (state) {
            case DetectState::kIdle:
                if (hit) {
                    state = DetectState::kValidating;
                    state_start_us = now;
                    last_hit_us = now;
                    ESP_LOGI(TAG, "face candidate: %s", names.c_str());
                }
                break;
            case DetectState::kValidating:
                if (hit) {
                    last_hit_us = now;
                    if (now - state_start_us >= (int64_t)duration_s_.load() * 1000000LL) {
                        ESP_LOGI(TAG, "face confirmed, waking: %s", names.c_str());
                        // <f>...</f> with 18 content bytes: the official cloud
                        // hard-rejects longer detect texts (2dceb7c).
                        app.WakeWordInvoke("<f>" + TruncateUtf8(names, 18) + "</f>");
                        state = DetectState::kCooldown;
                        state_start_us = now;
                    }
                } else if (now - last_hit_us >= 3 * 1000000LL) {
                    state = DetectState::kIdle;
                }
                break;
            case DetectState::kCooldown:
                if (!hit && now - state_start_us >= (int64_t)cooldown_s_.load() * 1000000LL) {
                    state = DetectState::kIdle;
                    ESP_LOGI(TAG, "cooldown complete");
                }
                break;
        }
    }
}

std::string FaceService::GetEndpoint() {
    std::lock_guard<std::mutex> lock(mutex_);
    return endpoint_;
}

void FaceService::SetModeAndEndpoint(int mode, const std::string& endpoint) {
    Settings settings("face", true);
    if (mode >= 0 && mode <= 3) {
        settings.SetInt("mode", mode);
        mode_ = mode;
    }
    if (!endpoint.empty()) {
        settings.SetString("endpoint", endpoint);
        std::lock_guard<std::mutex> lock(mutex_);
        endpoint_ = endpoint;
    }
    ESP_LOGI(TAG, "face config saved: mode=%d", mode_.load());
}

int FaceService::ToggleMode() {
    int next = (mode_.load() + 1) % 4;
    Settings settings("face", true);
    settings.SetInt("mode", next);
    mode_ = next;
    ESP_LOGI(TAG, "mode toggled to %d", next);
    return next;
}

std::string FaceService::StatusJson() {
    std::lock_guard<std::mutex> lock(mutex_);
    return "{\"mode\":" + std::to_string(mode_.load()) +
           ",\"endpoint\":\"" + endpoint_ + "\"" +
           ",\"interval_s\":" + std::to_string(interval_s_.load()) +
           ",\"threshold\":" + std::to_string(threshold_.load()) +
           ",\"duration_s\":" + std::to_string(duration_s_.load()) +
           ",\"cooldown_s\":" + std::to_string(cooldown_s_.load()) +
           ",\"known_only\":" + std::to_string(known_only_.load()) +
           ",\"api\":" + std::to_string(api_.load()) +
           ",\"det_thr0\":" + std::to_string(det_thr0_.load()) +
           ",\"det_thr1\":" + std::to_string(det_thr1_.load()) +
           ",\"last\":" + last_result_ +
           ",\"last_ts_ms\":" + std::to_string(last_ts_ms_.load()) + "}";
}

bool FaceService::ApplyConfigJson(const std::string& body, std::string* error) {
    cJSON* root = cJSON_Parse(body.c_str());
    if (root == nullptr) {
        if (error) *error = "invalid JSON";
        return false;
    }
    Settings settings("face", true);
    auto set_int = [&](const char* key, std::atomic<int>& target, int lo, int hi) {
        cJSON* item = cJSON_GetObjectItem(root, key);
        if (cJSON_IsNumber(item)) {
            int value = std::max(lo, std::min(hi, item->valueint));
            settings.SetInt(key, value);
            target = value;
        }
    };
    set_int("mode", mode_, 0, 3);
    set_int("interval_s", interval_s_, 1, 3600);
    set_int("threshold", threshold_, 0, 100);
    set_int("duration_s", duration_s_, 0, 60);
    set_int("cooldown_s", cooldown_s_, 0, 3600);
    set_int("known_only", known_only_, 0, 1);
    set_int("api", api_, 0, 1);
    set_int("det_thr0", det_thr0_, 1, 100);
    set_int("det_thr1", det_thr1_, 1, 100);
    cJSON* endpoint = cJSON_GetObjectItem(root, "endpoint");
    if (cJSON_IsString(endpoint)) {
        settings.SetString("endpoint", endpoint->valuestring);
        std::lock_guard<std::mutex> lock(mutex_);
        endpoint_ = endpoint->valuestring;
    }
    cJSON_Delete(root);
    return true;
}

void FaceService::RegisterMcpTools() {
    auto& mcp_server = McpServer::GetInstance();

    mcp_server.AddTool(
        "self.face.verify",
        "识别当前镜头前的人脸并返回身份列表。用于需要确认操作者身份的任务鉴权：\n"
        "在执行敏感操作前调用，根据返回的 name/score 判断当前操作者是否有权限。\n"
        "返回: {\"faces\":[{\"name\":\"harvest\",\"score\":92}]}；faces 为空表示镜头前无人。",
        PropertyList(), [this](const PropertyList&) -> ReturnValue {
            std::vector<FaceHit> hits;
            std::string error;
            if (!RecognizeOnce(hits, &error)) {
                return std::string("{\"error\":\"") + error + "\"}";
            }
            std::string result = "{\"faces\":[";
            for (size_t i = 0; i < hits.size(); ++i) {
                if (i) result += ",";
                result += "{\"name\":\"" + hits[i].name +
                          "\",\"score\":" + std::to_string(hits[i].score) + "}";
            }
            result += "]}";
            return result;
        });

    mcp_server.AddTool(
        "self.face.param_get",
        "获取人脸识别的当前配置：mode(0不感知/1人形感知:有人即唤醒/2智能问候:熟人叫名字/3哨兵模式:熟人免打扰陌生人警戒)、interval_s 采集间隔、"
        "threshold 置信度阈值(0-100)、duration_s 持续确认秒数、cooldown_s 冷却秒数、"
        "known_only(1=只有已知人脸才唤醒)。",
        PropertyList(),
        [this](const PropertyList&) -> ReturnValue { return StatusJson(); });

    mcp_server.AddTool(
        "self.face.param_set",
        "配置人脸识别参数。用户想开启/关闭看到人自动打招呼、调整灵敏度或频率时使用。\n"
        "参数均可选，-1 表示保持不变：mode(0不感知/1人形感知/2智能问候/3哨兵模式)、interval_s、"
        "threshold、duration_s、cooldown_s、known_only。",
        PropertyList({
            Property("mode", kPropertyTypeInteger, -1, -1, 3),
            Property("interval_s", kPropertyTypeInteger, -1, -1, 3600),
            Property("threshold", kPropertyTypeInteger, -1, -1, 100),
            Property("duration_s", kPropertyTypeInteger, -1, -1, 60),
            Property("cooldown_s", kPropertyTypeInteger, -1, -1, 3600),
            Property("known_only", kPropertyTypeInteger, -1, -1, 1),
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            Settings settings("face", true);
            auto apply = [&](const char* key, std::atomic<int>& target) {
                int value = properties[key].value<int>();
                if (value != -1) {
                    settings.SetInt(key, value);
                    target = value;
                }
            };
            apply("mode", mode_);
            apply("interval_s", interval_s_);
            apply("threshold", threshold_);
            apply("duration_s", duration_s_);
            apply("cooldown_s", cooldown_s_);
            apply("known_only", known_only_);
            return StatusJson();
        });
}
