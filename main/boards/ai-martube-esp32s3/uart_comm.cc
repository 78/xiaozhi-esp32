#include "uart_comm.h"
#include <esp_log.h>

static const char* TAG = "UartComm";

UartComm::UartComm(uart_port_t port, gpio_num_t tx_pin, gpio_num_t rx_pin, int baud_rate, int rx_buf_size)
    : port_(port), tx_pin_(tx_pin), rx_pin_(rx_pin), baud_rate_(baud_rate), rx_buf_size_(rx_buf_size) {}

UartComm::~UartComm() {
    Stop();
}

bool UartComm::Begin() {
    uart_config_t uart_config = {};
    uart_config.baud_rate = baud_rate_;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = UART_SCLK_DEFAULT;
    // 推荐顺序：先配置参数与引脚，再安装驱动；同时为 TX 分配缓冲区
    ESP_ERROR_CHECK(uart_param_config(port_, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(port_, tx_pin_, rx_pin_, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(port_, rx_buf_size_, 1024 /* tx buffer size */, 20, &uart_queue_, 0));
    // 调整 RX 超时以聚合短包，避免每字节触发事件
    ESP_ERROR_CHECK(uart_set_rx_timeout(port_, 10)); // 约 10 字符时间
    // 可选：使用默认满阈值，避免设置为1导致每字节事件
    // uart_set_rx_full_threshold(port_, 32);

    if (rx_task_ == nullptr) {
        xTaskCreate(RxTaskEntry, "uart_rx_task", 4096, this, 5, &rx_task_);
    }

    ready_ = true;
    ESP_LOGI(TAG, "UART begin: port=%d tx=%d rx=%d baud=%d", (int)port_, (int)tx_pin_, (int)rx_pin_, baud_rate_);
    return true;
}

void UartComm::Stop() {
    if (rx_task_) {
        vTaskDelete(rx_task_);
        rx_task_ = nullptr;
    }
    if (port_ >= UART_NUM_0 && port_ < UART_NUM_MAX) {
        uart_driver_delete(port_);
    }
    ready_ = false;
    uart_queue_ = nullptr;
}

bool UartComm::IsReady() const {
    return ready_;
}

bool UartComm::Send(const uint8_t* data, size_t len) {
    if (!ready_ || data == nullptr || len == 0) return false;
    std::lock_guard<std::mutex> lock(tx_mutex_);
    int written = uart_write_bytes(port_, (const char*)data, len);
    ESP_LOGI(TAG, "UART TX write=%d len=%d", written, (int)len); 
    uart_wait_tx_done(port_, pdMS_TO_TICKS(50));
    return written == (int)len;
}

bool UartComm::Send(const std::string& s) {
    return Send(reinterpret_cast<const uint8_t*>(s.data()), s.size());
}

void UartComm::SetParser(std::function<void(const uint8_t*, size_t)> parser) {
    parser_cb_ = std::move(parser);
}

bool UartComm::SetBaudRate(int baud_rate) {
    baud_rate_ = baud_rate;
    if (!ready_) return true;
    return uart_set_baudrate(port_, baud_rate_) == ESP_OK;
}

void UartComm::RxTaskEntry(void* arg) {
    static_cast<UartComm*>(arg)->RxTaskLoop();
}

void UartComm::RxTaskLoop() {
    uart_event_t event;
    while (true) {
        if (xQueueReceive(uart_queue_, &event, portMAX_DELAY)) {
            HandleEvent(event);
        }
    }
}

void UartComm::HandleEvent(const uart_event_t& event) {
    switch (event.type) {
        case UART_DATA: {
            size_t available = 0;
            uart_get_buffered_data_len(port_, &available);
            size_t to_read = event.size + available;
            ESP_LOGD(TAG, "UART_DATA event, event.size=%d, available=%d, to_read=%d",
                     (int)event.size, (int)available, (int)to_read);
            if (to_read == 0) break;
            uint8_t* buf = (uint8_t*)malloc(to_read);
            if (!buf) {
                ESP_LOGE(TAG, "malloc failed");
                break;
            }
            int len = uart_read_bytes(port_, buf, to_read, pdMS_TO_TICKS(20));
            if (len > 0) {
                ParseData(buf, (size_t)len);
            }
            free(buf);
            break;
        }
        case UART_FIFO_OVF:
        case UART_BUFFER_FULL:
            ESP_LOGW(TAG, "UART overflow or buffer full, flushing input");
            uart_flush_input(port_);
            xQueueReset(uart_queue_);
            break;
        case UART_BREAK:
        case UART_PARITY_ERR:
        case UART_FRAME_ERR:
            ESP_LOGW(TAG, "UART error event type=%d", event.type);
            break;
        default:
            // ignore other events
            break;
    }
}

void UartComm::ParseData(const uint8_t* data, size_t len) {
    // 预留解析接口：当前直接透传原始数据；后续可替换为协议解析
    if (parser_cb_) {
        parser_cb_(data, len);
    }
}