#ifndef _T_DECK_PRO_A7682E_MODEM_H_
#define _T_DECK_PRO_A7682E_MODEM_H_

#include "a7682e_tts.h"

#include <stdint.h>

#include <atomic>
#include <mutex>
#include <string>
#include <string_view>

#include <driver/gpio.h>
#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/task.h>

class A7682eModem {
public:
    A7682eModem();
    ~A7682eModem();

    bool Begin();
    bool IsReady() const { return modem_ready_.load(); }
    bool SpeakText(std::string_view utf8_text);
    bool IsSpeechBusy() const;
    void StopSpeech();
    bool SetOutputGain(int gain);

private:
    enum class CommandType : uint8_t {
        Speak,
        Stop,
    };

    struct Command {
        CommandType type = CommandType::Stop;
        int volume = 0;
        char text[a7682e::kTtsMaxTextBytes + 1] = {};
    };

    static void WorkerEntry(void* arg);
    void WorkerTask();
    bool ConfigureHardware();
    bool InitializeModem();
    bool EnsureReady();
    bool SendCommand(std::string_view command, std::string& response, int timeout_ms,
                     bool flush_input = true);
    bool EncodeAndSendTts(std::string_view utf8_text);
    bool WaitForTtsCompletion(std::string_view utf8_text, const std::string& initial_response);
    bool Enqueue(const Command& command, bool front = false);
    bool EnqueueUnlocked(const Command& command, bool front = false);
    void MaybeLogQueueOverflow();
    void CompleteSpeechCommand();

    std::mutex command_mutex_;
    QueueHandle_t command_queue_ = nullptr;
    TaskHandle_t worker_task_ = nullptr;
    EventGroupHandle_t init_event_ = nullptr;
    std::atomic<bool> modem_ready_{false};
    std::atomic<bool> speech_active_{false};
    std::atomic<uint32_t> outstanding_speech_count_{0};
    std::atomic<int> volume_{70};
    std::atomic<int> pending_gain_{-1};
    std::atomic<bool> stop_requested_{false};
    int64_t last_queue_overflow_log_us_ = 0;
    bool started_ = false;
    bool uart_ready_ = false;
    bool uart_driver_owned_ = false;
};

#endif  // _T_DECK_PRO_A7682E_MODEM_H_
