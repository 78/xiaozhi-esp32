#include "ui/display_manager.h"
#include "board.h"
#include "display.h"
#include "esp_log.h"
#include <array>

static const int BUTTON_COUNT = 6;

// Default empty hints
static std::array<std::string, BUTTON_COUNT> g_button_hints = {"", "", "", "", "", ""};
static int g_active_screen = 0;

static const char* TAG = "DisplayManager";

DisplayManager& DisplayManager::GetInstance() {
    static DisplayManager inst;
    return inst;
}

void DisplayManager::Init() {
    ESP_LOGI(TAG, "DisplayManager init");
}

void DisplayManager::ShowMainMenu() {
    auto& board = Board::GetInstance();
    auto display = board.GetDisplay();
    display->SetChatMessage("system", "English Teacher - Main Menu");
}

void DisplayManager::ShowMainMenu(const std::vector<std::string>& items, int selected_index) {
    auto& board = Board::GetInstance();
    auto display = board.GetDisplay();
    // Build a simple multi-line menu representation
    std::string buf;
    for (size_t i = 0; i < items.size(); ++i) {
        if ((int)i == selected_index) {
            buf += "> ";
        } else {
            buf += "  ";
        }
        buf += items[i];
        if (i + 1 < items.size()) buf += "\n";
    }
    display->SetChatMessage("system", buf.c_str());
}

void DisplayManager::ShowWordCard(const std::string& card_html) {
    auto& board = Board::GetInstance();
    auto display = board.GetDisplay();
    display->SetChatMessage("system", card_html.c_str());
}

void DisplayManager::UpdateConversationSide(bool is_user, const std::string& text_en, const std::string& text_cn) {
    auto& board = Board::GetInstance();
    auto display = board.GetDisplay();
    if (is_user) {
        display->SetChatMessage("user", text_en.c_str());
    } else {
        display->SetChatMessage("assistant", text_en.c_str());
    }
}

void DisplayManager::SetActiveScreen(int screen_id) {
    g_active_screen = screen_id;
}

void DisplayManager::SetButtonHints(const std::array<std::string, 6>& hints) {
    g_button_hints = hints;
}

void DisplayManager::DrawButtonHints() {
    auto& board = Board::GetInstance();
    auto display = board.GetDisplay();
    // Render hints on the status/chat area for now
    std::string line;
    for (int i = 0; i < BUTTON_COUNT; ++i) {
        if (!g_button_hints[i].empty()) {
            if (!line.empty()) line += " | ";
            line += "B" + std::to_string(i+1) + ":" + g_button_hints[i];
        }
    }
    if (line.empty()) {
        display->SetChatMessage("system", "");
    } else {
        display->SetChatMessage("system", line.c_str());
    }
}
