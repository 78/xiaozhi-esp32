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
        
        // 确保音频输出已启用
        auto& board = Board::GetInstance();
        auto codec = board.GetAudioCodec();
        if (!codec) {
            ESP_LOGE(TAG, "Audio codec not available");
            return;
        }
        
        codec->EnableOutput(true);
        codec->SetOutputVolume(10);
        
        auto music = Board::GetInstance().GetMusic();
        if (!music) {
            ESP_LOGE(TAG, "Music player not available");
            return;
        }
        
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
        // 通过AudioService间接控制音量
        auto& board = Board::GetInstance();
        auto codec = board.GetAudioCodec();
        codec->SetOutputVolume(codec->output_volume() + 10);
        ESP_LOGI(TAG, "Volume up requested");
    });
    
    volume_up_button_.OnLongPress([]() { 
        ESP_LOGI(TAG, "Volume up long pressed: switching to voice interaction mode");
        
        // 播放进入语音交互模式的提示音
        auto& app = Application::GetInstance();
        app.PlaySound("success"); // 播放成功提示音
        
        // 暂停音乐播放
        auto music = Board::GetInstance().GetMusic();
        if (music && music->IsPlaying()) {
            music->PauseSong();
            ESP_LOGI(TAG, "Music paused for voice interaction");
        }
        
        // 切换到语音交互模式
        app.GetAudioService().EnableWakeWordDetection(true);
        app.GetAudioService().EnableVoiceProcessing(true);
        ESP_LOGI(TAG, "Switched to voice interaction mode - waiting for user voice input");
    });
    
    // 音量下按钮回调
    volume_down_button_.OnClick([]() { 
        ESP_LOGI(TAG, "Volume down button clicked");
        auto& board = Board::GetInstance();
        auto codec = board.GetAudioCodec();
        codec->SetOutputVolume(codec->output_volume() - 10);
        ESP_LOGI(TAG, "Volume down requested");
    });
    
    volume_down_button_.OnLongPress([]() { 
        ESP_LOGI(TAG, "Volume down long pressed: stopping audio playback and voice interaction");
        
        // 播放停止提示音
        auto& app = Application::GetInstance();
        app.PlaySound("exclamation"); // 播放感叹号提示音
        
        // 停止音乐播放
        auto music = Board::GetInstance().GetMusic();
        if (music && music->IsPlaying()) {
            music->PauseSong();
            ESP_LOGI(TAG, "Music playback stopped");
        }
        
        // 停止语音交互
        app.GetAudioService().EnableWakeWordDetection(false);
        app.GetAudioService().EnableVoiceProcessing(false);
        ESP_LOGI(TAG, "Voice interaction stopped");
    });
}
