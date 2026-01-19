#ifndef QUIZ_MANAGER_H
#define QUIZ_MANAGER_H

#include "quiz_model.h"

#include <string>
#include <functional>
#include <mutex>
#include <memory>

// Forward declaration
class Board;

/**
 * @brief Callback types for quiz events
 */
using QuizQuestionCallback = std::function<void(const QuizQuestion& question)>;
using QuizAnswerCallback = std::function<void(const UserAnswer& answer, bool is_last)>;
using QuizResultCallback = std::function<void(const QuizSession& session)>;
using QuizErrorCallback = std::function<void(const std::string& error)>;

/**
 * @brief Quiz Manager - Handles quiz file parsing, state management, and scoring
 * 
 * Memory-safe design:
 * - Uses streaming file parser (fixed buffer, not full file load)
 * - RAII cleanup in destructor
 * - Fixed maximum question count
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
     * @brief Start a quiz from file on SD card
     * @param file_path Full path to quiz file (e.g., "/sdcard/quiz/test.txt")
     * @return true if quiz started successfully
     */
    bool StartQuiz(const std::string& file_path);
    
    /**
     * @brief Stop current quiz and cleanup
     */
    void StopQuiz();
    
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
     * @brief Get total number of questions
     */
    int GetTotalQuestions() const { return static_cast<int>(session_.questions.size()); }
    
    /**
     * @brief Submit an answer for current question
     * @param answer Character 'A', 'B', 'C', or 'D'
     * @return true if answer accepted
     */
    bool SubmitAnswer(char answer);
    
    /**
     * @brief Move to next question (called after answer submission)
     * @return true if moved to next question, false if quiz complete
     */
    bool NextQuestion();
    
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
    
    // ==================== File Discovery ====================
    
    /**
     * @brief Find all quiz files in /sdcard/quiz/ directory
     * @return Vector of file paths
     */
    static std::vector<std::string> FindQuizFiles();
    
private:
    // ==================== Internal Methods ====================
    
    /**
     * @brief Parse quiz file with streaming approach
     * @param file_path Path to quiz file
     * @return true if parsed successfully
     */
    bool ParseQuizFile(const std::string& file_path);
    
    /**
     * @brief Parse header lines (# QUIZ:, # SUBJECT:, # TOTAL:)
     */
    bool ParseHeader(const char* line);
    
    /**
     * @brief Parse question content
     */
    bool ParseQuestionLine(const char* line, QuizQuestion& current_question);
    
    /**
     * @brief Validate a single question structure
     */
    bool ValidateQuestion(const QuizQuestion& question);
    
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
     * @note PRECONDITION: mutex_ must be held by caller
     */
    void StopQuizInternal();
    
    // ==================== Member Variables ====================
    
    mutable std::mutex mutex_;
    QuizState state_;
    QuizSession session_;
    std::string current_file_path_;
    
    // Parser state
    bool in_question_;
    QuizQuestion pending_question_;
    
    // Callbacks
    QuizQuestionCallback on_question_ready_;
    QuizAnswerCallback on_answer_checked_;
    QuizResultCallback on_quiz_complete_;
    QuizErrorCallback on_error_;
};

#endif // QUIZ_MANAGER_H
