#include "esplog_display.h"

#include "esp_log.h"

#define TAG "EspLogDisplay"


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

void EspLogDisplay::SetIcon(const char* icon)
{
    ESP_LOGW(TAG, "SetIcon: %s", icon);
}

void EspLogDisplay::SetChatMessage(const char* role, const char* content)
{
    ESP_LOGW(TAG, "Role:%s", role);
    ESP_LOGW(TAG, "     %s", content);
}

