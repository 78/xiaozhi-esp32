#ifndef TELEGRAM_BOT_H
#define TELEGRAM_BOT_H

#include <string>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

class Esp32Camera;

class TelegramBot {
public:
    enum class ReplyMode {
        BOTH = 0,
        CHAT = 1,
        VOICE = 2,
    };

    struct Message {
        std::string text;
    };

    TelegramBot(Esp32Camera *camera);
    ~TelegramBot();

    void Start();
    void SetCamera(Esp32Camera *camera) { camera_ = camera; }
    void PushConversation(const char *role, const std::string &text);
    void PushSystem(const std::string &text);
    void PushPhoto(const uint8_t *jpeg_data, size_t jpeg_len);

    ReplyMode GetReplyMode() const { return reply_mode_; }

private:
    struct PhotoMessage {
        uint8_t *data;
        size_t len;
    };

    Esp32Camera *camera_ = nullptr;
    std::string token_;
    std::string chat_id_;
    TaskHandle_t task_handle_ = nullptr;
    QueueHandle_t out_queue_ = nullptr;
    QueueHandle_t photo_queue_ = nullptr;
    bool running_ = false;
    bool reported_polling_ = false;
    int poll_count_ = 0;
    uint64_t last_mic_check_us_ = 0;
    ReplyMode reply_mode_ = ReplyMode::BOTH;

    static void Task(void *arg);
    void Run();
    void Enqueue(const std::string &text);
    void LoadReplyMode();
    void SaveReplyMode(ReplyMode mode);
    std::string GetUpdates(int64_t offset, int timeout_sec);
    bool SendMessage(const std::string &target_chat_id, const std::string &text);
    bool SendPhoto(const std::string &target_chat_id, const uint8_t *jpeg_data, size_t jpeg_len, const std::string &filename = "capture.jpg");
    bool SendAudioWav(const std::string &target_chat_id, const uint8_t *wav_data, size_t wav_len);
    void RecordAndSend(const std::string &target_chat_id);
    void CaptureAndSendPhotos(const std::string &chat_id);
    void CheckMicLevels();
    void HandleCommand(const std::string &command, const std::string &sender_chat_id, const std::string &from_user);
};

#endif // TELEGRAM_BOT_H