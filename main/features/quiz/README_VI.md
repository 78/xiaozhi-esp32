# 📚 Hướng Dẫn Chi Tiết: Chức Năng Documentation Quiz

## Mục Lục
1. [Tổng Quan](#1-tổng-quan)
2. [Tư Duy Thiết Kế](#2-tư-duy-thiết-kế)
3. [Kiến Trúc Module](#3-kiến-trúc-module)
4. [Chi Tiết Kỹ Thuật](#4-chi-tiết-kỹ-thuật)
5. [Syntax và Code Pattern](#5-syntax-và-code-pattern)
6. [Workflow Hoạt Động](#6-workflow-hoạt-động)
7. [Hướng Dẫn Test](#7-hướng-dẫn-test)
8. [Xử Lý Lỗi và Debug](#8-xử-lý-lỗi-và-debug)

---

## 1. Tổng Quan

### 1.1. Mục Đích
Chức năng **Documentation Quiz** cho phép người dùng:
- Làm bài kiểm tra từ file câu hỏi trên thẻ SD
- Trả lời bằng **touch screen** hoặc **giọng nói**
- Nhận kết quả chi tiết sau khi hoàn thành

### 1.2. Các Files Tạo Mới

| File | Chức năng |
|------|-----------|
| `features/quiz/quiz_model.h` | Định nghĩa data structures |
| `features/quiz/quiz_manager.h` | Interface quản lý quiz |
| `features/quiz/quiz_manager.cc` | Logic xử lý quiz |
| `features/quiz/quiz_ui.h` | Interface UI LVGL |
| `features/quiz/quiz_ui.cc` | Giao diện người dùng |

### 1.3. Các Files Sửa Đổi

| File | Thay đổi |
|------|----------|
| `Kconfig.projbuild` | Thêm QUIZ_ENABLE options |
| `CMakeLists.txt` | Conditional compilation |
| `device_state.h` | Thêm kDeviceStateQuiz |
| `application.h/cc` | Quiz methods & voice handling |
| `weather_ui.h/cc` | Quiz button trên standby screen |

---

## 2. Tư Duy Thiết Kế

### 2.1. Nguyên Tắc Thiết Kế

#### a) Memory Safety (An toàn bộ nhớ)
ESP32 có RAM hạn chế (~320KB SRAM), nên cần:

```cpp
// ❌ KHÔNG LÀM: Load toàn bộ file vào RAM
std::string content = file.readAll(); // Có thể hết RAM!

// ✅ NÊN LÀM: Streaming parser, đọc theo chunks
char buffer[QUIZ_FILE_BUFFER_SIZE]; // 512 bytes cố định
while (fread(buffer, 1, sizeof(buffer), file)) {
    ParseChunk(buffer);
}
```

#### b) RAII Pattern (Resource Acquisition Is Initialization)
Tự động dọn dẹp tài nguyên khi object bị hủy:

```cpp
class QuizUI {
public:
    ~QuizUI() {
        Cleanup(); // Tự động gọi khi object bị hủy
    }
    
    void Cleanup() {
        if (quiz_panel_) {
            lv_obj_del(quiz_panel_); // Giải phóng LVGL objects
            quiz_panel_ = nullptr;
        }
    }
};
```

#### c) Singleton Pattern cho QuizManager
Đảm bảo chỉ có 1 instance quiz manager:

```cpp
class QuizManager {
public:
    static QuizManager& GetInstance() {
        static QuizManager instance;
        return instance;
    }
private:
    QuizManager() = default; // Constructor private
};
```

### 2.2. Lý Do Chọn Streaming Parser

**Vấn đề:** File quiz có thể lên đến vài MB
**Giải pháp:** Đọc file theo từng dòng, không load toàn bộ

```cpp
bool QuizManager::ParseQuizFile(const std::string& file_path) {
    FILE* file = fopen(file_path.c_str(), "r");
    if (!file) return false;
    
    char line_buffer[512];
    QuizQuestion current_question;
    
    // Đọc từng dòng, không đọc toàn bộ file
    while (fgets(line_buffer, sizeof(line_buffer), file)) {
        ParseLine(line_buffer, current_question);
    }
    
    fclose(file);
    return true;
}
```

---

## 3. Kiến Trúc Module

### 3.1. Sơ Đồ Kiến Trúc

```
┌─────────────────────────────────────────────────────────────┐
│                      APPLICATION LAYER                       │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────────┐  │
│  │ Application │───▶│ QuizManager │───▶│ QuizUI (LVGL)   │  │
│  │ (Singleton) │    │ (State M/C) │    │ (Touch/Display) │  │
│  └─────────────┘    └─────────────┘    └─────────────────┘  │
│         │                  │                    │            │
│         ▼                  ▼                    ▼            │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────────┐  │
│  │AudioService │    │  SD Card    │    │   LcdDisplay    │  │
│  │ (TTS/Voice) │    │ (File I/O)  │    │ (LVGL Render)   │  │
│  └─────────────┘    └─────────────┘    └─────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

### 3.2. State Machine (Máy Trạng Thái)

```cpp
enum class QuizState {
    IDLE,              // Chờ bắt đầu
    LOADING,           // Đang đọc file
    QUESTION_DISPLAY,  // Hiển thị câu hỏi
    WAITING_ANSWER,    // Chờ người dùng trả lời
    CHECKING_ANSWER,   // Kiểm tra đáp án
    SHOWING_RESULT,    // Hiển thị kết quả
    ERROR              // Có lỗi xảy ra
};
```

**Luồng chuyển trạng thái:**
```
IDLE → LOADING → QUESTION_DISPLAY → WAITING_ANSWER
                       ↑                   │
                       └───────────────────┘ (NextQuestion)
                                           │
                                           ▼
                                    SHOWING_RESULT → IDLE
```

---

## 4. Chi Tiết Kỹ Thuật

### 4.1. Format File Câu Hỏi (.txt)

```txt
# QUIZ: Tên bộ câu hỏi
# SUBJECT: Môn học (tùy chọn)
# TOTAL: 5

---Q1---
Câu hỏi số 1?
A. Đáp án A
B. Đáp án B
C. Đáp án C
D. Đáp án D
ANSWER: B

---Q2---
Câu hỏi tiếp theo?
A. Option A
B. Option B
C. Option C
D. Option D
ANSWER: C

---END---
```

**Quy tắc:**
- Encoding: UTF-8
- Max độ dài câu hỏi: 256 ký tự
- Max độ dài đáp án: 128 ký tự
- Phải có marker `---Q{N}---` trước mỗi câu
- Phải có `ANSWER: X` sau các options
- Kết thúc bằng `---END---`

### 4.2. Data Structures

```cpp
// Câu hỏi đơn lẻ
struct QuizQuestion {
    int question_number;
    std::string question_text;
    std::string options[4];  // A, B, C, D
    char correct_answer;     // 'A', 'B', 'C', 'D'
    
    bool IsValid() const {
        return !question_text.empty() && 
               correct_answer >= 'A' && 
               correct_answer <= 'D';
    }
};

// Câu trả lời của user
struct UserAnswer {
    int question_number;
    char selected_answer;  // User chọn
    char correct_answer;   // Đáp án đúng
    bool is_correct;
    
    UserAnswer() : is_correct(false) {}
};

// Session quiz
struct QuizSession {
    std::string title;
    std::string subject;
    std::vector<QuizQuestion> questions;
    std::vector<UserAnswer> user_answers;
    int current_index;
    
    int GetCorrectCount() const {
        return std::count_if(user_answers.begin(), user_answers.end(),
            [](const UserAnswer& a) { return a.is_correct; });
    }
};
```

### 4.3. Callback Pattern

```cpp
// Định nghĩa callback types
using QuestionReadyCallback = std::function<void(const QuizQuestion&)>;
using AnswerCheckedCallback = std::function<void(const UserAnswer&, bool is_last)>;
using QuizCompleteCallback = std::function<void(const QuizSession&)>;
using ErrorCallback = std::function<void(const std::string&)>;

// Trong QuizManager
class QuizManager {
private:
    QuestionReadyCallback on_question_ready_;
    AnswerCheckedCallback on_answer_checked_;
    QuizCompleteCallback on_quiz_complete_;
    ErrorCallback on_error_;
    
public:
    void SetOnQuestionReady(QuestionReadyCallback cb) { 
        on_question_ready_ = cb; 
    }
    
    // Gọi callback khi có câu hỏi mới
    void NotifyQuestionReady(const QuizQuestion& q) {
        if (on_question_ready_) {
            on_question_ready_(q);
        }
    }
};
```

---

## 5. Syntax và Code Pattern

### 5.1. Conditional Compilation

```cpp
// Trong header files - bảo vệ nếu feature không enabled
#ifdef CONFIG_QUIZ_ENABLE
    void StartQuizMode(const std::string& quiz_file = "");
    void StopQuizMode();
#endif

// Trong implementation
#ifdef CONFIG_QUIZ_ENABLE
void Application::StartQuizMode(const std::string& quiz_file) {
    // Implementation
}
#endif // CONFIG_QUIZ_ENABLE
```

### 5.2. LVGL Button với Lambda Callback

```cpp
// Tạo button và gắn callback
lv_obj_t* btn = lv_btn_create(parent);
lv_obj_set_size(btn, 100, 50);

// Lambda callback - capture `this` để truy cập member
lv_obj_add_event_cb(btn, [](lv_event_t* e) {
    // Lấy user_data đã truyền vào
    QuizUI* ui = static_cast<QuizUI*>(lv_event_get_user_data(e));
    if (ui) {
        ui->HandleButtonClick('A');
    }
}, LV_EVENT_CLICKED, this);  // `this` là user_data
```

### 5.3. Voice Keyword Detection

```cpp
bool Application::HandleQuizVoiceInput(const std::string& text) {
    // Keywords để kích hoạt quiz
    static const std::vector<std::string> quiz_keywords = {
        "tài liệu", "tai lieu",
        "kiểm tra", "kiem tra",
        "làm bài tập", "lam bai tap"
    };
    
    // Convert to lowercase để so sánh
    std::string lower_text = text;
    std::transform(lower_text.begin(), lower_text.end(), 
                   lower_text.begin(), ::tolower);
    
    // Tìm keyword trong text
    for (const auto& keyword : quiz_keywords) {
        if (lower_text.find(keyword) != std::string::npos) {
            StartQuizMode();
            return true;  // Đã xử lý
        }
    }
    return false;  // Không phải quiz command
}
```

### 5.4. Schedule Pattern (Thread-safe)

```cpp
// Application::Schedule() đảm bảo code chạy trên main thread
Schedule([this, display, question]() {
    // Code này chạy trên main thread, an toàn với LVGL
    display->SetChatMessage("system", question.question_text.c_str());
});
```

### 5.5. Smart Pointer Memory Management

```cpp
class Application {
private:
    // unique_ptr tự động delete khi Application bị hủy
    std::unique_ptr<QuizManager> quiz_manager_;
    
public:
    void StartQuizMode(const std::string& quiz_file) {
        // Lazy initialization
        if (!quiz_manager_) {
            quiz_manager_ = std::make_unique<QuizManager>();
        }
    }
};
```

---

## 6. Workflow Hoạt Động

### 6.1. Kích Hoạt Quiz

```
┌──────────────────────────────────────────────────────────────┐
│                    USER INPUT                                 │
├──────────────────────────────────────────────────────────────┤
│                                                               │
│  ┌─────────────┐          ┌─────────────────────────┐        │
│  │ Touch Quiz  │    OR    │ Voice: "làm bài tập"   │        │
│  │   Button    │          │ "kiểm tra" / "tài liệu" │        │
│  └──────┬──────┘          └───────────┬─────────────┘        │
│         │                             │                       │
│         ▼                             ▼                       │
│  ┌──────────────────────────────────────────────────┐        │
│  │            Application::StartQuizMode()           │        │
│  └──────────────────────────────────────────────────┘        │
│                          │                                    │
│                          ▼                                    │
│  ┌──────────────────────────────────────────────────┐        │
│  │  1. Create QuizManager if not exists              │        │
│  │  2. Find quiz files on SD card                    │        │
│  │  3. Parse first quiz file                         │        │
│  │  4. Set device state to kDeviceStateQuiz          │        │
│  │  5. Display first question                        │        │
│  └──────────────────────────────────────────────────┘        │
└──────────────────────────────────────────────────────────────┘
```

### 6.2. Luồng Trả Lời

```
┌────────────────────────────────────────────────────────────┐
│                     ANSWER FLOW                             │
├────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌───────────────┐        ┌──────────────────────┐         │
│  │ Touch Button  │   OR   │ Voice: "đáp án A"    │         │
│  │    A/B/C/D    │        │ "chọn B" / "câu C"   │         │
│  └───────┬───────┘        └──────────┬───────────┘         │
│          │                           │                      │
│          ▼                           ▼                      │
│  ┌─────────────────────────────────────────────────────┐   │
│  │            QuizManager::SubmitAnswer(char)           │   │
│  └─────────────────────────────────────────────────────┘   │
│                          │                                  │
│                          ▼                                  │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  1. Compare với correct_answer                       │   │
│  │  2. Store UserAnswer với is_correct                  │   │
│  │  3. Trigger on_answer_checked_ callback              │   │
│  │  4. Display: "Đúng!" hoặc "Sai!"                     │   │
│  │  5. Wait 1.5s, call NextQuestion()                   │   │
│  └─────────────────────────────────────────────────────┘   │
│                          │                                  │
│            ┌─────────────┴─────────────┐                   │
│            ▼                           ▼                    │
│     ┌────────────┐              ┌────────────┐             │
│     │ Còn câu   │              │ Hết câu    │             │
│     └─────┬──────┘              └──────┬─────┘             │
│           │                            │                    │
│           ▼                            ▼                    │
│   NextQuestion()              ShowResults()                 │
│                                                             │
└────────────────────────────────────────────────────────────┘
```

### 6.3. Hiển Thị Kết Quả

```cpp
std::string QuizManager::GenerateResultSummary() {
    std::string summary;
    int correct = session_.GetCorrectCount();
    int total = session_.questions.size();
    
    // Header
    summary = "KẾT QUẢ: " + std::to_string(correct) + "/" + 
              std::to_string(total) + " câu đúng\n\n";
    
    // Chi tiết các câu sai
    auto wrong_answers = GetWrongAnswers();
    if (!wrong_answers.empty()) {
        summary += "Các câu sai:\n";
        for (const auto& wa : wrong_answers) {
            // Format: "Câu 3 sai - Đáp án đúng: B. nội dung"
            summary += "• Câu " + std::to_string(wa.question_number) + 
                       " sai - Đáp án đúng: " + wa.correct_answer + ". ";
            
            // Thêm nội dung đáp án đúng
            int correct_idx = wa.correct_answer - 'A';
            summary += session_.questions[wa.question_number-1]
                       .options[correct_idx] + "\n";
        }
    }
    
    return summary;
}
```

---

## 7. Hướng Dẫn Test

### 7.1. Chuẩn Bị

#### Bước 1: Enable Features trong Menuconfig

```bash
cd xiaozhi-esp32
idf.py menuconfig
```

Navigate to:
```
Xiaozhi Assistant
  └── [*] Enable SD Card
  └── [*] Enable Documentation Quiz Feature
          └── (50) Maximum questions per quiz
          └── [*] Enable voice answer recognition
```

#### Bước 2: Tạo File Quiz

Tạo file `sample_quiz.txt` trên thẻ SD trong thư mục `/quiz/`:

```
/sdcard/
  └── quiz/
      └── sample_quiz.txt
```

Nội dung file (đã có sẵn trong project):
```txt
# QUIZ: Bài tập Toán Lớp 1
# SUBJECT: Toán học
# TOTAL: 5

---Q1---
Một cộng một bằng bao nhiêu?
A. 1
B. 2
C. 3
D. 4
ANSWER: B

---Q2---
Hai cộng hai bằng mấy?
A. 2
B. 3
C. 4
D. 5
ANSWER: C

---END---
```

#### Bước 3: Build và Flash

```bash
# Set target (nếu chưa set)
idf.py set-target esp32s3

# Build
idf.py build

# Flash
idf.py -p COMx flash

# Monitor logs
idf.py -p COMx monitor
```

### 7.2. Test Cases

#### Test Case 1: Touch Button Trigger

| Step | Action | Expected Result |
|------|--------|-----------------|
| 1 | Chờ device vào standby screen | Hiện màn hình idle với thời tiết |
| 2 | Nhấn nút Quiz (icon sách màu xanh) | Chuyển sang Quiz Mode |
| 3 | Xem câu hỏi đầu tiên | Hiển thị "Câu 1/5: Một cộng một..." |
| 4 | Nhấn button "B" | Hiển thị "Đúng!" |
| 5 | Chờ 1.5s | Chuyển sang câu 2 |

#### Test Case 2: Voice Trigger

| Step | Action | Expected Result |
|------|--------|-----------------|
| 1 | Nói "làm bài tập" hoặc "kiểm tra" | Quiz Mode được kích hoạt |
| 2 | Chờ câu hỏi hiển thị | Hiển thị câu hỏi đầu tiên |

#### Test Case 3: Voice Answer

| Step | Action | Expected Result |
|------|--------|-----------------|
| 1 | Trong Quiz Mode, nói "đáp án B" | Nhận diện đáp án B |
| 2 | Hoặc nói "chọn A" / "câu C" / "D" | Nhận diện tương ứng |

#### Test Case 4: Results Display

| Step | Action | Expected Result |
|------|--------|-----------------|
| 1 | Hoàn thành tất cả câu hỏi | Hiển thị kết quả |
| 2 | Xem summary | "KẾT QUẢ: X/5 câu đúng" |
| 3 | Nếu có sai | Liệt kê các câu sai + đáp án đúng |
| 4 | Chờ 5s | Tự động quay về standby |

### 7.3. Monitor Log Output

```
I (12345) QuizManager: Starting quiz from: /sdcard/quiz/sample_quiz.txt
I (12346) QuizManager: Parsed quiz: "Bài tập Toán Lớp 1" with 5 questions
I (12347) QuizManager: State: LOADING -> QUESTION_DISPLAY
I (12350) QuizUI: Showing question 1: Một cộng một bằng bao nhiêu?
I (15000) QuizUI: Button clicked: B
I (15001) QuizManager: Answer submitted: B, Correct: B, Result: CORRECT
I (16500) QuizManager: Moving to next question (2/5)
...
I (45000) QuizManager: Quiz complete! Score: 4/5
I (45001) QuizManager: Generating result summary
```

### 7.4. Kiểm Tra Memory Leak

Trong serial monitor, xem heap stats mỗi 10 giây:

```
I (xxxxx) SystemInfo: Heap - Free: 180000, Min: 150000, Largest: 120000
```

**Trước khi vào quiz:** Ghi nhận `Free` heap
**Sau khi thoát quiz:** `Free` heap phải tương đương (± 1KB)

```bash
# Nếu muốn force garbage collection test
# Vào/ra quiz mode 10 lần liên tiếp
# Heap phải stable, không giảm liên tục
```

---

## 8. Xử Lý Lỗi và Debug

### 8.1. Common Errors

| Error | Nguyên nhân | Giải pháp |
|-------|-------------|-----------|
| "Không tìm thấy file quiz" | Không có file trong /sdcard/quiz/ | Copy file vào đúng thư mục |
| "Không thể mở file quiz" | File bị hỏng hoặc encoding sai | Dùng UTF-8, kiểm tra format |
| Stack overflow | Quiz file quá lớn | Giới hạn file < 50 câu |
| LVGL crash | Gọi LVGL từ non-main thread | Dùng Schedule() |

### 8.2. Debug Tags

```cpp
#define TAG "QuizManager"  // Trong quiz_manager.cc
#define TAG "QuizUI"       // Trong quiz_ui.cc

// Xem logs cụ thể
idf.py monitor | grep QuizManager
```

### 8.3. Thêm Debug Log

```cpp
// Verbose logging
ESP_LOGD(TAG, "Parsing line: %s", line_buffer);  // Debug level
ESP_LOGI(TAG, "Question parsed: %s", q.question_text.c_str());  // Info
ESP_LOGW(TAG, "Empty option detected");  // Warning
ESP_LOGE(TAG, "File not found: %s", path.c_str());  // Error
```

---

## 📝 Tác Giả

- **Module**: Documentation Quiz Feature
- **Version**: 1.0.0
- **Date**: 2026-01-19
- **Framework**: ESP-IDF + LVGL

## 📄 License

MIT License - Xem file LICENSE trong project root.
