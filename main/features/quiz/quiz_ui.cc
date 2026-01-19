#include "quiz_ui.h"
#include <esp_log.h>
#include <cstdio>
#include "board.h"
#include "display/lcd_display.h"
#include "lvgl_theme.h"
#include "application.h"

#define TAG "QuizUI"

// ==================== Neon Theme Colors ====================
// Match the weather standby screen cyan neon theme
static const lv_color_t COLOR_BACKGROUND = lv_color_hex(0x000000);   // Pure black for neon contrast
static const lv_color_t COLOR_PANEL_BG = lv_color_hex(0x101010);     // Very dark grey
static const lv_color_t COLOR_TEXT = lv_color_hex(0xFFFFFF);         // White text

// Neon colors (matching weather_ui)
static const lv_color_t COLOR_NEON_CYAN = lv_color_hex(0x00FFFF);    // Cyan for question box
static const lv_color_t COLOR_NEON_GREEN = lv_color_hex(0x39FF14);   // Green for correct
static const lv_color_t COLOR_NEON_RED = lv_color_hex(0xFF3366);     // Red for wrong
static const lv_color_t COLOR_NEON_ORANGE = lv_color_hex(0xFFA500);  // Orange accent

// Button neon colors
static const lv_color_t COLOR_BUTTON_A = lv_color_hex(0x00BFFF);     // Deep sky blue
static const lv_color_t COLOR_BUTTON_B = lv_color_hex(0x00CED1);     // Dark turquoise
static const lv_color_t COLOR_BUTTON_C = lv_color_hex(0x20B2AA);     // Light sea green
static const lv_color_t COLOR_BUTTON_D = lv_color_hex(0x48D1CC);     // Medium turquoise

// Helper: Apply neon box styling with glow effect
static void StyleNeonBox(lv_obj_t* obj, lv_color_t neon_color, int border_width = 2) {
    lv_obj_set_style_bg_color(obj, COLOR_PANEL_BG, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_0, 0); // Transparent background as requested
    lv_obj_set_style_border_color(obj, neon_color, 0);
    lv_obj_set_style_border_width(obj, border_width, 0);
    lv_obj_set_style_radius(obj, 10, 0);
    
    // Shadow for glow effect
    lv_obj_set_style_shadow_width(obj, 15, 0);
    lv_obj_set_style_shadow_color(obj, neon_color, 0);
    lv_obj_set_style_shadow_spread(obj, 2, 0);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_60, 0);
}

// Helper: Apply neon button styling
static void StyleNeonButton(lv_obj_t* btn, lv_color_t neon_color) {
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x101010), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_20, 0); // Low opacity for buttons
    lv_obj_set_style_border_color(btn, neon_color, 0);
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_radius(btn, 8, 0);
    
    // Glow effect
    lv_obj_set_style_shadow_width(btn, 12, 0);
    lv_obj_set_style_shadow_color(btn, neon_color, 0);
    lv_obj_set_style_shadow_spread(btn, 1, 0);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_50, 0);
    
    // Pressed state - brighter glow
    lv_obj_set_style_bg_color(btn, neon_color, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn, LV_OPA_30, LV_STATE_PRESSED);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_80, LV_STATE_PRESSED);
}

QuizUI::QuizUI()
{
    ESP_LOGI(TAG, "QuizUI created");
}

QuizUI::~QuizUI()
{
    Cleanup();
    ESP_LOGI(TAG, "QuizUI destroyed");
}

void QuizUI::Cleanup()
{
    // LVGL objects are automatically cleaned up when parent is deleted
    // But we'll be explicit for clarity and safety
    
    if (quiz_panel_) {
        lv_obj_del(quiz_panel_);
        quiz_panel_ = nullptr;
    }
    
    if (results_panel_) {
        lv_obj_del(results_panel_);
        results_panel_ = nullptr;
    }
    
    // Clear all pointers (they're children of panels, so already deleted)
    progress_label_ = nullptr;
    question_container_ = nullptr;
    question_label_ = nullptr;
    buttons_container_ = nullptr;
    button_a_ = button_b_ = button_c_ = button_d_ = nullptr;
    for (int i = 0; i < 4; i++) option_labels_[i] = nullptr;
    results_title_ = nullptr;
    results_score_ = nullptr;
    results_details_ = nullptr;
    
    is_initialized_ = false;
    
    ESP_LOGI(TAG, "QuizUI cleanup complete");
}

void QuizUI::SetupQuizUI(lv_obj_t* parent, int screen_width, int screen_height)
{
    if (is_initialized_) {
        ESP_LOGW(TAG, "QuizUI already initialized, cleaning up first");
        Cleanup();
    }
    
    parent_ = parent;
    screen_width_ = screen_width;
    screen_height_ = screen_height;

    // Get font from current theme to ensure Vietnamese support
    auto display = Board::GetInstance().GetDisplay();
    if (display && display->GetTheme()) {
        auto theme = static_cast<LvglTheme*>(display->GetTheme());
        if (theme && theme->text_font()) {
            quiz_font_ = theme->text_font()->font();
        }
    }
    
    // Create main quiz panel (full screen overlay)
    // Use lv_layer_top() to ensure it covers everything (status bar, etc.)
    quiz_panel_ = lv_obj_create(lv_layer_top());
    lv_obj_set_size(quiz_panel_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(quiz_panel_, COLOR_BACKGROUND, 0);
    lv_obj_set_style_bg_opa(quiz_panel_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(quiz_panel_, 0, 0);
    lv_obj_set_style_radius(quiz_panel_, 0, 0);
    lv_obj_set_style_pad_all(quiz_panel_, 0, 0); // No padding on main panel to ensure full coverage
    lv_obj_set_scrollbar_mode(quiz_panel_, LV_SCROLLBAR_MODE_OFF);
    
    // Flex layout for vertical arrangement
    lv_obj_set_flex_flow(quiz_panel_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(quiz_panel_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER); // Center everything
    // Use inner container for padding if needed, or apply pad to column items
    
    CreateProgressBar();
    CreateQuestionPanel();
    CreateAnswerButtons();
    CreateResultsPanel();
    CreateCloseButton();
    
    // Initially hidden
    lv_obj_add_flag(quiz_panel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(results_panel_, LV_OBJ_FLAG_HIDDEN);
    
    is_initialized_ = true;
    ESP_LOGI(TAG, "QuizUI setup complete (%dx%d)", screen_width, screen_height);
}

void QuizUI::CreateProgressBar()
{
    // Progress label at top with neon cyan text
    progress_label_ = lv_label_create(quiz_panel_);
    lv_label_set_text(progress_label_, "Câu 1/10");
    if (quiz_font_) lv_obj_set_style_text_font(progress_label_, quiz_font_, 0);
    lv_obj_set_style_text_color(progress_label_, COLOR_NEON_CYAN, 0);  // Neon cyan text
    lv_obj_set_width(progress_label_, LV_PCT(100));
    lv_obj_set_style_text_align(progress_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_top(progress_label_, 5, 0);
    lv_obj_set_style_pad_bottom(progress_label_, 5, 0);
}

void QuizUI::CreateQuestionPanel()
{
    // Scrollable container for question with NEON CYAN border
    question_container_ = lv_obj_create(quiz_panel_);
    lv_obj_set_width(question_container_, LV_PCT(95)); // Slightly smaller than full width for margin
    
    // Auto-fit height to content, but cap it to allow buttons to show
    lv_obj_set_height(question_container_, LV_SIZE_CONTENT); 
    lv_obj_set_style_max_height(question_container_, LV_PCT(60), 0); // Max 60% of screen height
    
    lv_obj_set_style_pad_all(question_container_, 12, 0);
    lv_obj_set_scrollbar_mode(question_container_, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_scroll_dir(question_container_, LV_DIR_VER);
    
    // Apply neon box styling with cyan glow
    StyleNeonBox(question_container_, COLOR_NEON_CYAN, 2);
    
    // Question text label with white text
    question_label_ = lv_label_create(question_container_);
    if (quiz_font_) lv_obj_set_style_text_font(question_label_, quiz_font_, 0);
    lv_label_set_text(question_label_, "");
    lv_obj_set_width(question_label_, LV_PCT(100));
    lv_label_set_long_mode(question_label_, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(question_label_, COLOR_TEXT, 0);
    lv_obj_set_style_text_align(question_label_, LV_TEXT_ALIGN_CENTER, 0); // Center text
}

void QuizUI::CreateAnswerButtons()
{
    // Container for 2x2 button grid
    buttons_container_ = lv_obj_create(quiz_panel_);
    lv_obj_set_width(buttons_container_, LV_PCT(100));
    lv_obj_set_height(buttons_container_, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(buttons_container_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(buttons_container_, 0, 0);
    lv_obj_set_style_pad_all(buttons_container_, 0, 0);
    lv_obj_set_scrollbar_mode(buttons_container_, LV_SCROLLBAR_MODE_OFF);
    
    // Use flex for 2x2 grid layout
    lv_obj_set_flex_flow(buttons_container_, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(buttons_container_, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(buttons_container_, 6, 0);
    lv_obj_set_style_pad_column(buttons_container_, 6, 0);
    
    // Calculate button size (2 columns)
    int btn_width = (screen_width_ - 40) / 2;
    int btn_height = 50;
    
    // Create buttons A, B, C, D
    button_a_ = CreateStyledButton(buttons_container_, "A", OnButtonAClicked, COLOR_BUTTON_A);
    lv_obj_set_size(button_a_, btn_width, btn_height);
    option_labels_[0] = lv_obj_get_child(button_a_, 0);
    
    button_b_ = CreateStyledButton(buttons_container_, "B", OnButtonBClicked, COLOR_BUTTON_B);
    lv_obj_set_size(button_b_, btn_width, btn_height);
    option_labels_[1] = lv_obj_get_child(button_b_, 0);
    
    button_c_ = CreateStyledButton(buttons_container_, "C", OnButtonCClicked, COLOR_BUTTON_C);
    lv_obj_set_size(button_c_, btn_width, btn_height);
    option_labels_[2] = lv_obj_get_child(button_c_, 0);
    
    button_d_ = CreateStyledButton(buttons_container_, "D", OnButtonDClicked, COLOR_BUTTON_D);
    lv_obj_set_size(button_d_, btn_width, btn_height);
    option_labels_[3] = lv_obj_get_child(button_d_, 0);
}

lv_obj_t* QuizUI::CreateStyledButton(lv_obj_t* parent, const char* label_text, 
                                      lv_event_cb_t callback, lv_color_t color)
{
    lv_obj_t* btn = lv_btn_create(parent);
    
    // Apply neon button styling with glow effect
    StyleNeonButton(btn, color);
    
    // Store this pointer for callback
    lv_obj_set_user_data(btn, this);
    lv_obj_add_event_cb(btn, callback, LV_EVENT_CLICKED, this);
    
    // Label with neon colored text
    lv_obj_t* label = lv_label_create(btn);
    if (quiz_font_) lv_obj_set_style_text_font(label, quiz_font_, 0);
    lv_label_set_text(label, label_text);
    lv_obj_set_style_text_color(label, color, 0);  // Match button neon color
    lv_obj_center(label);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(label, lv_pct(95));
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    
    return btn;
}

void QuizUI::CreateResultsPanel()
{
    // Results panel (overlay on quiz_panel) with black background
    results_panel_ = lv_obj_create(lv_layer_top());
    lv_obj_set_size(results_panel_, LV_PCT(100), LV_PCT(100));
    lv_obj_align(results_panel_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(results_panel_, COLOR_BACKGROUND, 0);
    lv_obj_set_style_bg_opa(results_panel_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(results_panel_, 0, 0);
    lv_obj_set_style_radius(results_panel_, 0, 0);
    lv_obj_set_style_pad_all(results_panel_, 16, 0);
    lv_obj_set_scrollbar_mode(results_panel_, LV_SCROLLBAR_MODE_AUTO);
    
    lv_obj_set_flex_flow(results_panel_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(results_panel_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(results_panel_, 10, 0);
    
    // Title with neon cyan color
    results_title_ = lv_label_create(results_panel_);
    if (quiz_font_) lv_obj_set_style_text_font(results_title_, quiz_font_, 0);
    lv_label_set_text(results_title_, "KẾT QUẢ");
    lv_obj_set_style_text_color(results_title_, COLOR_NEON_CYAN, 0);  // Neon cyan
    lv_obj_set_width(results_title_, LV_PCT(100));
    lv_obj_set_style_text_align(results_title_, LV_TEXT_ALIGN_CENTER, 0);
    
    // Score with neon green
    results_score_ = lv_label_create(results_panel_);
    lv_label_set_text(results_score_, "0/0");
    if (quiz_font_) lv_obj_set_style_text_font(results_score_, quiz_font_, 0);
    lv_obj_set_style_text_color(results_score_, COLOR_NEON_GREEN, 0);  // Neon green
    lv_obj_set_width(results_score_, LV_PCT(100));
    lv_obj_set_style_text_align(results_score_, LV_TEXT_ALIGN_CENTER, 0);
    
    // Details container with neon cyan border
    lv_obj_t* details_container = lv_obj_create(results_panel_);
    lv_obj_set_width(details_container, LV_PCT(100));
    lv_obj_set_flex_grow(details_container, 1);
    lv_obj_set_style_pad_all(details_container, 10, 0);
    lv_obj_set_scrollbar_mode(details_container, LV_SCROLLBAR_MODE_AUTO);
    
    // Apply neon box styling
    StyleNeonBox(details_container, COLOR_NEON_CYAN, 2);
    
    results_details_ = lv_label_create(details_container);
    if (quiz_font_) lv_obj_set_style_text_font(results_details_, quiz_font_, 0);
    lv_label_set_text(results_details_, "");
    lv_obj_set_width(results_details_, LV_PCT(100));
    lv_label_set_long_mode(results_details_, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(results_details_, COLOR_TEXT, 0);
}

void QuizUI::CreateCloseButton()
{
    // Close button (X) at top right
    lv_obj_t* close_btn = lv_btn_create(quiz_panel_);
    lv_obj_set_size(close_btn, 40, 40);
    lv_obj_add_flag(close_btn, LV_OBJ_FLAG_FLOATING);
    lv_obj_align(close_btn, LV_ALIGN_TOP_RIGHT, -5, 5);
    
    // Transparent dark style
    lv_obj_set_style_bg_color(close_btn, lv_color_hex(0x303030), 0);
    lv_obj_set_style_bg_opa(close_btn, LV_OPA_80, 0);
    lv_obj_set_style_radius(close_btn, 20, 0); // Circle
    
    lv_obj_t* label = lv_label_create(close_btn);
    lv_label_set_text(label, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_center(label);
    
    lv_obj_add_event_cb(close_btn, [](lv_event_t* e) {
        Application::GetInstance().StopQuizMode();
    }, LV_EVENT_CLICKED, NULL);
}

// ==================== Button Callbacks ====================

void QuizUI::OnButtonAClicked(lv_event_t* e)
{
    QuizUI* ui = static_cast<QuizUI*>(lv_event_get_user_data(e));
    if (ui) ui->HandleButtonClick('A');
}

void QuizUI::OnButtonBClicked(lv_event_t* e)
{
    QuizUI* ui = static_cast<QuizUI*>(lv_event_get_user_data(e));
    if (ui) ui->HandleButtonClick('B');
}

void QuizUI::OnButtonCClicked(lv_event_t* e)
{
    QuizUI* ui = static_cast<QuizUI*>(lv_event_get_user_data(e));
    if (ui) ui->HandleButtonClick('C');
}

void QuizUI::OnButtonDClicked(lv_event_t* e)
{
    QuizUI* ui = static_cast<QuizUI*>(lv_event_get_user_data(e));
    if (ui) ui->HandleButtonClick('D');
}

void QuizUI::HandleButtonClick(char answer)
{
    ESP_LOGI(TAG, "Button clicked: %c", answer);
    last_selected_ = answer;
    
    if (on_answer_press_) {
        on_answer_press_(answer);
    }
}

// ==================== Display Methods ====================

void QuizUI::Show()
{
    if (!is_initialized_) {
        ESP_LOGW(TAG, "QuizUI not initialized");
        return;
    }
    
    lv_obj_remove_flag(quiz_panel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(results_panel_, LV_OBJ_FLAG_HIDDEN);
    ESP_LOGI(TAG, "Quiz panel shown");
}

void QuizUI::Hide()
{
    if (!is_initialized_) return;
    
    lv_obj_add_flag(quiz_panel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(results_panel_, LV_OBJ_FLAG_HIDDEN);
    ESP_LOGI(TAG, "Quiz panel hidden");
}

bool QuizUI::IsVisible() const
{
    if (!is_initialized_ || !quiz_panel_) return false;
    return !lv_obj_has_flag(quiz_panel_, LV_OBJ_FLAG_HIDDEN);
}

void QuizUI::ShowQuestion(const QuizQuestion& question, int current_index, int total_questions)
{
    if (!is_initialized_) return;
    
    // Update progress
    char progress_text[32];
    snprintf(progress_text, sizeof(progress_text), "Câu %d/%d", 
             current_index + 1, total_questions);
    lv_label_set_text(progress_label_, progress_text);
    
    // Update question text
    lv_label_set_text(question_label_, question.question_text.c_str());
    
    // Update button labels with options
    const char* prefixes[] = {"A. ", "B. ", "C. ", "D. "};
    lv_obj_t* buttons[] = {button_a_, button_b_, button_c_, button_d_};
    
    for (int i = 0; i < 4; i++) {
        if (option_labels_[i]) {
            std::string btn_text = std::string(prefixes[i]) + question.options[i];
            lv_label_set_text(option_labels_[i], btn_text.c_str());
        }
        
        // Reset button style
        if (buttons[i]) {
            lv_color_t colors[] = {COLOR_BUTTON_A, COLOR_BUTTON_B, COLOR_BUTTON_C, COLOR_BUTTON_D};
            lv_obj_set_style_bg_color(buttons[i], colors[i], 0);
        }
    }
    
    // Enable buttons
    SetAnswerButtonsEnabled(true);
    
    // Show quiz panel, hide results
    lv_obj_remove_flag(quiz_panel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(results_panel_, LV_OBJ_FLAG_HIDDEN);
    
    // Scroll question to top
    lv_obj_scroll_to_y(question_container_, 0, LV_ANIM_OFF);
    
    ESP_LOGI(TAG, "Showing question %d: %s", 
             current_index + 1, question.question_text.c_str());
}

void QuizUI::ShowAnswerFeedback(char selected, char correct, bool is_correct)
{
    if (!is_initialized_) return;
    
    lv_obj_t* buttons[] = {button_a_, button_b_, button_c_, button_d_};
    
    // Highlight selected and correct answers with neon glow
    for (int i = 0; i < 4; i++) {
        char letter = 'A' + i;
        if (buttons[i]) {
            if (letter == correct) {
                // Correct answer - neon green glow
                lv_obj_set_style_border_color(buttons[i], COLOR_NEON_GREEN, 0);
                lv_obj_set_style_shadow_color(buttons[i], COLOR_NEON_GREEN, 0);
                lv_obj_set_style_shadow_opa(buttons[i], LV_OPA_80, 0);
            } else if (letter == selected && !is_correct) {
                // Wrong selection - neon red glow
                lv_obj_set_style_border_color(buttons[i], COLOR_NEON_RED, 0);
                lv_obj_set_style_shadow_color(buttons[i], COLOR_NEON_RED, 0);
                lv_obj_set_style_shadow_opa(buttons[i], LV_OPA_80, 0);
            }
        }
    }
    
    // Disable buttons during feedback
    SetAnswerButtonsEnabled(false);
}

void QuizUI::ShowResults(int correct_count, int total_count, const std::string& wrong_details)
{
    if (!is_initialized_) return;
    
    // Update score
    char score_text[64];
    snprintf(score_text, sizeof(score_text), "Đúng: %d/%d", correct_count, total_count);
    lv_label_set_text(results_score_, score_text);
    
    // Set color based on score with neon colors
    float percentage = (float)correct_count / (float)total_count;
    if (percentage >= 0.8f) {
        lv_obj_set_style_text_color(results_score_, COLOR_NEON_GREEN, 0);  // Neon green
    } else if (percentage >= 0.5f) {
        lv_obj_set_style_text_color(results_score_, COLOR_NEON_ORANGE, 0); // Neon orange
    } else {
        lv_obj_set_style_text_color(results_score_, COLOR_NEON_RED, 0);    // Neon red
    }
    
    // Update details
    if (wrong_details.empty()) {
        lv_label_set_text(results_details_, "Chúc mừng! Bạn đã trả lời đúng tất cả!");
    } else {
        lv_label_set_text(results_details_, wrong_details.c_str());
    }
    
    // Show results panel
    lv_obj_add_flag(quiz_panel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(results_panel_, LV_OBJ_FLAG_HIDDEN);
    
    ESP_LOGI(TAG, "Showing results: %d/%d", correct_count, total_count);
}

void QuizUI::SetAnswerButtonsEnabled(bool enabled)
{
    if (!is_initialized_) return;
    
    lv_obj_t* buttons[] = {button_a_, button_b_, button_c_, button_d_};
    for (auto* btn : buttons) {
        if (btn) {
            if (enabled) {
                lv_obj_remove_state(btn, LV_STATE_DISABLED);
            } else {
                lv_obj_add_state(btn, LV_STATE_DISABLED);
            }
        }
    }
}
