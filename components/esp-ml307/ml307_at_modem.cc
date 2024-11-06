#include "ml307_at_modem.h"
#include <esp_log.h>
#include <esp_err.h>
#include <sstream>
#include <iomanip>
#include <cstring>

static const char* TAG = "Ml307AtModem";


static bool is_number(const std::string& s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), ::isdigit) && s.length() < 10;
}

Ml307AtModem::Ml307AtModem(int tx_pin, int rx_pin, size_t rx_buffer_size)
    : rx_buffer_size_(rx_buffer_size), uart_num_(DEFAULT_UART_NUM), tx_pin_(tx_pin), rx_pin_(rx_pin), baud_rate_(DEFAULT_BAUD_RATE) {
    event_group_handle_ = xEventGroupCreate();

    uart_config_t uart_config = {};
    uart_config.baud_rate = baud_rate_;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.source_clk = UART_SCLK_DEFAULT;
    
    ESP_ERROR_CHECK(uart_driver_install(uart_num_, rx_buffer_size_ * 2, 0, 100, &event_queue_handle_, 0));
    ESP_ERROR_CHECK(uart_param_config(uart_num_, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(uart_num_, tx_pin_, rx_pin_, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    xTaskCreate([](void* arg) {
        auto ml307_at_modem = (Ml307AtModem*)arg;
        ml307_at_modem->EventTask();
        vTaskDelete(NULL);
    }, "uart_at_modem_event", 4096, this, 5, &event_task_handle_);

    xTaskCreate([](void* arg) {
        auto ml307_at_modem = (Ml307AtModem*)arg;
        ml307_at_modem->ReceiveTask();
        vTaskDelete(NULL);
    }, "uart_at_modem_receive", 4096, this, 5, &receive_task_handle_);
}

Ml307AtModem::~Ml307AtModem() {
    vTaskDelete(event_task_handle_);
    vTaskDelete(receive_task_handle_);
    vEventGroupDelete(event_group_handle_);
    uart_driver_delete(uart_num_);
}

bool Ml307AtModem::DetectBaudRate() {
    // Write and Read AT command to detect the current baud rate
    std::vector<int> baud_rates = {115200, 921600, 460800, 230400, 57600, 38400, 19200, 9600};
    while (true) {
        ESP_LOGI(TAG, "Detecting baud rate...");
        for (int rate : baud_rates) {
            uart_set_baudrate(uart_num_, rate);
            if (Command("AT", 20)) {
                ESP_LOGI(TAG, "Detected baud rate: %d", rate);
                baud_rate_ = rate;
                return true;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    return false;
}

bool Ml307AtModem::SetBaudRate(int new_baud_rate) {
    if (!DetectBaudRate()) {
        ESP_LOGE(TAG, "Failed to detect baud rate");
        return false;
    }
    if (new_baud_rate == baud_rate_) {
        return true;
    }
    // Set new baud rate
    if (Command(std::string("AT+IPR=") + std::to_string(new_baud_rate))) {
        uart_set_baudrate(uart_num_, new_baud_rate);
        baud_rate_ = new_baud_rate;
        ESP_LOGI(TAG, "Set baud rate to %d", new_baud_rate);
        return true;
    }
    ESP_LOGI(TAG, "Failed to set baud rate to %d", new_baud_rate);
    return false;
}

int Ml307AtModem::WaitForNetworkReady() {
    ESP_LOGI(TAG, "Waiting for network ready...");
    Command("AT+CEREG=1", 1000);
    while (!network_ready_) {
        if (pin_ready_ == 2) {
            ESP_LOGE(TAG, "PIN is not ready");
            return -1;
        }
        if (registration_state_ == 3) {
            ESP_LOGI(TAG, "Registration denied");
            return -2;
        }
        Command("AT+MIPCALL?");
        xEventGroupWaitBits(event_group_handle_, AT_EVENT_NETWORK_READY, pdTRUE, pdFALSE, pdMS_TO_TICKS(1000));
    }
    return 0;
}

std::string Ml307AtModem::GetImei() {
    if (Command("AT+CIMI")) {
        return response_;
    }
    return "";
}

std::string Ml307AtModem::GetIccid() {
    if (Command("AT+ICCID")) {
        return iccid_;
    }
    return "";
}

std::string Ml307AtModem::GetModuleName() {
    if (Command("AT+CGMR")) {
        return response_;
    }
    return "";
}

std::string Ml307AtModem::GetCarrierName() {
    if (Command("AT+COPS?")) {
        return carrier_name_;
    }
    return "";
}

int Ml307AtModem::GetCsq() {
    if (Command("AT+CSQ")) {
        return csq_;
    }
    return -1;
}

void Ml307AtModem::SetDebug(bool debug) {
    debug_ = debug;
}

std::list<CommandResponseCallback>::iterator Ml307AtModem::RegisterCommandResponseCallback(CommandResponseCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_ );
    return on_data_received_.insert(on_data_received_.end(), callback);
}

void Ml307AtModem::UnregisterCommandResponseCallback(std::list<CommandResponseCallback>::iterator iterator) {
    std::lock_guard<std::mutex> lock(mutex_);
    on_data_received_.erase(iterator);
}

bool Ml307AtModem::Command(const std::string command, int timeout_ms) {
    std::lock_guard<std::mutex> lock(command_mutex_);
    if (debug_) {
        ESP_LOGI(TAG, ">> %.64s", command.c_str());
    }
    response_.clear();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        last_command_ = command + "\r\n";
        int ret = uart_write_bytes(uart_num_, last_command_.c_str(), last_command_.length());
        if (ret < 0) {
            ESP_LOGE(TAG, "uart_write_bytes failed: %d", ret);
            return false;
        }
    }

    if (timeout_ms > 0) {
        auto bits = xEventGroupWaitBits(event_group_handle_, AT_EVENT_COMMAND_DONE | AT_EVENT_COMMAND_ERROR, pdTRUE, pdFALSE, pdMS_TO_TICKS(timeout_ms));
        if (bits & AT_EVENT_COMMAND_DONE) {
            return true;
        } else if (bits & AT_EVENT_COMMAND_ERROR) {
            ESP_LOGE(TAG, "command error: %s", command.c_str());
            return false;
        }
    }
    return false;
}

void Ml307AtModem::EventTask() {
    uart_event_t event;
    while (true) {
        if (xQueueReceive(event_queue_handle_, &event, portMAX_DELAY) == pdTRUE) {
            switch (event.type)
            {
            case UART_DATA:
                xEventGroupSetBits(event_group_handle_, AT_EVENT_DATA_AVAILABLE);
                break;
            case UART_BREAK:
                ESP_LOGI(TAG, "break");
                break;
            case UART_BUFFER_FULL:
                ESP_LOGE(TAG, "buffer full");
                break;
            case UART_FIFO_OVF:
                ESP_LOGE(TAG, "FIFO overflow");
                NotifyCommandResponse("FIFO_OVERFLOW", {});
                break;
            default:
                ESP_LOGE(TAG, "unknown event type: %d", event.type);
                break;
            }
        }
    }
}

void Ml307AtModem::ReceiveTask() {
    while (true) {
        auto bits = xEventGroupWaitBits(event_group_handle_, AT_EVENT_DATA_AVAILABLE, pdTRUE, pdFALSE, portMAX_DELAY);
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
    }
}

bool Ml307AtModem::ParseResponse() {
    auto end_pos = rx_buffer_.find("\r\n");
    if (end_pos == std::string::npos) {
        return false;
    }

    // Ignore empty lines
    if (end_pos == 0) {
        rx_buffer_.erase(0, 2);
        return true;
    }
    if (debug_) {
        ESP_LOGI(TAG, "<< %.64s", rx_buffer_.substr(0, end_pos).c_str());
    }

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

        NotifyCommandResponse(command, arguments);
        return true;
    } else if (rx_buffer_.size() >= 4 && rx_buffer_[0] == 'O' && rx_buffer_[1] == 'K' && rx_buffer_[2] == '\r' && rx_buffer_[3] == '\n') {
        rx_buffer_.erase(0, 4);
        xEventGroupSetBits(event_group_handle_, AT_EVENT_COMMAND_DONE);
        return true;
    } else if (rx_buffer_.size() >= 1 && rx_buffer_[0] == '>') {
        rx_buffer_.erase(0, 1);
        xEventGroupSetBits(event_group_handle_, AT_EVENT_COMMAND_DONE);
        return true;
    } else if (rx_buffer_.size() >= 7 && rx_buffer_[0] == 'E' && rx_buffer_[1] == 'R' && rx_buffer_[2] == 'R' && rx_buffer_[3] == 'O' && rx_buffer_[4] == 'R' && rx_buffer_[5] == '\r' && rx_buffer_[6] == '\n') {
        rx_buffer_.erase(0, 7);
        xEventGroupSetBits(event_group_handle_, AT_EVENT_COMMAND_ERROR);
        return true;
    } else {
        response_ = rx_buffer_.substr(0, end_pos).c_str();
        rx_buffer_.erase(0, end_pos + 2);
        return true;
    }
    return false;
}

void Ml307AtModem::OnMaterialReady(std::function<void()> callback) {
    on_material_ready_ = callback;
}

void Ml307AtModem::NotifyCommandResponse(const std::string& command, const std::vector<AtArgumentValue>& arguments) {
    if (command == "CME ERROR") {
        xEventGroupSetBits(event_group_handle_, AT_EVENT_COMMAND_ERROR);
        return;
    }
    if (command == "MIPCALL" && arguments.size() >= 3) {
        if (arguments[1].int_value == 1) {
            ip_address_ = arguments[2].string_value;
            network_ready_ = true;
            xEventGroupSetBits(event_group_handle_, AT_EVENT_NETWORK_READY);
        }
    } else if (command == "ICCID" && arguments.size() >= 1) {
        iccid_ = arguments[0].string_value;
    } else if (command == "COPS" && arguments.size() >= 4) {
        carrier_name_ = arguments[2].string_value;
    } else if (command == "CSQ" && arguments.size() >= 1) {
        csq_ = arguments[0].int_value;
    } else if (command == "MATREADY") {
        network_ready_ = false;
        if (on_material_ready_) {
            on_material_ready_();
        }
    } else if (command == "CEREG" && arguments.size() >= 1) {
        if (arguments.size() == 1) {
            registration_state_ = arguments[0].int_value;
        } else {
            registration_state_ = arguments[1].int_value;
        }
    } else if (command == "CPIN" && arguments.size() >= 1) {
        if (arguments[0].string_value == "READY") {
            pin_ready_ = 1;
        } else {
            pin_ready_ = 2;
        }
    }

    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& callback : on_data_received_) {
        callback(command, arguments);
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

void Ml307AtModem::EncodeHexAppend(std::string& dest, const char* data, size_t length) {
    dest.reserve(dest.size() + length * 2);  // 预分配空间
    for (size_t i = 0; i < length; i++) {
        dest.push_back(hex_chars[(data[i] & 0xF0) >> 4]);
        dest.push_back(hex_chars[data[i] & 0x0F]);
    }
}

void Ml307AtModem::DecodeHexAppend(std::string& dest, const char* data, size_t length) {
    dest.reserve(dest.size() + length / 2);  // 预分配空间
    for (size_t i = 0; i < length; i += 2) {
        char byte = (CharToHex(data[i]) << 4) | CharToHex(data[i + 1]);
        dest.push_back(byte);
    }
}

std::string Ml307AtModem::EncodeHex(const std::string& data) {
    std::string encoded;
    EncodeHexAppend(encoded, data.c_str(), data.size());
    return encoded;
}

std::string Ml307AtModem::DecodeHex(const std::string& data) {
    std::string decoded;
    DecodeHexAppend(decoded, data.c_str(), data.size());
    return decoded;
}

void Ml307AtModem::Reset() {
    Command("AT+MREBOOT=0");
}

void Ml307AtModem::ResetConnections() {
    // Reset HTTP instances
    Command("AT+MHTTPDEL=0");
    Command("AT+MHTTPDEL=1");
    Command("AT+MHTTPDEL=2");
    Command("AT+MHTTPDEL=3");
}