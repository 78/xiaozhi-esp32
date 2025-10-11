#include "services/word_practice.h"
#include "esp_log.h"
#include "ui/display_manager.h"
#include "audio/audio_manager.h"

#include <vector>

static const char* TAG = "WordPractice";

struct WordCard {
    std::string word_en;
    std::string word_cn;
    std::string example_en;
    std::string example_cn;
};

static std::vector<WordCard> g_words = {
    {"apple", "苹果", "I like to eat an apple.", "我喜欢吃苹果。"},
    {"orange", "橙子", "An orange is rich in vitamin C.", "橙子富含维生素C。"},
    {"book", "书", "This book is very interesting.", "这本书很有趣。"}
};

static size_t g_current_index = 0;

WordPracticeService& WordPracticeService::GetInstance() {
    static WordPracticeService inst;
    return inst;
}

void WordPracticeService::Init() {
    ESP_LOGI(TAG, "WordPracticeService init");
}

void WordPracticeService::StartLesson(const std::string& lesson_id) {
    ESP_LOGI(TAG, "StartLesson %s", lesson_id.c_str());
    (void)lesson_id;
    g_current_index = 0;
    std::string html = "<h1>" + g_words[g_current_index].word_en + "</h1>";
    html += "<p>" + g_words[g_current_index].word_cn + "</p>";
    DisplayManager::GetInstance().ShowWordCard(html);
}

void WordPracticeService::Next() {
    ESP_LOGI(TAG, "WordPractice Next");
    if (g_current_index + 1 < g_words.size()) ++g_current_index;
    std::string html = "<h1>" + g_words[g_current_index].word_en + "</h1>";
    html += "<p>" + g_words[g_current_index].word_cn + "</p>";
    DisplayManager::GetInstance().ShowWordCard(html);
}

void WordPracticeService::Prev() {
    ESP_LOGI(TAG, "WordPractice Prev");
    if (g_current_index > 0) --g_current_index;
    std::string html = "<h1>" + g_words[g_current_index].word_en + "</h1>";
    html += "<p>" + g_words[g_current_index].word_cn + "</p>";
    DisplayManager::GetInstance().ShowWordCard(html);
}

void WordPracticeService::Exit() {
    ESP_LOGI(TAG, "WordPractice Exit");
}

void WordPracticeService::ReadCurrent() {
    ESP_LOGI(TAG, "ReadCurrent");
    auto &w = g_words[g_current_index];
    std::string text = w.word_en + ": " + w.word_cn;
    DisplayManager::GetInstance().UpdateConversationSide(false, text, "");
    AudioManager::GetInstance().PlayPcm(nullptr, 0);
}

void WordPracticeService::ReadExample() {
    ESP_LOGI(TAG, "ReadExample");
    auto &w = g_words[g_current_index];
    std::string text = w.example_en + " - " + w.example_cn;
    DisplayManager::GetInstance().UpdateConversationSide(false, text, "");
    AudioManager::GetInstance().PlayPcm(nullptr, 0);
}

void WordPracticeService::AskQuiz() {
    ESP_LOGI(TAG, "AskQuiz");
    auto &w = g_words[g_current_index];
    std::string q = "What's the Chinese for '" + w.word_en + "'?";
    DisplayManager::GetInstance().UpdateConversationSide(false, q, "");
}

