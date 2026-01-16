#include "ec11_encoder.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"

static const char* TAG = "EC11_ENCODER";
static const int64_t DEBOUNCE_TIME_US = 1000; // 1ms debounce time

Ec11Encoder::Ec11Encoder(gpio_num_t pin_a, gpio_num_t pin_b)
    : pin_a_(pin_a), pin_b_(pin_b) {
}

Ec11Encoder::~Ec11Encoder() {
    if (task_handle_) {
        vTaskDelete(task_handle_);
    }
    if (xSemaphore_) {
        vSemaphoreDelete(xSemaphore_);
    }
    gpio_isr_handler_remove(pin_a_);
    gpio_isr_handler_remove(pin_b_);
}

void Ec11Encoder::Start() {
    // Configure GPIOs
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << pin_a_) | (1ULL << pin_b_);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    if (!xSemaphore_) {
        xSemaphore_ = xSemaphoreCreateBinary();
    }

    // Initialize last_state_ with current pin levels
    int a_state = gpio_get_level(pin_a_);
    int b_state = gpio_get_level(pin_b_);
    last_state_ = (a_state << 1) | b_state;

    // Install ISR service if not already installed (checked by return value)
    if (gpio_install_isr_service(0) != ESP_OK) {
        ESP_LOGW(TAG, "ISR service might already be installed");
    }
    
    gpio_isr_handler_add(pin_a_, gpio_isr_handler, this);
    gpio_isr_handler_add(pin_b_, gpio_isr_handler, this);

    xTaskCreate(ec11_encoder_task, "ec11_encoder_task", 4096, this, 5, &task_handle_);
    ESP_LOGI(TAG, "Encoder started on GPIO %d and %d", pin_a_, pin_b_);
}

void Ec11Encoder::SetCallback(std::function<void(int)> callback) {
    callback_ = callback;
}

void IRAM_ATTR Ec11Encoder::gpio_isr_handler(void* arg) {
    auto* self = static_cast<Ec11Encoder*>(arg);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    int64_t current_time = esp_timer_get_time();
    if (current_time - self->last_isr_time_ > DEBOUNCE_TIME_US) {
        self->last_isr_time_ = current_time;
        if (self && self->xSemaphore_) {
            xSemaphoreGiveFromISR(self->xSemaphore_, &xHigherPriorityTaskWoken);
        }
    }
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void Ec11Encoder::ec11_encoder_task(void* arg) {
    auto* self = static_cast<Ec11Encoder*>(arg);
    int accumulator = 0;
    
    // Transition table for full quadrature decoding
    static const int8_t TRANSITION_TABLE[16] = {
        0, -1,  1,  0, // 00 -> ...
        1,  0,  0, -1, // 01 -> ...
       -1,  0,  0,  1, // 10 -> ...
        0,  1, -1,  0  // 11 -> ...
    };

    // Initialize last_state_ with current pin levels to prevent startup jump
    int a_state = gpio_get_level(self->pin_a_);
    int b_state = gpio_get_level(self->pin_b_);
    self->last_state_ = (a_state << 1) | b_state;

    while (1) {
        if (xSemaphoreTake(self->xSemaphore_, portMAX_DELAY) == pdTRUE) {
            a_state = gpio_get_level(self->pin_a_);
            b_state = gpio_get_level(self->pin_b_);
            int current_state = (a_state << 1) | b_state;

            int idx = (self->last_state_ << 2) | current_state;
            int direction = TRANSITION_TABLE[idx & 0x0F];

            if (direction != 0) {
                accumulator += direction;
                // 2x resolution: Trigger every 2 steps
                if (abs(accumulator) >= 2) {
                    int report_dir = (accumulator > 0) ? 1 : -1;
                    self->encoder_position_ += report_dir;
                    // ESP_LOGI(TAG, "Rotated, Position: %d", self->encoder_position_);
                    if (self->callback_) {
                        self->callback_(report_dir);
                    }
                    accumulator = 0;
                }
            }
            self->last_state_ = current_state;
        }
    }
}