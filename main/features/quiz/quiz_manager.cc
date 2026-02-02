#include "quiz_manager.h"
#include <esp_log.h>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <cJSON.h>
#include <cstring>
#include <esp_mac.h>
#include <thread>
#include <functional>

#define TAG "QuizManager"

// Maximum buffer size for HTTP response
#define MAX_HTTP_OUTPUT_BUFFER 2048

QuizManager::QuizManager()
    : state_(QuizState::IDLE)
    , client_handle_(nullptr)
{
    ESP_LOGI(TAG, "QuizManager created (Server Mode)");
    session_.Reset();
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

// ==================== HTTP Client Logic ====================

std::string QuizManager::GetDeviceId() const
{
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    char buf[7];
    // Use last 3 bytes as requested: "mã 6 số" (hex)
    snprintf(buf, sizeof(buf), "%02x%02x%02x", mac[3], mac[4], mac[5]);
    return std::string(buf);
}

bool QuizManager::InitHttpClient()
{
    if (client_handle_) {
        return true; // Already initialized
    }

    esp_http_client_config_t config = {};
    config.url = QUIZ_SERVER_URL; // Base URL, will be updated per request
    config.method = HTTP_METHOD_POST;
    config.timeout_ms = 5000;
    config.buffer_size = MAX_HTTP_OUTPUT_BUFFER;
    config.disable_auto_redirect = true;
    config.keep_alive_enable = true; // IMPORTANT: Keep-Alive for valid persistent connections
    config.crt_bundle_attach = esp_crt_bundle_attach; // Enable SSL Certificate Bundle for HTTPS (Render)

    client_handle_ = esp_http_client_init(&config);
    if (!client_handle_) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return false;
    }
    
    ESP_LOGI(TAG, "HTTP Client initialized with Keep-Alive");
    return true;
}

bool QuizManager::SendRequest(const char* endpoint, cJSON* payload, cJSON** response_json)
{
    if (!InitHttpClient()) {
        return false;
    }

    std::string url = std::string(QUIZ_SERVER_URL) + endpoint;
    esp_http_client_set_url(client_handle_, url.c_str());
    esp_http_client_set_method(client_handle_, HTTP_METHOD_POST);
    esp_http_client_set_header(client_handle_, "Content-Type", "application/json");

    // Add common header or ensure payload has deviceId
    // For simplicity, we add deviceId to payload in caller functions, not here generic
    
    char* payload_str = cJSON_PrintUnformatted(payload);
    int payload_len = strlen(payload_str);

    ESP_LOGI(TAG, "Sending POST to %s: %s", endpoint, payload_str);

    esp_err_t err = esp_http_client_open(client_handle_, payload_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP Open failed: %s", esp_err_to_name(err));
        free(payload_str);
        return false;
    }

    int wlen = esp_http_client_write(client_handle_, payload_str, payload_len);
    free(payload_str);
    if (wlen < 0) {
        ESP_LOGE(TAG, "HTTP Write failed");
        esp_http_client_close(client_handle_);
        return false;
    }

    int content_len = esp_http_client_fetch_headers(client_handle_);
    if (content_len < 0) {
        ESP_LOGE(TAG, "HTTP Fetch Headers failed");
        // Even if fetch headers failed, we should try to close clean if possible or just rely on init to reset
        esp_http_client_close(client_handle_); 
        return false;
    }

    int status_code = esp_http_client_get_status_code(client_handle_);
    ESP_LOGI(TAG, "HTTP Status: %d, Content-Len: %d", status_code, content_len);

    if (status_code != 200) {
        ESP_LOGE(TAG, "Server returned error status: %d", status_code);
        esp_http_client_close(client_handle_); // Close connection on error
        return false;
    }

    if (content_len <= 0) {
        content_len = MAX_HTTP_OUTPUT_BUFFER; 
    }
    
    // Allocate buffer for response
    // Check for excessive size
    if (content_len > 10 * 1024) { 
        ESP_LOGE(TAG, "Response too large: %d", content_len);
        esp_http_client_close(client_handle_);
        return false;
    }

    char* buffer = (char*)malloc(content_len + 1);
    if (!buffer) {
        ESP_LOGE(TAG, "Metrics: malloc failed");
        esp_http_client_close(client_handle_);
        return false;
    }
    
    // Read the response body
    // In a robust implementation, we might need a loop if read_len < content_len
    // but for this specific RPC, we usually get it all or standard chunked behavior handled by read_response
    int read_len = esp_http_client_read_response(client_handle_, buffer, content_len);
    
    if (read_len >= 0) {
        buffer[read_len] = 0; // Null terminate
        ESP_LOGI(TAG, "Response: %s", buffer);
        *response_json = cJSON_Parse(buffer);
        free(buffer);
        // We do NOT call esp_http_client_close(client_handle_) here to support Keep-Alive
        // The next InitHttpClient/SendRequest will reuse it if enabled.
        return (*response_json != nullptr);
    } else {
        ESP_LOGE(TAG, "Failed to read response");
        free(buffer);
        esp_http_client_close(client_handle_);
        return false;
    }
}

bool QuizManager::ParseQuestionJson(cJSON* q_obj, int display_index, QuizQuestion& out_question)
{
    if (!q_obj) return false;

    cJSON* text = cJSON_GetObjectItem(q_obj, "text");
    cJSON* options = cJSON_GetObjectItem(q_obj, "options");

    if (!text || !cJSON_IsString(text) || !options || !cJSON_IsArray(options)) {
        return false;
    }

    out_question.Clear();
    out_question.question_number = display_index; // Display number 1-based
    out_question.question_text = text->valuestring;

    int opt_count = cJSON_GetArraySize(options);
    for (int i = 0; i < opt_count && i < 4; i++) {
        cJSON* opt = cJSON_GetArrayItem(options, i);
        if (opt && cJSON_IsString(opt)) {
            out_question.options[i] = opt->valuestring;
        }
    }
    
    return true;
}

// ==================== Quiz Flow ====================

#include <esp_pthread.h> // Add this include

// Helper to run task in background with sufficient stack size
template<typename Func>
void RunInBackground(Func&& func) {
    auto cfg = esp_pthread_get_default_config();
    cfg.stack_size = 10 * 1024; // Increase stack to 10KB to safe for HTTPS/SSL
    cfg.prio = 2; // Lower priority to avoid starving AFE/Audio tasks (who are at 5 or 8)
    cfg.thread_name = "QuizTask";
    esp_pthread_set_cfg(&cfg);

    std::thread([func]() {
        func();
    }).detach();
}

bool QuizManager::StartQuiz()
{
    // Run in background to prevent blocking AFE/Audio loop
    RunInBackground([this]() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (session_.is_active) {
            StopQuizInternal();
        }
        
        ESP_LOGI(TAG, "Starting quiz (connecting to server)...");
        SetState(QuizState::LOADING);
        
        cJSON* req = cJSON_CreateObject();
        if (!req) return;

        cJSON_AddStringToObject(req, "deviceId", GetDeviceId().c_str());
        
        cJSON* resp = nullptr;
        
        if (SendRequest("/api/quiz/start", req, &resp)) {
            cJSON_Delete(req);
            
            if (resp) {
                cJSON* sessId = cJSON_GetObjectItem(resp, "sessionId");
                cJSON* total = cJSON_GetObjectItem(resp, "total");
                cJSON* question = cJSON_GetObjectItem(resp, "question");
                
                if (sessId && cJSON_IsString(sessId) && question) {
                    session_id_ = sessId->valuestring;
                    session_.Reset();
                    session_.is_active = true;
                    session_.metadata.total_questions = total ? total->valueint : 0;
                    
                    QuizQuestion q;
                    if (ParseQuestionJson(question, 1, q)) {
                        session_.questions.clear();
                        session_.questions.push_back(q);
                        session_.current_question_index = 0;
                        
                        SetState(QuizState::QUESTION_DISPLAY);
                        ESP_LOGI(TAG, "Quiz started. Session: %s", session_id_.c_str());
                        
                        if (on_question_ready_) {
                            on_question_ready_(q);
                        }
                    }
                }
                cJSON_Delete(resp);
            }
        } else {
            cJSON_Delete(req);
            ReportError("Failed to start quiz. Check server connection.");
        }
    });

    return true; // Return immediately
}

bool QuizManager::SubmitAnswer(char answer)
{
    // Run in background to prevent blocking AFE
    RunInBackground([this, answer]() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!session_.is_active || session_id_.empty()) {
            return;
        }

        // Prevent multiple submissions
        if (state_ == QuizState::CHECKING_ANSWER || state_ == QuizState::SHOWING_RESULT) {
            ESP_LOGW(TAG, "Ignored duplicate answer submission in state %d", (int)state_);
            return;
        }
        
        if (client_handle_ == nullptr) {
            if (!InitHttpClient()) {
                 ReportError("Connection lost");
                 return;
            }
        }

        SetState(QuizState::CHECKING_ANSWER);
        
        cJSON* req = cJSON_CreateObject();
        if (!req) return;

        cJSON_AddStringToObject(req, "sessionId", session_id_.c_str());
        char ansStr[2] = {answer, 0};
        cJSON_AddStringToObject(req, "answer", ansStr);
        cJSON_AddStringToObject(req, "deviceId", GetDeviceId().c_str()); 
        
        cJSON* resp = nullptr;
        
        if (SendRequest("/api/quiz/answer", req, &resp)) {
            cJSON_Delete(req);
            
            if (resp) {
                cJSON* correct = cJSON_GetObjectItem(resp, "correct");
                cJSON* correctOption = cJSON_GetObjectItem(resp, "correctOption");
                cJSON* nextQProto = cJSON_GetObjectItem(resp, "nextQuestion");
                cJSON* finished = cJSON_GetObjectItem(resp, "finished");
                
                bool is_correct = cJSON_IsTrue(correct);
                char correct_char = 0;
                if (correctOption && cJSON_IsString(correctOption) && correctOption->valuestring) {
                     correct_char = correctOption->valuestring[0];
                }

                if (session_.current_question_index < session_.questions.size()) {
                    session_.questions[session_.current_question_index].correct_answer = correct_char;
                }
                
                UserAnswer user_answer(session_.current_question_index + 1, answer, correct_char, is_correct);
                session_.user_answers.push_back(user_answer);
                
                bool is_last = cJSON_IsTrue(finished);
                
                if (on_answer_checked_) {
                    on_answer_checked_(user_answer, is_last);
                }
                
                if (is_last) {
                    SetState(QuizState::SHOWING_RESULT);
                    if (on_quiz_complete_) {
                        on_quiz_complete_(session_);
                    }
                } else if (nextQProto) {
                    QuizQuestion nextQ;
                    int next_display_num = session_.current_question_index + 2; 
                    
                    if (ParseQuestionJson(nextQProto, next_display_num, nextQ)) {
                        if (session_.questions.size() <= session_.current_question_index + 1) {
                             session_.questions.push_back(nextQ);
                        }
                    }
                }
                cJSON_Delete(resp);
            } else {
                 ReportError("Empty response");
            }
        } else {
            cJSON_Delete(req);
            ReportError("Failed to submit answer");
        }
    });

    return true; 
}


bool QuizManager::StopQuiz()
{
    std::lock_guard<std::mutex> lock(mutex_);
    StopQuizInternal();
    return true;
}

void QuizManager::StopQuizInternal()
{
    // Close / Cleanup HTTP Client
    if (client_handle_) {
        esp_http_client_cleanup(client_handle_);
        client_handle_ = nullptr;
    }
    
    session_.Reset();
    session_id_.clear();
    SetState(QuizState::IDLE);
    ESP_LOGI(TAG, "Quiz Stopped");
}

bool QuizManager::NextQuestion()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    int next_index = session_.current_question_index + 1;
    if (next_index < session_.questions.size()) {
        session_.current_question_index = next_index;
        SetState(QuizState::QUESTION_DISPLAY);
        
        ESP_LOGI(TAG, "Moving to question %d", next_index + 1);
        
        if (on_question_ready_) {
            on_question_ready_(session_.questions[next_index]);
        }
        return true;
    } 
    
    ESP_LOGI(TAG, "NextQuestion called but no next question available (Index: %d, Size: %d)", 
             session_.current_question_index, (int)session_.questions.size());
             
    if (state_ == QuizState::SHOWING_RESULT) {
        return false;
    }
    
    return false;
}

const QuizQuestion* QuizManager::GetCurrentQuestion() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return session_.GetCurrentQuestion();
}

std::vector<QuizManager::WrongAnswerInfo> QuizManager::GetWrongAnswers() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<WrongAnswerInfo> wrong_list;
    
    for (const auto& answer : session_.user_answers) {
        if (!answer.is_correct) {
            WrongAnswerInfo info;
            info.question_number = answer.question_number;
            info.user_answer = answer.selected_answer;
            info.correct_answer = answer.correct_answer;
            
            for (const auto& q : session_.questions) {
                if (q.question_number == answer.question_number) {
                    info.correct_option_text = q.GetOption(q.correct_answer);
                    break;
                }
            }
            
            wrong_list.push_back(info);
        }
    }
    
    return wrong_list;
}

std::string QuizManager::GenerateResultSummary() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    int total = session_.metadata.total_questions;
    int correct = session_.GetCorrectCount();
    
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "Kết quả: Bạn đã trả lời đúng %d trên %d câu hỏi.", correct, total);
    
    if (correct == total) {
        return std::string(buffer) + " Thật tuyệt vời!";
    } else if (correct >= total / 2) {
        return std::string(buffer) + " Khá tốt!";
    } else {
        return std::string(buffer) + " Hãy cố gắng lần sau nhé.";
    }
}
