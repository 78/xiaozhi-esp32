#ifndef QUIZ_MODEL_H
#define QUIZ_MODEL_H

#include <string>
#include <vector>
#include <cstdint>

// Memory-safe constants
static constexpr size_t QUIZ_MAX_QUESTION_LEN = 256;
static constexpr size_t QUIZ_MAX_OPTION_LEN = 128;
static constexpr size_t QUIZ_MAX_OPTIONS = 4;
static constexpr size_t QUIZ_FILE_BUFFER_SIZE = 512;

/**
 * @brief Single quiz question with options and correct answer
 */
struct QuizQuestion {
    std::string question_text;           // The question content
    std::string options[QUIZ_MAX_OPTIONS]; // A, B, C, D options
    char correct_answer;                  // 'A', 'B', 'C', or 'D'
    int question_number;                  // 1-based index
    
    QuizQuestion() : correct_answer('\0'), question_number(0) {}
    
    void Clear() {
        question_text.clear();
        for (auto& opt : options) {
            opt.clear();
        }
        correct_answer = '\0';
        question_number = 0;
    }
    
    bool IsValid() const {
        return !question_text.empty() && 
               correct_answer >= 'A' && correct_answer <= 'D' &&
               question_number > 0;
    }
    
    // Get option text by choice letter
    const std::string& GetOption(char choice) const {
        if (choice >= 'A' && choice <= 'D') {
            return options[choice - 'A'];
        }
        static std::string empty;
        return empty;
    }
};

/**
 * @brief Metadata from quiz file header
 */
struct QuizMetadata {
    std::string title;      // QUIZ: header
    std::string subject;    // SUBJECT: header (optional)
    int total_questions;    // TOTAL: header
    
    QuizMetadata() : total_questions(0) {}
    
    void Clear() {
        title.clear();
        subject.clear();
        total_questions = 0;
    }
};

/**
 * @brief Tracks user's answer for a question
 */
struct UserAnswer {
    int question_number;
    char selected_answer;   // User's choice: A, B, C, D
    char correct_answer;    // Correct answer: A, B, C, D
    bool is_correct;
    
    UserAnswer() : question_number(0), selected_answer('\0'), correct_answer('\0'), is_correct(false) {}
    UserAnswer(int num, char ans, char correct, bool result) 
        : question_number(num), selected_answer(ans), correct_answer(correct), is_correct(result) {}
};

/**
 * @brief Quiz session state and results
 */
struct QuizSession {
    QuizMetadata metadata;
    std::vector<QuizQuestion> questions;
    std::vector<UserAnswer> user_answers;
    int current_question_index;   // 0-based
    bool is_active;
    
    QuizSession() : current_question_index(0), is_active(false) {}
    
    void Reset() {
        metadata.Clear();
        questions.clear();
        user_answers.clear();
        current_question_index = 0;
        is_active = false;
    }
    
    // Get current question (nullptr if none)
    const QuizQuestion* GetCurrentQuestion() const {
        if (current_question_index >= 0 && 
            current_question_index < static_cast<int>(questions.size())) {
            return &questions[current_question_index];
        }
        return nullptr;
    }
    
    bool HasNextQuestion() const {
        return current_question_index + 1 < static_cast<int>(questions.size());
    }
    
    bool IsComplete() const {
        return !questions.empty() && 
               user_answers.size() >= questions.size();
    }
    
    int GetCorrectCount() const {
        int count = 0;
        for (const auto& ans : user_answers) {
            if (ans.is_correct) count++;
        }
        return count;
    }
    
    int GetWrongCount() const {
        return static_cast<int>(user_answers.size()) - GetCorrectCount();
    }
};

/**
 * @brief Quiz state machine states
 */
enum class QuizState {
    IDLE,               // Not in quiz mode
    LOADING,            // Loading/parsing quiz file
    QUESTION_DISPLAY,   // Showing current question
    WAITING_ANSWER,     // Waiting for user input (touch/voice)
    CHECKING_ANSWER,    // Processing submitted answer
    SHOWING_RESULT,     // Showing final results
    ERROR               // Error state
};

/**
 * @brief Converts QuizState to string for logging
 */
inline const char* QuizStateToString(QuizState state) {
    switch (state) {
        case QuizState::IDLE: return "IDLE";
        case QuizState::LOADING: return "LOADING";
        case QuizState::QUESTION_DISPLAY: return "QUESTION_DISPLAY";
        case QuizState::WAITING_ANSWER: return "WAITING_ANSWER";
        case QuizState::CHECKING_ANSWER: return "CHECKING_ANSWER";
        case QuizState::SHOWING_RESULT: return "SHOWING_RESULT";
        case QuizState::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

#endif // QUIZ_MODEL_H
