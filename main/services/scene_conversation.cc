#include "services/scene_conversation.h"
#include "esp_log.h"

static const char* TAG = "SceneConversation";

SceneConversationService& SceneConversationService::GetInstance() {
    static SceneConversationService inst;
    return inst;
}

void SceneConversationService::Init() {
    ESP_LOGI(TAG, "SceneConversationService init");
}

void SceneConversationService::StartScene(const std::string& scene_id) {
    ESP_LOGI(TAG, "StartScene %s", scene_id.c_str());
}

void SceneConversationService::Answer(const std::string& user_audio_or_text) {
    ESP_LOGI(TAG, "SceneConversation Answer");
}
