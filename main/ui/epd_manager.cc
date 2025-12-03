#include "ui/epd_manager.h"

#include "board.h"
#include "display.h"
#include "esp_log.h"

#include <algorithm>
#include <new>

// EPD helpers (DrawMixedString) - optional
#include "ui/epd_renderer.h"

namespace {
constexpr size_t kCommandQueueLength = 10;
constexpr TickType_t kQueueWaitTicks = pdMS_TO_TICKS(100);
constexpr uint32_t kTaskStackSize = 4096;
constexpr UBaseType_t kTaskPriority = 4;
}

struct EpdManager::Command {
    enum class Type {
        SHOW_MAIN_MENU_DEFAULT,
        SHOW_MAIN_MENU_DYNAMIC,
        SHOW_WORD_CARD,
        UPDATE_CONVERSATION,
        SET_ACTIVE_SCREEN,
        SET_BUTTON_HINTS,
        DRAW_BUTTON_HINTS,
    } type;

    std::vector<std::string> menu_items;
    int selected_index = 0;
    bool is_user = false;
    std::string text_en;
    std::string text_cn;
    std::string card_html;
    std::array<std::string, EpdManager::kButtonCount> hints = {"", "", "", "", "", ""};
    int screen_id = 0;
};

static const char* TAG = "EpdManager";

EpdManager& EpdManager::GetInstance() {
    static EpdManager inst;
    return inst;
}

void EpdManager::Init() {
    if (initialized_) {
        ESP_LOGI(TAG, "EpdManager already initialized");
        return;
    }

    ESP_LOGI(TAG, "EpdManager init");
    EpdRenderer::Init();
    EnsureTaskCreated();

    if (command_queue_ && task_handle_) {
        initialized_ = true;
    } else {
        ESP_LOGE(TAG, "Failed to launch EPD command task");
    }
}

void EpdManager::EnsureTaskCreated() {
    if (!command_queue_) {
        command_queue_ = xQueueCreate(kCommandQueueLength, sizeof(Command*));
        if (!command_queue_) {
            ESP_LOGE(TAG, "Failed to create EPD command queue");
            return;
        }
    }

    if (!task_handle_) {
        BaseType_t res = xTaskCreate(&EpdManager::TaskEntry, "epd_mgr", kTaskStackSize, this, kTaskPriority, &task_handle_);
        if (res != pdPASS) {
            ESP_LOGE(TAG, "Failed to create EPD task (%d)", res);
            task_handle_ = nullptr;
        }
    }
}

void EpdManager::TaskEntry(void* arg) {
    auto* self = static_cast<EpdManager*>(arg);
    if (self) {
        self->TaskLoop();
    }
    vTaskDelete(nullptr);
}

void EpdManager::TaskLoop() {
    while (true) {
        Command* raw_cmd = nullptr;
        if (xQueueReceive(command_queue_, &raw_cmd, portMAX_DELAY) == pdTRUE && raw_cmd) {
            ProcessCommand(*raw_cmd);
            delete raw_cmd;
        }
    }
}

void EpdManager::DispatchCommand(Command* cmd) {
    if (!cmd) {
        return;
    }
    EnsureTaskCreated();
    if (!command_queue_ || !task_handle_) {
        ProcessCommand(*cmd);
        delete cmd;
        return;
    }
    if (xQueueSend(command_queue_, &cmd, kQueueWaitTicks) != pdPASS) {
        ESP_LOGW(TAG, "EPD queue busy, discard latest command");
        delete cmd;
    }
}

void EpdManager::ProcessCommand(Command& cmd) {
    auto& board = Board::GetInstance();
    auto display = board.GetDisplay();

    switch (cmd.type) {
        case Command::Type::SHOW_MAIN_MENU_DEFAULT:
            display->SetChatMessage("system", "English Teacher - Main Menu");
            break;
        case Command::Type::SHOW_MAIN_MENU_DYNAMIC: {
            std::string buf;
            for (size_t i = 0; i < cmd.menu_items.size(); ++i) {
                buf += ((int)i == cmd.selected_index) ? "> " : "  ";
                buf += cmd.menu_items[i];
                if (i + 1 < cmd.menu_items.size()) buf += "\n";
            }
            if (EpdRenderer::Available()) {
                EpdRenderer::DrawText(buf, 0, 10);
                EpdRenderer::Display(true);
            } else {
                display->SetChatMessage("system", buf.c_str());
            }
            break;
        }
        case Command::Type::SHOW_WORD_CARD: {
            if (EpdRenderer::Available()) {
                std::string plain = cmd.card_html;
                std::string out;
                size_t p = 0;
                while (p < plain.size()) {
                    if (plain[p] == '<') {
                        while (p < plain.size() && plain[p] != '>') ++p;
                        if (p < plain.size()) ++p;
                    } else {
                        out += plain[p++];
                    }
                }
                EpdRenderer::DrawText(out, 0, 20);
                EpdRenderer::Display(true);
            } else {
                display->SetChatMessage("system", cmd.card_html.c_str());
            }
            break;
        }
        case Command::Type::UPDATE_CONVERSATION: {
            ESP_LOGW(TAG, "Conversation %s: en='%s' cn='%s'",
                cmd.is_user ? "user" : "assistant",
                cmd.text_en.c_str(),
                cmd.text_cn.c_str());

            conversation_history_.push_back({cmd.is_user, cmd.text_en, cmd.text_cn});
            if ((int)conversation_history_.size() > kMaxConversationHistory) {
                conversation_history_.erase(conversation_history_.begin());
            }

            constexpr int refresh_width = 400;
            constexpr int refresh_height = 300;
            constexpr int margin_x = 8;
            constexpr int margin_y = 10;
            constexpr int line_gap = 6;
            constexpr int text_height_en = 16;
            constexpr int text_height_cn = 14;
            const int column_offset = refresh_width / 2;

            EpdRenderer::Clear();

            int y_user = margin_y;
            int y_ai = margin_y;
            for (const auto& entry : conversation_history_) {
                const bool is_user = entry.is_user;
                const int base_x = is_user ? margin_x : column_offset + margin_x;
                int& cursor_y = is_user ? y_user : y_ai;

                std::string line_en = (is_user ? "Me: " : "AI: ") + entry.en;
                if (!line_en.empty()) {
                    EpdRenderer::DrawText(line_en, base_x, cursor_y);
                    cursor_y += text_height_en;
                }

                if (!entry.cn.empty()) {
                    EpdRenderer::DrawText(entry.cn, base_x, cursor_y);
                    cursor_y += text_height_cn;
                }

                cursor_y += line_gap;
            }

            int window_height = std::max(y_user, y_ai) + margin_y;
            window_height = std::min(refresh_height, window_height);
            EpdRenderer::DisplayWindow(0, 0, refresh_width, window_height, true);
            break;
        }
        case Command::Type::SET_ACTIVE_SCREEN:
            active_screen_ = cmd.screen_id;
            break;
        case Command::Type::SET_BUTTON_HINTS:
            button_hints_ = cmd.hints;
            break;
        case Command::Type::DRAW_BUTTON_HINTS: {
            std::string line;
            for (int i = 0; i < kButtonCount; ++i) {
                if (!button_hints_[i].empty()) {
                    if (!line.empty()) line += " | ";
                    line += "B" + std::to_string(i + 1) + ":" + button_hints_[i];
                }
            }

            if (line.empty()) {
                if (!EpdRenderer::Available()) {
                    display->SetChatMessage("system", "");
                }
            } else {
                if (EpdRenderer::Available()) {
                    EpdRenderer::DrawText(line, 0, display->height() - 40);
                    EpdRenderer::Display(true);
                } else {
                    display->SetChatMessage("system", line.c_str());
                }
            }
            break;
        }
    }
}

void EpdManager::ShowMainMenu() {
    Command* cmd = new (std::nothrow) Command();
    if (!cmd) {
        ESP_LOGE(TAG, "Failed to allocate command for ShowMainMenu");
        return;
    }
    cmd->type = Command::Type::SHOW_MAIN_MENU_DEFAULT;
    DispatchCommand(cmd);
}

void EpdManager::ShowMainMenu(const std::vector<std::string>& items, int selected_index) {
    Command* cmd = new (std::nothrow) Command();
    if (!cmd) {
        ESP_LOGE(TAG, "Failed to allocate command for ShowMainMenu(items)");
        return;
    }
    cmd->type = Command::Type::SHOW_MAIN_MENU_DYNAMIC;
    cmd->menu_items = items;
    cmd->selected_index = selected_index;
    DispatchCommand(cmd);
}

void EpdManager::ShowWordCard(const std::string& card_html) {
    Command* cmd = new (std::nothrow) Command();
    if (!cmd) {
        ESP_LOGE(TAG, "Failed to allocate command for ShowWordCard");
        return;
    }
    cmd->type = Command::Type::SHOW_WORD_CARD;
    cmd->card_html = card_html;
    DispatchCommand(cmd);
}

void EpdManager::UpdateConversationSide(bool is_user, const std::string& text_en, const std::string& text_cn) {
    Command* cmd = new (std::nothrow) Command();
    if (!cmd) {
        ESP_LOGE(TAG, "Failed to allocate command for UpdateConversationSide");
        return;
    }
    cmd->type = Command::Type::UPDATE_CONVERSATION;
    cmd->is_user = is_user;
    cmd->text_en = text_en;
    cmd->text_cn = text_cn;
    DispatchCommand(cmd);
}

void EpdManager::SetActiveScreen(int screen_id) {
    Command* cmd = new (std::nothrow) Command();
    if (!cmd) {
        ESP_LOGE(TAG, "Failed to allocate command for SetActiveScreen");
        return;
    }
    cmd->type = Command::Type::SET_ACTIVE_SCREEN;
    cmd->screen_id = screen_id;
    DispatchCommand(cmd);
}

void EpdManager::SetButtonHints(const std::array<std::string, 6>& hints) {
    Command* cmd = new (std::nothrow) Command();
    if (!cmd) {
        ESP_LOGE(TAG, "Failed to allocate command for SetButtonHints");
        return;
    }
    cmd->type = Command::Type::SET_BUTTON_HINTS;
    cmd->hints = hints;
    DispatchCommand(cmd);
}

void EpdManager::DrawButtonHints() {
    Command* cmd = new (std::nothrow) Command();
    if (!cmd) {
        ESP_LOGE(TAG, "Failed to allocate command for DrawButtonHints");
        return;
    }
    cmd->type = Command::Type::DRAW_BUTTON_HINTS;
    DispatchCommand(cmd);
}
