#pragma once
#include <array>
#include <string>
#include <vector>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

class EpdManager {
public:
    static EpdManager& GetInstance();
    static constexpr int kButtonCount = 6;
    static constexpr int kMaxConversationHistory = 12;
    void Init();
    void ShowMainMenu();
    void ShowMainMenu(const std::vector<std::string>& items, int selected_index);
    void ShowWordCard(const std::string& card_html);
    void UpdateConversationSide(bool is_user, const std::string& text_en, const std::string& text_cn);
    void SetActiveScreen(int screen_id);
    // Set button hints (6 entries) to be shown on screen
    void SetButtonHints(const std::array<std::string, 6>& hints);
    void DrawButtonHints();

private:
    struct Command;
    struct ConversationEntry {
        bool is_user = false;
        std::string en;
        std::string cn;
    };

    void EnsureTaskCreated();
    static void TaskEntry(void* arg);
    void TaskLoop();
    void DispatchCommand(Command* cmd);
    void ProcessCommand(Command& cmd);

    EpdManager() = default;

    QueueHandle_t command_queue_ = nullptr;
    TaskHandle_t task_handle_ = nullptr;
    bool initialized_ = false;

    int active_screen_ = 0;
    std::array<std::string, kButtonCount> button_hints_ = {"", "", "", "", "", ""};
    std::vector<ConversationEntry> conversation_history_;
};
