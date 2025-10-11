#pragma once
#include <string>

class WordPracticeService {
public:
    static WordPracticeService& GetInstance();
    void Init();
    void StartLesson(const std::string& lesson_id);
    void Next();
    void Prev();
    void Exit();
    // Read current word (TTS), read example sentence, and ask quiz
    void ReadCurrent();
    void ReadExample();
    void AskQuiz();
private:
    WordPracticeService() = default;
};
