// HTTP client for the R2D2 Core's satellite API, on a task of its own.
//
// The conversation WebSocket only exists while someone talks; everything else
// the screen shows comes from plain HTTP with the same Device-Id and token:
//   GET  /satellite/status          clock sync and weather; carries telemetry (every minute, idle)
//   GET  /satellite/nav             the touch navigation's areas
//   GET  /satellite/screen/<id>     one screen's content
//   GET  /satellite/camera.jpg      the door camera, already shrunk
//   POST /satellite/action          a tapped item's home action
// Requests are queued and run one at a time off the UI; results go back to
// R2d2Display, which takes its lock. The base URL is the OTA URL's host:
// .../satellite/ota/ → http://host:port.
#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <atomic>
#include <cstdint>
#include <string>

namespace r2d2 {

class R2d2Display;

class CoreClient {
public:
    explicit CoreClient(R2d2Display* display);
    void Start();

    // Non-blocking: queued for the client task. seq comes back with the result
    // so the caller can drop answers it no longer waits for.
    void FetchNav();
    void FetchScreen(const std::string& id, uint32_t seq);
    void FetchImage(const std::string& path, uint32_t seq);
    void Action(const std::string& json_body, uint32_t seq);

    bool online() const { return online_; }

private:
    struct Job {
        enum Kind { kNav, kScreen, kImage, kAction } kind;
        std::string arg;
        uint32_t seq;
    };

    static void Run(void* arg);
    void Enqueue(Job::Kind kind, const std::string& arg, uint32_t seq);
    void Handle(const Job& job);
    bool PollStatus();
    std::string Telemetry();
    // GET/POST against the Core; body on HTTP 200, status in *code.
    bool Request(const char* method, const std::string& path, const std::string& content, std::string* body,
                 int* code);
    std::string BaseUrl();

    R2d2Display* display_;
    QueueHandle_t queue_ = nullptr;
    TaskHandle_t task_ = nullptr;
    std::atomic<bool> online_{false};
    bool nav_loaded_ = false;
};

}  // namespace r2d2
