#include "quiz_manager.h"

#include <cstdio>
#include <cstring>
#include <cerrno>
#include <dirent.h>
#include <sys/stat.h>
#include <esp_log.h>

#define TAG "QuizManager"

// Maximum questions to prevent memory issues
#ifndef CONFIG_QUIZ_MAX_QUESTIONS
#define CONFIG_QUIZ_MAX_QUESTIONS 50
#endif

QuizManager::QuizManager()
    : state_(QuizState::IDLE)
    , in_question_(false)
{
    ESP_LOGI(TAG, "QuizManager created");
}

QuizManager::~QuizManager()
{
    StopQuiz();
    ESP_LOGI(TAG, "QuizManager destroyed");
}

void QuizManager::SetState(QuizState new_state)
{
    if (state_ != new_state) {
        ESP_LOGI(TAG, "State: %s -> %s", 
                 QuizStateToString(state_), 
                 QuizStateToString(new_state));
        state_ = new_state;
    }
}

void QuizManager::ReportError(const std::string& error)
{
    ESP_LOGE(TAG, "Error: %s", error.c_str());
    SetState(QuizState::ERROR);
    if (on_error_) {
        on_error_(error);
    }
}

bool QuizManager::StartQuiz(const std::string& file_path)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (session_.is_active) {
        ESP_LOGW(TAG, "Quiz already active, stopping first");
        StopQuizInternal();  // Use internal version - mutex already held
    }
    
    ESP_LOGI(TAG, "Starting quiz from: %s", file_path.c_str());
    SetState(QuizState::LOADING);
    
    session_.Reset();
    current_file_path_ = file_path;
    
    // Reserve space for questions to avoid reallocations
    session_.questions.reserve(CONFIG_QUIZ_MAX_QUESTIONS);
    session_.user_answers.reserve(CONFIG_QUIZ_MAX_QUESTIONS);
    
    if (!ParseQuizFile(file_path)) {
        ReportError("Failed to parse quiz file: " + file_path);
        return false;
    }
    
    if (session_.questions.empty()) {
        ReportError("No valid questions found in file");
        return false;
    }
    
    session_.is_active = true;
    session_.current_question_index = 0;
    SetState(QuizState::QUESTION_DISPLAY);
    
    ESP_LOGI(TAG, "Quiz started with %d questions", 
             static_cast<int>(session_.questions.size()));
    
    // Notify first question ready
    if (on_question_ready_) {
        on_question_ready_(session_.questions[0]);
    }
    
    return true;
}

void QuizManager::StopQuiz()
{
    std::lock_guard<std::mutex> lock(mutex_);
    StopQuizInternal();
}

void QuizManager::StopQuizInternal()
{
    // PRECONDITION: mutex_ must be held by caller
    
    if (!session_.is_active) {
        return;
    }
    
    ESP_LOGI(TAG, "Stopping quiz");
    
    session_.Reset();
    current_file_path_.clear();
    in_question_ = false;
    pending_question_.Clear();
    
    SetState(QuizState::IDLE);
}

const QuizQuestion* QuizManager::GetCurrentQuestion() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return session_.GetCurrentQuestion();
}

bool QuizManager::SubmitAnswer(char answer)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!session_.is_active) {
        ESP_LOGW(TAG, "Cannot submit answer: quiz not active");
        return false;
    }
    
    if (answer < 'A' || answer > 'D') {
        // Try lowercase
        if (answer >= 'a' && answer <= 'd') {
            answer = answer - 'a' + 'A';
        } else {
            ESP_LOGW(TAG, "Invalid answer: %c", answer);
            return false;
        }
    }
    
    const QuizQuestion* current = session_.GetCurrentQuestion();
    if (!current) {
        ESP_LOGW(TAG, "No current question");
        return false;
    }
    
    SetState(QuizState::CHECKING_ANSWER);
    
    bool is_correct = (answer == current->correct_answer);
    UserAnswer user_answer(current->question_number, answer, current->correct_answer, is_correct);
    session_.user_answers.push_back(user_answer);
    
    ESP_LOGI(TAG, "Question %d: User=%c, Correct=%c, %s",
             current->question_number, answer, current->correct_answer,
             is_correct ? "CORRECT" : "WRONG");
    
    bool is_last = !session_.HasNextQuestion();
    
    // Notify answer checked
    if (on_answer_checked_) {
        on_answer_checked_(user_answer, is_last);
    }
    
    return true;
}

bool QuizManager::NextQuestion()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!session_.is_active) {
        return false;
    }
    
    if (session_.HasNextQuestion()) {
        session_.current_question_index++;
        SetState(QuizState::QUESTION_DISPLAY);
        
        const QuizQuestion* next = session_.GetCurrentQuestion();
        if (next && on_question_ready_) {
            on_question_ready_(*next);
        }
        
        return true;
    } else {
        // Quiz complete
        SetState(QuizState::SHOWING_RESULT);
        
        ESP_LOGI(TAG, "Quiz complete! Score: %d/%d",
                 session_.GetCorrectCount(),
                 static_cast<int>(session_.questions.size()));
        
        if (on_quiz_complete_) {
            on_quiz_complete_(session_);
        }
        
        return false;
    }
}

std::string QuizManager::GenerateResultSummary() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    int correct = session_.GetCorrectCount();
    int total = static_cast<int>(session_.questions.size());
    int wrong = total - correct;
    
    std::string summary;
    summary.reserve(512);  // Pre-allocate to avoid reallocations
    
    // Vietnamese result announcement
    if (wrong == 0) {
        summary = "Chúc mừng! Bạn đã trả lời đúng tất cả " + 
                  std::to_string(total) + " câu hỏi!";
    } else {
        summary = "Kết quả: Bạn trả lời đúng " + std::to_string(correct) + 
                  " trên " + std::to_string(total) + " câu. ";
        summary += "Sai " + std::to_string(wrong) + " câu. ";
        
        // List wrong answers
        auto wrong_answers = GetWrongAnswers();
        for (const auto& wa : wrong_answers) {
            summary += "Câu " + std::to_string(wa.question_number) + 
                       " sai, đáp án đúng là " + std::string(1, wa.correct_answer) + 
                       ": " + wa.correct_option_text + ". ";
        }
    }
    
    return summary;
}

std::vector<QuizManager::WrongAnswerInfo> QuizManager::GetWrongAnswers() const
{
    // Note: Called from GenerateResultSummary which already holds mutex
    std::vector<WrongAnswerInfo> wrong_list;
    
    for (const auto& answer : session_.user_answers) {
        if (!answer.is_correct) {
            WrongAnswerInfo info;
            info.question_number = answer.question_number;
            info.user_answer = answer.selected_answer;
            
            // Find the question to get correct answer text
            for (const auto& q : session_.questions) {
                if (q.question_number == answer.question_number) {
                    info.correct_answer = q.correct_answer;
                    info.correct_option_text = q.GetOption(q.correct_answer);
                    break;
                }
            }
            
            wrong_list.push_back(info);
        }
    }
    
    return wrong_list;
}

// ==================== File Parsing ====================

bool QuizManager::ParseQuizFile(const std::string& file_path)
{
    FILE* file = fopen(file_path.c_str(), "r");
    if (!file) {
        ESP_LOGE(TAG, "Cannot open file: %s", file_path.c_str());
        return false;
    }
    
    // Fixed-size buffer for streaming read
    char line_buffer[QUIZ_FILE_BUFFER_SIZE];
    in_question_ = false;
    pending_question_.Clear();
    
    int line_count = 0;
    bool header_parsed = false;
    (void)header_parsed;  // Suppress unused variable warning
    
    while (fgets(line_buffer, sizeof(line_buffer), file)) {
        line_count++;
        
        // Remove trailing newline/carriage return
        size_t len = strlen(line_buffer);
        while (len > 0 && (line_buffer[len-1] == '\n' || line_buffer[len-1] == '\r')) {
            line_buffer[--len] = '\0';
        }
        
        // Skip empty lines
        if (len == 0) {
            continue;
        }
        
        // Check for end marker
        if (strncmp(line_buffer, "---END---", 9) == 0) {
            // Save pending question if any
            if (in_question_ && ValidateQuestion(pending_question_)) {
                session_.questions.push_back(pending_question_);
            }
            break;
        }
        
        // Parse header lines (start with #)
        if (line_buffer[0] == '#') {
            ParseHeader(line_buffer);
            header_parsed = true;
            continue;
        }
        
        // Check for question marker ---Q{n}---
        if (strncmp(line_buffer, "---Q", 4) == 0) {
            // Save previous question if valid
            if (in_question_ && ValidateQuestion(pending_question_)) {
                session_.questions.push_back(pending_question_);
                
                // Check max questions limit
                if (session_.questions.size() >= CONFIG_QUIZ_MAX_QUESTIONS) {
                    ESP_LOGW(TAG, "Max questions limit reached: %d", CONFIG_QUIZ_MAX_QUESTIONS);
                    break;
                }
            }
            
            // Start new question
            pending_question_.Clear();
            in_question_ = true;
            
            // Parse question number from ---Q{n}---
            int q_num = 0;
            if (sscanf(line_buffer, "---Q%d---", &q_num) == 1) {
                pending_question_.question_number = q_num;
            }
            continue;
        }
        
        // Parse question content
        if (in_question_) {
            ParseQuestionLine(line_buffer, pending_question_);
        }
    }
    
    fclose(file);
    
    ESP_LOGI(TAG, "Parsed %d lines, found %d questions",
             line_count, static_cast<int>(session_.questions.size()));
    
    return !session_.questions.empty();
}

bool QuizManager::ParseHeader(const char* line)
{
    if (strncmp(line, "# QUIZ:", 7) == 0) {
        const char* value = line + 7;
        while (*value == ' ') value++;  // Skip whitespace
        session_.metadata.title = value;
        ESP_LOGI(TAG, "Quiz title: %s", session_.metadata.title.c_str());
        return true;
    }
    
    if (strncmp(line, "# SUBJECT:", 10) == 0) {
        const char* value = line + 10;
        while (*value == ' ') value++;
        session_.metadata.subject = value;
        ESP_LOGI(TAG, "Subject: %s", session_.metadata.subject.c_str());
        return true;
    }
    
    if (strncmp(line, "# TOTAL:", 8) == 0) {
        session_.metadata.total_questions = atoi(line + 8);
        ESP_LOGI(TAG, "Total questions hint: %d", session_.metadata.total_questions);
        return true;
    }
    
    return false;
}

bool QuizManager::ParseQuestionLine(const char* line, QuizQuestion& question)
{
    size_t len = strlen(line);
    if (len < 2) return false;
    
    // Check for ANSWER: line
    if (strncmp(line, "ANSWER:", 7) == 0) {
        const char* ans = line + 7;
        while (*ans == ' ') ans++;
        if (*ans >= 'A' && *ans <= 'D') {
            question.correct_answer = *ans;
        } else if (*ans >= 'a' && *ans <= 'd') {
            question.correct_answer = *ans - 'a' + 'A';
        }
        return true;
    }
    
    // Check for option lines (A. B. C. D.)
    if ((line[0] >= 'A' && line[0] <= 'D') && line[1] == '.') {
        int option_index = line[0] - 'A';
        const char* option_text = line + 2;
        while (*option_text == ' ') option_text++;
        
        // Truncate if too long
        if (strlen(option_text) > QUIZ_MAX_OPTION_LEN) {
            question.options[option_index] = std::string(option_text, QUIZ_MAX_OPTION_LEN);
        } else {
            question.options[option_index] = option_text;
        }
        return true;
    }
    
    // Otherwise, it's question text (possibly multi-line)
    if (question.question_text.empty()) {
        // Truncate if too long
        if (len > QUIZ_MAX_QUESTION_LEN) {
            question.question_text = std::string(line, QUIZ_MAX_QUESTION_LEN);
        } else {
            question.question_text = line;
        }
    } else if (question.question_text.length() < QUIZ_MAX_QUESTION_LEN) {
        // Append for multi-line questions
        question.question_text += " ";
        size_t remaining = QUIZ_MAX_QUESTION_LEN - question.question_text.length();
        if (len > remaining) {
            question.question_text.append(line, remaining);
        } else {
            question.question_text += line;
        }
    }
    
    return true;
}

bool QuizManager::ValidateQuestion(const QuizQuestion& question)
{
    if (question.question_text.empty()) {
        ESP_LOGW(TAG, "Q%d: Empty question text", question.question_number);
        return false;
    }
    
    if (question.correct_answer < 'A' || question.correct_answer > 'D') {
        ESP_LOGW(TAG, "Q%d: Invalid correct answer: %c", 
                 question.question_number, question.correct_answer);
        return false;
    }
    
    // Check at least correct answer option exists
    int correct_idx = question.correct_answer - 'A';
    if (question.options[correct_idx].empty()) {
        ESP_LOGW(TAG, "Q%d: Correct answer option is empty", question.question_number);
        return false;
    }
    
    return true;
}

// ==================== File Discovery ====================

std::vector<std::string> QuizManager::FindQuizFiles()
{
    std::vector<std::string> files;
    const char* quiz_dir = "/sdcard/quiz";
    
    ESP_LOGI(TAG, "=== Quiz File Discovery ===");
    ESP_LOGI(TAG, "Searching for quiz files in: %s", quiz_dir);
    
    // First check if /sdcard is accessible
    DIR* sdcard_dir = opendir("/sdcard");
    if (!sdcard_dir) {
        ESP_LOGE(TAG, "Cannot access /sdcard - SD card not mounted!");
        return files;
    }
    ESP_LOGI(TAG, "/sdcard is accessible");
    closedir(sdcard_dir);
    
    // Try to open quiz directory
    DIR* dir = opendir(quiz_dir);
    if (!dir) {
        ESP_LOGW(TAG, "Quiz directory not found: %s", quiz_dir);
        ESP_LOGI(TAG, "Attempting to create quiz directory...");
        
        // Try to create the directory
        if (mkdir(quiz_dir, 0755) == 0) {
            ESP_LOGI(TAG, "SUCCESS: Created quiz directory: %s", quiz_dir);
            ESP_LOGI(TAG, ">>> Please copy your quiz .txt files to this directory and restart <<<");
        } else {
            ESP_LOGE(TAG, "FAILED: Cannot create quiz directory (errno=%d: %s)", 
                     errno, strerror(errno));
        }
        return files;
    }
    
    ESP_LOGI(TAG, "Quiz directory opened successfully");
    
    struct dirent* entry;
    int total_entries = 0;
    while ((entry = readdir(dir)) != nullptr) {
        const char* name = entry->d_name;
        
        // Skip . and ..
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            continue;
        }
        
        total_entries++;
        size_t len = strlen(name);
        
        ESP_LOGI(TAG, "  Entry[%d]: '%s' (len=%d)", total_entries, name, (int)len);
        
        // Check for .txt or .TXT extension (case-insensitive)
        if (len > 4) {
            const char* ext = name + len - 4;
            bool is_txt = (ext[0] == '.' &&
                          (ext[1] == 't' || ext[1] == 'T') &&
                          (ext[2] == 'x' || ext[2] == 'X') &&
                          (ext[3] == 't' || ext[3] == 'T'));
            
            if (is_txt) {
                std::string path = std::string(quiz_dir) + "/" + name;
                files.push_back(path);
                ESP_LOGI(TAG, "  -> MATCHED as quiz file!");
            }
        }
    }
    
    closedir(dir);
    
    ESP_LOGI(TAG, "=== Scan Complete ===");
    ESP_LOGI(TAG, "Total entries: %d, Quiz files found: %d", 
             total_entries, static_cast<int>(files.size()));
    
    if (files.empty()) {
        ESP_LOGW(TAG, "No quiz files found!");
        ESP_LOGW(TAG, "Please add .txt files to: %s", quiz_dir);
    }
    
    return files;
}
