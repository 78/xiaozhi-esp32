#include "services/word_practice.h"
#include "esp_log.h"

static const char* TAG = "WordPractice";

WordPracticeService& WordPracticeService::GetInstance() {
    static WordPracticeService inst;
    return inst;
}

void WordPracticeService::Init() {
    ESP_LOGI(TAG, "WordPracticeService init");
}

void WordPracticeService::StartLesson(const std::string& lesson_id) {
    ESP_LOGI(TAG, "StartLesson %s", lesson_id.c_str());
}

void WordPracticeService::Next() {
    ESP_LOGI(TAG, "WordPractice Next");
}

void WordPracticeService::Prev() {
    ESP_LOGI(TAG, "WordPractice Prev");
}

void WordPracticeService::Exit() {
    ESP_LOGI(TAG, "WordPractice Exit");
}
