#include "ui/menu_manager.h"
#include "ui/epd_manager.h"
#include "input/button_manager.h"
#include "services/word_practice.h"
#include "services/free_conversation.h"
#include "ui/screen.h"
#include <vector>
#include <string>

static std::vector<std::string> g_main_menu_items = {"Word Practice", "Free Conversation", "Scene Conversation", "Settings"};
static int g_main_menu_index = 0;

void MenuManager::Init() {
    // Main screen hints
    EpdManager::GetInstance().SetButtonHints({
        std::string("Up"), std::string("Down"), std::string("Select"),
        std::string("Back"), std::string("PTT"), std::string("Menu")
    });

    // Register main screen navigation
    ButtonManager::GetInstance().RegisterScreenCallback(ScreenId::MAIN, ButtonId::MENU_UP, [](){
        if (g_main_menu_index > 0) --g_main_menu_index;
        EpdManager::GetInstance().ShowMainMenu(g_main_menu_items, g_main_menu_index);
    });
    ButtonManager::GetInstance().RegisterScreenCallback(ScreenId::MAIN, ButtonId::MENU_DOWN, [](){
        if (g_main_menu_index < (int)g_main_menu_items.size()-1) ++g_main_menu_index;
        EpdManager::GetInstance().ShowMainMenu(g_main_menu_items, g_main_menu_index);
    });
    ButtonManager::GetInstance().RegisterScreenCallback(ScreenId::MAIN, ButtonId::SELECT, [](){
        switch (g_main_menu_index) {
            case 0:
                ButtonManager::GetInstance().SetActiveScreen(ScreenId::WORD_PRACTICE);
                EpdManager::GetInstance().SetActiveScreen((int)ScreenId::WORD_PRACTICE);
                EpdManager::GetInstance().ShowMainMenu(g_main_menu_items, g_main_menu_index);
                break;
            case 1:
                ButtonManager::GetInstance().SetActiveScreen(ScreenId::FREE_CONVERSATION);
                EpdManager::GetInstance().SetActiveScreen((int)ScreenId::FREE_CONVERSATION);
                EpdManager::GetInstance().ShowMainMenu(g_main_menu_items, g_main_menu_index);
                break;
            case 2:
                ButtonManager::GetInstance().SetActiveScreen(ScreenId::SCENE_CONVERSATION);
                EpdManager::GetInstance().SetActiveScreen((int)ScreenId::SCENE_CONVERSATION);
                EpdManager::GetInstance().ShowMainMenu(g_main_menu_items, g_main_menu_index);
                break;
            case 3:
                // settings placeholder
                break;
        }
    });
    ButtonManager::GetInstance().RegisterScreenCallback(ScreenId::MAIN, ButtonId::BACK, [](){
        // Maybe go to sleep/previous menu - keep as no-op for now
    });

    // Show initial menu
    EpdManager::GetInstance().ShowMainMenu(g_main_menu_items, g_main_menu_index);

    // Word practice screen hints (B1..B6)
    EpdManager::GetInstance().SetButtonHints({
        std::string("Prev"), std::string("Next"), std::string("Read"),
        std::string("ReadEx"), std::string("Quiz"), std::string("Home")
    });

    
    ButtonManager::GetInstance().RegisterScreenCallback(ScreenId::WORD_PRACTICE, ButtonId::BACK, [](){
        ButtonManager::GetInstance().SetActiveScreen(ScreenId::MAIN);
        EpdManager::GetInstance().SetActiveScreen((int)ScreenId::MAIN);
    });

    // Free conversation screen mapping
    ButtonManager::GetInstance().RegisterScreenCallback(ScreenId::FREE_CONVERSATION, ButtonId::MENU_UP, [](){
        // Start speaking
        FreeConversationService::GetInstance().Start(true);
    });
    ButtonManager::GetInstance().RegisterScreenCallback(ScreenId::FREE_CONVERSATION, ButtonId::MENU_DOWN, [](){
        // Stop speaking
        FreeConversationService::GetInstance().Stop();
    });
    ButtonManager::GetInstance().RegisterScreenCallback(ScreenId::FREE_CONVERSATION, ButtonId::SELECT, [](){
        // AI ask question (skeleton)
    });
    ButtonManager::GetInstance().RegisterScreenCallback(ScreenId::FREE_CONVERSATION, ButtonId::BACK, [](){
        ButtonManager::GetInstance().SetActiveScreen(ScreenId::MAIN);
        EpdManager::GetInstance().SetActiveScreen((int)ScreenId::MAIN);
    });
}
