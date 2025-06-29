#include "esplog_display.h"

#include "esp_log.h"

#define TAG "EspLog显示"


EspLogDisplay::EspLogDisplay()
{}

EspLogDisplay::~EspLogDisplay()
{}

void EspLogDisplay::SetStatus(const char* status)
{
    ESP_LOGI(TAG, "设置状态: %s", status);
}

void EspLogDisplay::ShowNotification(const char* notification, int duration_ms)
{
    ESP_LOGI(TAG, "通知: %s", notification);
}
void EspLogDisplay::ShowNotification(const std::string &notification, int duration_ms) 
{ 
    ShowNotification(notification.c_str(), duration_ms); 
}


void EspLogDisplay::SetEmotion(const char* emotion)
{
    ESP_LOGI(TAG, "设置表情: %s", emotion);
}

void EspLogDisplay::SetIcon(const char* icon)
{
    ESP_LOGI(TAG, "设置图标: %s", icon);
}

void EspLogDisplay::SetChatMessage(const char* role, const char* content)
{
    ESP_LOGI(TAG, "角色:%s , 消息：%s", role, content);
}

