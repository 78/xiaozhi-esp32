#pragma once
#include <string>
#include <vector>
#include <array>

class DisplayManager {
public:
    static DisplayManager& GetInstance();
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
    DisplayManager() = default;
};
