#ifndef RETERMINAL_D1001_FACE_SERVICE_H
#define RETERMINAL_D1001_FACE_SERVICE_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

class D1001Camera;
class HumanFaceDetect;

// One recognized face from the external recognizer.
struct FaceHit {
    std::string name;  // "unknown" = face detected but not in the library
    int score = 0;     // 0-100
};

// Face recognition backed by an external HTTP endpoint (the D1001 has no
// NPU). Mirrors the SenseCAP Watcher port's detection state machine
// (IDLE -> VALIDATING -> wake -> COOLDOWN) but sends JPEG frames to a
// configurable recognizer instead of running a local model.
//
// Endpoint contract:
//   POST <endpoint>  body: image/jpeg
//   200 -> {"faces": [{"name": "harvest", "score": 92}, ...]}
//   score accepts 0-1 floats or 0-100 numbers; empty faces = nobody there.
//
// Two consumers:
//   1. Wake mode: a face persisting for duration_s triggers
//      Application::WakeWordInvoke("<detect>...</detect>").
//   2. MCP tool self.face.verify: on-demand identification so the assistant
//      can authenticate the operator before sensitive tasks.
class FaceService {
public:
    explicit FaceService(D1001Camera* camera);

    // Loads config, registers the MCP tools and spawns the watch task.
    void Start();

    // One synchronous capture + recognition round. Returns false and fills
    // `error` on capture/network/parse failure.
    bool RecognizeOnce(std::vector<FaceHit>& hits, std::string* error = nullptr);

    enum Mode {
        kModeOff = 0,           // 不感知：关闭视觉，不唤醒
        kModeHumanMotion = 1,   // 人形感知：本地人脸检测，有人即唤醒
        kModeSmartGreeting = 2, // 智能问候：熟人叫名字主动问候，陌生人也唤醒
        kModeSentry = 3,        // 哨兵模式：熟人免打扰，陌生人警戒
    };

    // Cycles 0 (off) -> 1 (human motion) -> 2 (smart greeting) -> 3 (sentry);
    // persists and returns the new mode.
    int ToggleMode();
    int GetMode() const { return mode_.load(); }
    std::string GetEndpoint();
    void SetModeAndEndpoint(int mode, const std::string& endpoint);
    // Re-reads every parameter from Settings("face") (after on-screen edits).
    void ReloadConfig() { LoadConfig(); }

    std::string StatusJson();
    bool ApplyConfigJson(const std::string& body, std::string* error);

private:
    void WatchLoop();
    void LoadConfig();
    void RegisterMcpTools();
    bool AnyQualifiedHit(const std::vector<FaceHit>& hits, std::string* names);
    // On-device esp-dl face detection on a raw camera frame.
    bool DetectFaceLocal();

    D1001Camera* camera_;
    TaskHandle_t task_ = nullptr;
    std::mutex mutex_;  // endpoint_ + last_result_

    // Settings namespace "face"
    std::atomic<int> mode_{0};  // 0 off, 1 detect-wake (local), 2 recognize-wake
    HumanFaceDetect* detector_ = nullptr;  // lazy; model streams from its partition
    std::atomic<int> interval_s_{5};  // seconds between recognitions
    std::atomic<int> threshold_{60};  // min score
    std::atomic<int> det_thr0_{5};   // MSR stage score thr (percent)
    std::atomic<int> det_thr1_{50};  // MNP stage score thr (percent)
    std::atomic<int> duration_s_{2};  // face must persist this long to wake
    std::atomic<int> cooldown_s_{8};  // after a wake, wait this long + face gone
    std::atomic<int> known_only_{1};  // 1 = "unknown" faces do not wake
    std::atomic<int> api_{0};         // 0 = raw JPEG POST, 1 = {"image_base64"} JSON
    std::string endpoint_;

    std::string last_result_ = "[]";
    std::atomic<int64_t> last_ts_ms_{0};
};

#endif  // RETERMINAL_D1001_FACE_SERVICE_H
