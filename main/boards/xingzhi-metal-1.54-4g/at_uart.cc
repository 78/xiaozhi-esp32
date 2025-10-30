#include "at_uart.h"
#include <esp_log.h>
#include <esp_err.h>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <sstream>

#define TAG "AtUart_METAL"


// AtUart 构造函数实现
AtUart::AtUart(gpio_num_t tx_pin, gpio_num_t rx_pin, gpio_num_t dtr_pin)
    : tx_pin_(tx_pin), rx_pin_(rx_pin), dtr_pin_(dtr_pin), uart_num_(UART_NUM),
      baud_rate_(115200), initialized_(false),
      event_task_handle_(nullptr), event_queue_handle_(nullptr), event_group_handle_(nullptr) {
}

AtUart::~AtUart() {
    if (event_task_handle_) {
        vTaskDelete(event_task_handle_);
    }
    if (event_group_handle_) {
        vEventGroupDelete(event_group_handle_);
    }
    if (initialized_) {
        uart_driver_delete(uart_num_);
    }
}

void AtUart::Initialize() {
    if (initialized_) {
        return;
    }
    
    event_group_handle_ = xEventGroupCreate();
    if (!event_group_handle_) {
        ESP_LOGE(TAG, "创建事件组失败");
        return;
    }

    uart_config_t uart_config = {};
    uart_config.baud_rate = baud_rate_;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.source_clk = UART_SCLK_DEFAULT;
    
    ESP_ERROR_CHECK(uart_driver_install(uart_num_, 8192, 0, 100, &event_queue_handle_, ESP_INTR_FLAG_IRAM));
    ESP_ERROR_CHECK(uart_param_config(uart_num_, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(uart_num_, tx_pin_, rx_pin_, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    
    if (dtr_pin_ != GPIO_NUM_NC) {
        gpio_config_t config = {};
        config.pin_bit_mask = (1ULL << dtr_pin_);
        config.mode = GPIO_MODE_OUTPUT;
        config.pull_up_en = GPIO_PULLUP_DISABLE;
        config.pull_down_en = GPIO_PULLDOWN_DISABLE;
        config.intr_type = GPIO_INTR_DISABLE;
        gpio_config(&config);
        gpio_set_level(dtr_pin_, 0);
    }

    xTaskCreatePinnedToCore([](void* arg) {
        auto ml307_at_modem = (AtUart*)arg;
        ml307_at_modem->EventTask();
        vTaskDelete(NULL);
    }, "modem_event", 2048, this, configMAX_PRIORITIES - 1, &event_task_handle_, 0);

    xTaskCreatePinnedToCore([](void* arg) {
        auto ml307_at_modem = (AtUart*)arg;
        ml307_at_modem->ReceiveTask();
        vTaskDelete(NULL);
    }, "modem_receive", 2048 * 3, this, configMAX_PRIORITIES - 2, &receive_task_handle_, 0);
    initialized_ = true;
}

void AtUart::EventTask() {
    uart_event_t event;
    while (true) {
        if (xQueueReceive(event_queue_handle_, &event, portMAX_DELAY) == pdTRUE) {
            switch (event.type)
            {
            case UART_DATA:
                xEventGroupSetBits(event_group_handle_, AT_EVENT_DATA_AVAILABLE);
                break;
            case UART_BREAK:
                xEventGroupSetBits(event_group_handle_, AT_EVENT_BREAK);
                break;
            case UART_BUFFER_FULL:
                xEventGroupSetBits(event_group_handle_, AT_EVENT_BUFFER_FULL);
                break;
            case UART_FIFO_OVF:
                xEventGroupSetBits(event_group_handle_, AT_EVENT_FIFO_OVF);
                break;
            default:
                ESP_LOGE(TAG, "unknown event type: %d", event.type);
                break;
            }
        }
    }
}

void AtUart::ReceiveTask() {
    while (true) {
        auto bits = xEventGroupWaitBits(event_group_handle_, AT_EVENT_DATA_AVAILABLE | AT_EVENT_FIFO_OVF | AT_EVENT_BUFFER_FULL | AT_EVENT_BREAK, pdTRUE, pdFALSE, portMAX_DELAY);
        if (bits & AT_EVENT_DATA_AVAILABLE) {
            size_t available;
            uart_get_buffered_data_len(uart_num_, &available);
            if (available > 0) {
                // Extend rx_buffer_ and read into buffer
                rx_buffer_.resize(rx_buffer_.size() + available);
                char* rx_buffer_ptr = &rx_buffer_[rx_buffer_.size() - available];
                uart_read_bytes(uart_num_, rx_buffer_ptr, available, portMAX_DELAY);
                while (ParseResponse()) {}
            }
        }
        if (bits & AT_EVENT_FIFO_OVF) {
            ESP_LOGE(TAG, "FIFO overflow");
            HandleUrc("FIFO_OVERFLOW", {});
        }
        if (bits & AT_EVENT_BREAK) {
            ESP_LOGE(TAG, "Break");
        }
        if (bits & AT_EVENT_BUFFER_FULL) {
            ESP_LOGE(TAG, "Buffer full");
        }
    }
}

static bool is_number(const std::string& s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), ::isdigit) && s.length() < 10;
}

bool AtUart::ParseResponse() {
    if (wait_for_response_ && rx_buffer_[0] == '>') {
        rx_buffer_.erase(0, 1);
        xEventGroupSetBits(event_group_handle_, AT_EVENT_COMMAND_DONE);
        return true;
    }

    auto end_pos = rx_buffer_.find("\r\n");
    if (end_pos == std::string::npos) {
        // FIXME: for +MHTTPURC: "ind", missing newline
        if (rx_buffer_.size() >= 16 && memcmp(rx_buffer_.c_str(), "+MHTTPURC: \"ind\"", 16) == 0) {
            // Find the end of this line and add \r\n if missing
            auto next_plus = rx_buffer_.find("+", 1);
            if (next_plus != std::string::npos) {
                // Insert \r\n before the next + command
                rx_buffer_.insert(next_plus, "\r\n");
            } else {
                // Append \r\n at the end
                rx_buffer_.append("\r\n");
            }
            end_pos = rx_buffer_.find("\r\n");
        } else {
            return false;
        }
    }

    // Ignore empty lines
    if (end_pos == 0) {
        rx_buffer_.erase(0, 2);
        return true;
    }

    ESP_LOGD(TAG, "<< %.64s (%u bytes)", rx_buffer_.substr(0, end_pos).c_str(), end_pos);
    // print last 64 bytes before end_pos if available
    // if (end_pos > 64) {
    //     ESP_LOGI(TAG, "<< LAST: %.64s", rx_buffer_.c_str() + end_pos - 64);
    // }

    // Parse "+CME ERROR: 123,456,789"
    if (rx_buffer_[0] == '+') {
        std::string command, values;
        auto pos = rx_buffer_.find(": ");
        if (pos == std::string::npos || pos > end_pos) {
            command = rx_buffer_.substr(1, end_pos - 1);
        } else {
            command = rx_buffer_.substr(1, pos - 1);
            values = rx_buffer_.substr(pos + 2, end_pos - pos - 2);
        }
        rx_buffer_.erase(0, end_pos + 2);

        // Parse "string", int, int, ... into AtArgumentValue
        std::vector<AtArgumentValue> arguments;
        std::istringstream iss(values);
        std::string item;
        while (std::getline(iss, item, ',')) {
            AtArgumentValue argument;
            if (item.front() == '"') {
                argument.type = AtArgumentValue::Type::String;
                argument.string_value = item.substr(1, item.size() - 2);
            } else if (item.find(".") != std::string::npos) {
                argument.type = AtArgumentValue::Type::Double;
                argument.double_value = std::stod(item);
            } else if (is_number(item)) {
                argument.type = AtArgumentValue::Type::Int;
                argument.int_value = std::stoi(item);
                argument.string_value = std::move(item);
            } else {
                argument.type = AtArgumentValue::Type::String;
                argument.string_value = std::move(item);
            }
            arguments.push_back(argument);
        }

        HandleUrc(command, arguments);
        return true;
    } else if (rx_buffer_.size() >= 4 && rx_buffer_[0] == 'O' && rx_buffer_[1] == 'K' && rx_buffer_[2] == '\r' && rx_buffer_[3] == '\n') {
        rx_buffer_.erase(0, 4);
        xEventGroupSetBits(event_group_handle_, AT_EVENT_COMMAND_DONE);
        return true;
    } else if (rx_buffer_.size() >= 7 && rx_buffer_[0] == 'E' && rx_buffer_[1] == 'R' && rx_buffer_[2] == 'R' && rx_buffer_[3] == 'O' && rx_buffer_[4] == 'R' && rx_buffer_[5] == '\r' && rx_buffer_[6] == '\n') {
        rx_buffer_.erase(0, 7);
        xEventGroupSetBits(event_group_handle_, AT_EVENT_COMMAND_ERROR);
        return true;
    } else {
        std::lock_guard<std::mutex> lock(mutex_);
        response_ = rx_buffer_.substr(0, end_pos);
        rx_buffer_.erase(0, end_pos + 2);
        return true;
    }
    return false;
}

void AtUart::HandleCommand(const char* command) {
    // 这个函数现在主要用于向后兼容，大部分处理逻辑已经移到 ParseLine 中
    if (wait_for_response_) {
        response_.append(command);
        response_.append("\r\n");
    }
}

void AtUart::HandleUrc(const std::string& command, const std::vector<AtArgumentValue>& arguments) {
    if (command == "CME ERROR") {
        cme_error_code_ = arguments[0].int_value;
        xEventGroupSetBits(event_group_handle_, AT_EVENT_COMMAND_ERROR);
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& callback : urc_callbacks_) {
        callback(command, arguments);
    }
}

bool AtUart::DetectBaudRate() {
    int baud_rates[] = {115200, 921600, 460800, 230400, 57600, 38400, 19200, 9600};
    while (true) {
        ESP_LOGI(TAG, "Detecting baud rate...");
        for (size_t i = 0; i < sizeof(baud_rates) / sizeof(baud_rates[0]); i++) {
            int rate = baud_rates[i];
            uart_set_baudrate(uart_num_, rate);
            if (SendCommand("AT", 20)) {
                ESP_LOGI(TAG, "Detected baud rate: %d", rate);
                baud_rate_ = rate;
                return true;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    return false;
}

bool AtUart::SetBaudRate(int new_baud_rate) {
    if (!DetectBaudRate()) {
        ESP_LOGE(TAG, "Failed to detect baud rate");
        return false;
    }
    if (new_baud_rate == baud_rate_) {
        return true;
    }
    // Set new baud rate
    if (!SendCommand(std::string("AT+IPR=") + std::to_string(new_baud_rate))) {
        ESP_LOGI(TAG, "Failed to set baud rate to %d", new_baud_rate);
        return false;
    }
    uart_set_baudrate(uart_num_, new_baud_rate);
    baud_rate_ = new_baud_rate;
    ESP_LOGI(TAG, "Set baud rate to %d", new_baud_rate);
    return true;
}

bool AtUart::SendData(const char* data, size_t length) {
    if (!initialized_) {
        ESP_LOGE(TAG, "UART未初始化");
        return false;
    }
    
    int ret = uart_write_bytes(uart_num_, data, length);
    if (ret < 0) {
        ESP_LOGE(TAG, "uart_write_bytes failed: %d", ret);
        return false;
    }
    return true;
}

bool AtUart::SendCommandWithData(const std::string& command, size_t timeout_ms, bool add_crlf, const char* data, size_t data_length) {
    std::lock_guard<std::mutex> lock(command_mutex_);
    ESP_LOGD(TAG, ">> %.64s (%u bytes)", command.data(), command.length());

    xEventGroupClearBits(event_group_handle_, AT_EVENT_COMMAND_DONE | AT_EVENT_COMMAND_ERROR);
    wait_for_response_ = true;
    cme_error_code_ = 0;
    response_.clear();

    if (add_crlf) {
        if (!SendData((command + "\r\n").data(), command.length() + 2)) {
            return false;
        }
    } else {
        if (!SendData(command.data(), command.length())) {
            return false;
        }
    }

    if (timeout_ms > 0) {
        auto bits = xEventGroupWaitBits(event_group_handle_, AT_EVENT_COMMAND_DONE | AT_EVENT_COMMAND_ERROR, pdTRUE, pdFALSE, pdMS_TO_TICKS(timeout_ms));
        wait_for_response_ = false;
        if (!(bits & AT_EVENT_COMMAND_DONE)) {
            return false;
        }
    } else {
        wait_for_response_ = false;
    }

    if (0)
    {
    if (data && data_length > 0) {
        wait_for_response_ = true;
        if (!SendData(data, data_length)) {
            return false;
        }
        auto bits = xEventGroupWaitBits(event_group_handle_, AT_EVENT_COMMAND_DONE | AT_EVENT_COMMAND_ERROR, pdTRUE, pdFALSE, pdMS_TO_TICKS(timeout_ms));
        wait_for_response_ = false;
        if (!(bits & AT_EVENT_COMMAND_DONE)) {
            return false;
        }
    }
    }

    // 与组件不同的地方
    if (data && data_length > 0) {
        wait_for_response_ = true;
        std::string data_with_crlf(data, data_length);
        data_with_crlf += "\r\n"; 
        if (!SendData(data_with_crlf.data(), data_with_crlf.size())) {
            return false;
        }
        auto bits = xEventGroupWaitBits(event_group_handle_, AT_EVENT_COMMAND_DONE | AT_EVENT_COMMAND_ERROR, pdTRUE, pdFALSE, pdMS_TO_TICKS(timeout_ms));
        wait_for_response_ = false;
        if (!(bits & AT_EVENT_COMMAND_DONE)) {
            return false;
        }
    }
    return true;
}

bool AtUart::SendCommand(const std::string& command, size_t timeout_ms, bool add_crlf) {
    return SendCommandWithData(command, timeout_ms, add_crlf, nullptr, 0);
}

std::list<UrcCallback>::iterator AtUart::RegisterUrcCallback(UrcCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    return urc_callbacks_.insert(urc_callbacks_.end(), callback);
}

void AtUart::UnregisterUrcCallback(std::list<UrcCallback>::iterator iterator) {
    std::lock_guard<std::mutex> lock(mutex_);
    urc_callbacks_.erase(iterator);
}

void AtUart::SetDtrPin(bool high) {
    if (dtr_pin_ != GPIO_NUM_NC) {
        ESP_LOGD(TAG, "Set DTR pin %d to %d", dtr_pin_, high ? 1 : 0);
        gpio_set_level(dtr_pin_, high ? 1 : 0);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static const char hex_chars[] = "0123456789ABCDEF";
// 辅助函数，将单个十六进制字符转换为对应的数值
inline uint8_t CharToHex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;  // 对于无效输入，返回0
}

void AtUart::EncodeHexAppend(std::string& dest, const char* data, size_t length) {
    dest.reserve(dest.size() + length * 2 + 4);  // 预分配空间，多分配4个字节用于\r\n\0
    for (size_t i = 0; i < length; i++) {
        dest.push_back(hex_chars[(data[i] & 0xF0) >> 4]);
        dest.push_back(hex_chars[data[i] & 0x0F]);
    }
}

void AtUart::DecodeHexAppend(std::string& dest, const char* data, size_t length) {
    dest.reserve(dest.size() + length / 2 + 4);  // 预分配空间，多分配4个字节用于\r\n\0
    for (size_t i = 0; i < length; i += 2) {
        char byte = (CharToHex(data[i]) << 4) | CharToHex(data[i + 1]);
        dest.push_back(byte);
    }
}

std::string AtUart::EncodeHex(const std::string& data) {
    std::string encoded;
    EncodeHexAppend(encoded, data.c_str(), data.size());
    return encoded;
}

std::string AtUart::DecodeHex(const std::string& data) {
    std::string decoded;
    DecodeHexAppend(decoded, data.c_str(), data.size());
    return decoded;
}