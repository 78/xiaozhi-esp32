#include "esplog_display.h"

#include "esp_log.h"

#define TAG "Display2Log"


EspLogDisplay::EspLogDisplay()
{}

EspLogDisplay::~EspLogDisplay()
{}

void EspLogDisplay::SetStatus(const char* status)
{
    ESP_LOGW(TAG, "SetStatus: %s", status);
}

void EspLogDisplay::ShowNotification(const char* notification, int duration_ms)
{
    ESP_LOGW(TAG, "ShowNotification: %s", notification);
}
void EspLogDisplay::ShowNotification(const std::string &notification, int duration_ms) 
{ 
    ShowNotification(notification.c_str(), duration_ms); 
}


void EspLogDisplay::SetEmotion(const char* emotion)
{
    ESP_LOGW(TAG, "SetEmotion: %s", emotion);
}

void EspLogDisplay::SetChatMessage(const char* role, const char* content)
{
    ESP_LOGW(TAG, "Role:%s", role);
    ESP_LOGW(TAG, "     %s", content);
}

// 音乐播放相关（用日志模拟UI行为）
// 显示当前播放的歌曲信息
void EspLogDisplay::SetMusicInfo(const char* info)
{
    ESP_LOGW(TAG, "MusicInfo: %s", info ? info : "");
}

// 启动频谱显示（此处仅打印日志）
void EspLogDisplay::start()
{
    ESP_LOGW(TAG, "Spectrum start");
}

// 停止频谱显示（此处仅打印日志）
void EspLogDisplay::stopFft()
{
    ESP_LOGW(TAG, "Spectrum stop");
}

