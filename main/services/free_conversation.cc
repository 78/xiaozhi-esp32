#include "services/free_conversation.h"
#include "esp_log.h"

static const char* TAG = "FreeConversation";

FreeConversationService& FreeConversationService::GetInstance() {
    static FreeConversationService inst;
    return inst;
}

void FreeConversationService::Init() {
    ESP_LOGI(TAG, "FreeConversationService init");
}

void FreeConversationService::Start(bool push_to_talk) {
    ESP_LOGI(TAG, "FreeConversation Start push_to_talk=%d", push_to_talk);
}

void FreeConversationService::Stop() {
    ESP_LOGI(TAG, "FreeConversation Stop");
}
