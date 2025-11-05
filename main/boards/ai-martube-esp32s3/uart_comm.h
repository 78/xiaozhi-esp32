#pragma once

#include <driver/uart.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <functional>
#include <mutex>
#include <string>

class UartComm {
public:
    UartComm(uart_port_t port, gpio_num_t tx_pin, gpio_num_t rx_pin, int baud_rate = 115200, int rx_buf_size = 2048);
    ~UartComm();

    bool Begin();
    void Stop();
    bool IsReady() const;

    // 发送接口
    bool Send(const uint8_t* data, size_t len);
    bool Send(const std::string& s);

    // 设置解析器回调：实时接收数据后调用
    void SetParser(std::function<void(const uint8_t*, size_t)> parser);

    // 运行时配置
    bool SetBaudRate(int baud_rate);

private:
    uart_port_t port_;
    gpio_num_t tx_pin_;
    gpio_num_t rx_pin_;
    int baud_rate_;
    int rx_buf_size_;
    bool ready_ = false;

    TaskHandle_t rx_task_ = nullptr;
    QueueHandle_t uart_queue_ = nullptr;
    std::mutex tx_mutex_;
    std::function<void(const uint8_t*, size_t)> parser_cb_ = nullptr;

    static void RxTaskEntry(void* arg);
    void RxTaskLoop();
    void HandleEvent(const uart_event_t& event);
    void ParseData(const uint8_t* data, size_t len);
};