#include "ui/display_manager.h"
#include "board.h"
#include "display.h"
#include "esp_log.h"
#include <array>

// EPD helpers (DrawMixedString) - optional
#include "ui/epd_renderer.h"

static const int BUTTON_COUNT = 6;
// Simple in-memory conversation history (keep last N entries)
struct ConvEntry { bool is_user; std::string en; std::string cn; };
static std::vector<ConvEntry> g_conv_history;
static const int MAX_CONV_HISTORY = 12;

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
    if (EpdRenderer::Available()) {
        EpdRenderer::FillAndDraw(buf, 0, 10);
        EpdRenderer::Display(true);
    } else {
        display->SetChatMessage("system", buf.c_str());
    }
}

void DisplayManager::ShowWordCard(const std::string& card_html) {
    auto& board = Board::GetInstance();
    auto display = board.GetDisplay();
    if (EpdRenderer::Available()) {
        std::string plain = card_html;
        size_t p = 0;
        std::string out;
        while (p < plain.size()) {
            if (plain[p] == '<') {
                while (p < plain.size() && plain[p] != '>') ++p;
                if (p < plain.size()) ++p;
            } else {
                out += plain[p++];
            }
        }
        EpdRenderer::FillAndDraw(out, 0, 20);
        EpdRenderer::Display(true);
    } else {
        display->SetChatMessage("system", card_html.c_str());
    }
}

void DisplayManager::UpdateConversationSide(bool is_user, const std::string& text_en, const std::string& text_cn) {
    auto& board = Board::GetInstance();
    auto display = board.GetDisplay();
    // Append to history
    g_conv_history.push_back({is_user, text_en, text_cn});
    if ((int)g_conv_history.size() > MAX_CONV_HISTORY) g_conv_history.erase(g_conv_history.begin());

    if (!EpdRenderer::Available()) {
        // Fallback to simple chat area APIs
        if (is_user) display->SetChatMessage("user", text_en.c_str());
        else display->SetChatMessage("assistant", text_en.c_str());
        return;
    }

    // Render history on EPD: left column = user, right column = assistant
    EpdRenderer::Clear();
    int margin_x = 8;
    int col_width = 200; // for 4.2" landscape assume width ~400
    int y = 10;
    for (const auto &e : g_conv_history) {
        std::string line_en = e.is_user ? std::string("Me: ") + e.en : std::string("AI: ") + e.en;
        std::string line_cn = e.cn;
        if (e.is_user) {
            EpdRenderer::Draw(line_en, margin_x, y);
            y += 16;
            if (!line_cn.empty()) { EpdRenderer::Draw(line_cn, margin_x, y); y += 14; }
        } else {
            EpdRenderer::Draw(line_en, margin_x + col_width, y);
            y += 16;
            if (!line_cn.empty()) { EpdRenderer::Draw(line_cn, margin_x + col_width, y); y += 14; }
        }
        // small spacing between entries
        y += 6;
    }
    EpdRenderer::Display(true);
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
        if (!EpdRenderer::Available()) {
            display->SetChatMessage("system", "");
        }
    } else {
        if (EpdRenderer::Available()) {
            EpdRenderer::Draw(line, 0, display->height() - 40);
            EpdRenderer::Display(true);
        } else {
            display->SetChatMessage("system", line.c_str());
        }
    }
}
