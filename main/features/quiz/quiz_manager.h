#ifndef QUIZ_MANAGER_H
#define QUIZ_MANAGER_H

#include "quiz_model.h"

#include <string>
#include <functional>
#include <mutex>
#include <memory>
#include <esp_http_client.h>
#include <cJSON.h>

// Server Configuration
// TODO: User needs to update this with their deployed URL
#define QUIZ_SERVER_URL "https://quiz-server-xiaozhi.onrender.com" 

/**
 * @brief Callback types for quiz events
 */
using QuizQuestionCallback = std::function<void(const QuizQuestion& question)>;
using QuizAnswerCallback = std::function<void(const UserAnswer& answer, bool is_last)>;
using QuizResultCallback = std::function<void(const QuizSession& session)>;
using QuizErrorCallback = std::function<void(const std::string& error)>;

/**
 * @brief Quiz Manager - Server-Client Architecture
 * 
 * Handles communication with the Quiz Server via HTTP Keep-Alive.
 * Offloads logic to the server to reduce ESP32 processing load.
 */
class QuizManager {
public:
    QuizManager();
    ~QuizManager();
    
    // Prevent copying
    QuizManager(const QuizManager&) = delete;
    QuizManager& operator=(const QuizManager&) = delete;
    
    // ==================== Lifecycle ====================
    
    /**
     * @brief Start a new quiz session by connecting to the server
     * @return true if request initiated successfully
     */
    bool StartQuiz();
    
    /**
     * @brief Stop current quiz and cleanup connection
     */
    /**
     * @brief Stop current quiz and cleanup connection
     */
    bool StopQuiz();
    
    /**
     * @brief Check if quiz is currently active
     */
    bool IsActive() const { return session_.is_active; }
    
    // ==================== Quiz Flow ====================
    
    /**
     * @brief Get current quiz state
     */
    QuizState GetState() const { return state_; }
    
    /**
     * @brief Get current question being displayed
     */
    const QuizQuestion* GetCurrentQuestion() const;
    
    /**
     * @brief Get current question index (0-based)
     */
    int GetCurrentQuestionIndex() const { return session_.current_question_index; }
    
    /**
     * @brief Submit an answer to the server
     * @param answer Character 'A', 'B', 'C', or 'D'
     * @return true if request initiated successfully
     */
    bool SubmitAnswer(char answer);

    /**
     * @brief Move to next question (called after answer submission logic)
     * @return true if moved to next question, false if quiz complete/waiting
     */
    bool NextQuestion();
    
    /**
     * @brief Get total number of questions
     */
    int GetTotalQuestions() const { return session_.metadata.total_questions; }
    
    /**
     * @brief Get the quiz session (for results)
     */
    const QuizSession& GetSession() const { return session_; }
    
    // ==================== Results ====================
    
    /**
     * @brief Generate result summary string
     * @return Formatted result string for TTS
     */
    std::string GenerateResultSummary() const;
    
    /**
     * @brief Get list of wrong answers with correct answers
     * @return Vector of wrong question info
     */
    struct WrongAnswerInfo {
        int question_number;
        char user_answer;
        char correct_answer;
        std::string correct_option_text;
    };
    std::vector<WrongAnswerInfo> GetWrongAnswers() const;
    
    // ==================== Callbacks ====================
    
    void SetOnQuestionReady(QuizQuestionCallback callback) { on_question_ready_ = callback; }
    void SetOnAnswerChecked(QuizAnswerCallback callback) { on_answer_checked_ = callback; }
    void SetOnQuizComplete(QuizResultCallback callback) { on_quiz_complete_ = callback; }
    void SetOnError(QuizErrorCallback callback) { on_error_ = callback; }
    
private:
    // ==================== Internal Methods ====================
    
    /**
     * @brief Initialize HTTP client with Keep-Alive
     */
    bool InitHttpClient();
    
    /**
     * @brief Get device unique ID (last 3 bytes of MAC)
     */
    std::string GetDeviceId() const;

    /**
     * @brief Send HTTP POST request
     */
    bool SendRequest(const char* endpoint, cJSON* payload, cJSON** response_json);
    
    /**
     * @brief Parse question from JSON response
     */
    bool ParseQuestionJson(cJSON* q_obj, int display_index, QuizQuestion& out_question);
    
    /**
     * @brief Set state and log transition
     */
    void SetState(QuizState new_state);
    
    /**
     * @brief Report error and set error state
     */
    void ReportError(const std::string& error);
    
    /**
     * @brief Internal stop quiz - called when mutex is already held
     */
    void StopQuizInternal();
    
    // ==================== Member Variables ====================
    
    mutable std::mutex mutex_;
    QuizState state_;
    QuizSession session_;
    
    // Server config
    std::string session_id_;
    esp_http_client_handle_t client_handle_;
    
    // Callbacks
    QuizQuestionCallback on_question_ready_;
    QuizAnswerCallback on_answer_checked_;
    QuizResultCallback on_quiz_complete_;
    QuizErrorCallback on_error_;
};

#endif // QUIZ_MANAGER_H
