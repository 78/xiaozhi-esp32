#include "button_manager.h"
#include "application.h"
#include <esp_log.h>

#define TAG "ButtonManager"

ButtonManager& ButtonManager::GetInstance() {
    static ButtonManager instance;
    return instance;
}

ButtonManager::ButtonManager()
    : boot_button_(BOOT_BUTTON_GPIO),
      volume_up_button_(VOLUME_UP_BUTTON_GPIO),
      volume_down_button_(VOLUME_DOWN_BUTTON_GPIO) {
}

bool ButtonManager::Initialize() {
    if (initialized_) {
        ESP_LOGW(TAG, "ButtonManager already initialized");
        return true;
    }
    
    ESP_LOGI(TAG, "Initializing ButtonManager...");
    
    // 设置按钮回调
    SetupButtonCallbacks();
    
    initialized_ = true;
    ESP_LOGI(TAG, "ButtonManager initialized successfully");
    return true;
}

void ButtonManager::SetupButtonCallbacks() {
    ESP_LOGI(TAG, "Setting up button callbacks...");
    
    // BOOT按钮回调
    boot_button_.OnClick([]() { 
        ESP_LOGI(TAG, "Boot button clicked"); 
    });
    
    boot_button_.OnLongPress([]() { 
        ESP_LOGI(TAG, "BOOT long pressed: play boot tone");
        // 使用Application的PlaySound方法播放开机音效
        //auto& app = Application::GetInstance();
        //app.PlaySound("activation");
        //ESP_LOGI(TAG, "Boot tone played");
        auto& board = Board::GetInstance();
        auto codec = board.GetAudioCodec();
        codec->EnableOutput(true);
        codec->SetOutputVolume(10);
        
        auto music = Board::GetInstance().GetMusic();
        auto song_name = "稻香";
        auto artist_name = "";
        if (!music->Download(song_name, artist_name)) {
            ESP_LOGI(TAG, "获取音乐资源失败");
            return;
        }
        
        auto download_result = music->GetDownloadResult();
        ESP_LOGI(TAG, "Music details result: %s", download_result.c_str());
    });
    
    // 音量上按钮回调
    volume_up_button_.OnClick([]() { 
        ESP_LOGI(TAG, "Volume up button clicked");
        auto& app = Application::GetInstance();
        auto& audio_service = app.GetAudioService();
        // 通过AudioService间接控制音量
        ESP_LOGI(TAG, "Volume up requested");
    });
    
    volume_up_button_.OnLongPress([]() { 
        ESP_LOGI(TAG, "Volume up long pressed: set to maximum");
        auto& app = Application::GetInstance();
        auto& audio_service = app.GetAudioService();
        ESP_LOGI(TAG, "Volume set to maximum requested");
    });
    
    // 音量下按钮回调
    volume_down_button_.OnClick([]() { 
        ESP_LOGI(TAG, "Volume down button clicked");
        auto& app = Application::GetInstance();
        auto& audio_service = app.GetAudioService();
        ESP_LOGI(TAG, "Volume down requested");
    });
    
    volume_down_button_.OnLongPress([]() { 
        ESP_LOGI(TAG, "Volume down long pressed: mute");
        auto& app = Application::GetInstance();
        auto& audio_service = app.GetAudioService();
        ESP_LOGI(TAG, "Volume mute requested");
    });
}
