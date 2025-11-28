#pragma once
#include <string>

class FreeConversationService {
public:
    static FreeConversationService& GetInstance();
    void Init();
    void Start(bool push_to_talk);
    void Stop();
private:
    FreeConversationService() = default;
};
