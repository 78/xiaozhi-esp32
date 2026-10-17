// Polls the R2D2 Core for the idle screen's data (GET /satellite/status).
//
// The conversation WebSocket only exists while someone talks, so the clock
// sync and the weather come over plain HTTP every few minutes, with the same
// Device-Id and token the WebSocket uses. The URL is the OTA URL's sibling:
// .../satellite/ota/ → .../satellite/status.
#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace r2d2 {

class R2d2Display;

class StatusPoller {
public:
    explicit StatusPoller(R2d2Display* display);
    void Start();

private:
    static void Run(void* arg);
    bool PollOnce();

    R2d2Display* display_;
    TaskHandle_t task_ = nullptr;
};

}  // namespace r2d2
