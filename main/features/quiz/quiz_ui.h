#ifndef QUIZ_UI_H
#define QUIZ_UI_H

#include "quiz_model.h"
#include <lvgl.h>
#include <functional>
#include <string>

class Display;

/**
 * @brief Callback for answer button press
 */
using QuizAnswerPressCallback = std::function<void(char answer)>;

/**
 * @brief Quiz UI - LVGL-based quiz display with touch buttons
 * 
 * Memory-safe design:
 * - All LVGL objects properly cleaned up in destructor
 * - Uses existing LVGL memory management
 */
class QuizUI {
public:
    QuizUI();
    ~QuizUI();
    
    // Prevent copying
    QuizUI(const QuizUI&) = delete;
    QuizUI& operator=(const QuizUI&) = delete;
    
    // ==================== Setup ====================
    
    /**
     * @brief Initialize quiz UI on parent screen
     * @param parent Parent LVGL object (usually screen or container)
     * @param screen_width Display width
     * @param screen_height Display height
     * @param display Display pointer for thread safety
     */
    void SetupQuizUI(lv_obj_t* parent, int screen_width, int screen_height, Display* display = nullptr);
    
    /**
     * @brief Set callback for when user presses an answer button
     */
    void SetOnAnswerPress(QuizAnswerPressCallback callback) { on_answer_press_ = callback; }
    
    // ==================== Display ====================
    
    /**
     * @brief Show the quiz panel
     */
    void Show();
    
    /**
     * @brief Hide the quiz panel
     */
    void Hide();
    
    /**
     * @brief Check if quiz panel is visible
     */
    bool IsVisible() const;
    
    /**
     * @brief Display a question
     * @param question Question to display
     * @param current_index 0-based current question index
     * @param total_questions Total number of questions
     */
    void ShowQuestion(const QuizQuestion& question, int current_index, int total_questions);
    
    /**
     * @brief Show answer feedback (correct/wrong indicator)
     * @param selected Selected answer letter
     * @param correct Correct answer letter
     * @param is_correct Whether user was correct
     */
    void ShowAnswerFeedback(char selected, char correct, bool is_correct);
    
    /**
     * @brief Display quiz results
     * @param correct_count Number of correct answers
     * @param total_count Total questions
     * @param wrong_details Details about wrong answers (for display)
     */
    void ShowResults(int correct_count, int total_count, const std::string& wrong_details);
    
    /**
     * @brief Enable or disable answer buttons
     */
    void SetAnswerButtonsEnabled(bool enabled);
    
    /**
     * @brief Cleanup all UI resources
     */
    void Cleanup();

private:
    // ==================== Button Callbacks ====================
    
    static void OnButtonAClicked(lv_event_t* e);
    static void OnButtonBClicked(lv_event_t* e);
    static void OnButtonCClicked(lv_event_t* e);
    static void OnButtonDClicked(lv_event_t* e);
    
    void HandleButtonClick(char answer);
    
    // ==================== UI Creation Helpers ====================
    
    void CreateQuestionPanel();
    void CreateProgressBar();
    void CreateAnswerButtons();
    void CreateResultsPanel();
    void CreateCloseButton();
    
    lv_obj_t* CreateStyledButton(lv_obj_t* parent, const char* label_text, 
                                  lv_event_cb_t callback, lv_color_t color);
    
    void UpdateButtonStyle(lv_obj_t* btn, bool is_selected, bool is_correct, bool show_result);
    
    // ==================== Member Variables ====================
    
    lv_obj_t* parent_ = nullptr;
    int screen_width_ = 0;
    int screen_height_ = 0;
    
    // Main container
    lv_obj_t* quiz_panel_ = nullptr;
    
    // Question display
    lv_obj_t* progress_label_ = nullptr;      // "Câu 1/10"
    lv_obj_t* question_container_ = nullptr;  // Scrollable container
    lv_obj_t* question_label_ = nullptr;      // Question text
    
    // Answer buttons
    lv_obj_t* buttons_container_ = nullptr;
    lv_obj_t* button_a_ = nullptr;
    lv_obj_t* button_b_ = nullptr;
    lv_obj_t* button_c_ = nullptr;
    lv_obj_t* button_d_ = nullptr;
    lv_obj_t* option_labels_[4] = {nullptr};
    
    // Results panel
    lv_obj_t* results_panel_ = nullptr;
    lv_obj_t* results_title_ = nullptr;
    lv_obj_t* results_score_ = nullptr;
    lv_obj_t* results_details_ = nullptr;
    
    // Callback
    QuizAnswerPressCallback on_answer_press_;
    
    // State
    bool is_initialized_ = false;
    char last_selected_ = '\0';
    
    // Theme font
    const lv_font_t* quiz_font_ = nullptr;
    
    // Display lock
    Display* display_ = nullptr;
};

#endif // QUIZ_UI_H
