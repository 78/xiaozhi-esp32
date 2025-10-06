#pragma once
#include <string>

class SceneConversationService {
public:
    static SceneConversationService& GetInstance();
    void Init();
    void StartScene(const std::string& scene_id);
    void Answer(const std::string& user_audio_or_text);
private:
    SceneConversationService() = default;
};
