#ifndef _ML307_AT_MODEM_H_
#define _ML307_AT_MODEM_H_

#include <cstddef>
#include <string>
#include <vector>
#include <list>
#include <functional>
#include <mutex>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/event_groups.h>
#include <driver/gpio.h>
#include <driver/uart.h>

#define AT_EVENT_DATA_AVAILABLE BIT1
#define AT_EVENT_COMMAND_DONE BIT2
#define AT_EVENT_COMMAND_ERROR BIT3
#define AT_EVENT_NETWORK_READY BIT4

#define DEFAULT_COMMAND_TIMEOUT 10000
#define DEFAULT_BAUD_RATE 115200
#define DEFAULT_UART_NUM UART_NUM_1

struct AtArgumentValue {
    enum class Type {
        String,
        Int,
        Double
    };
    Type type;
    std::string string_value;
    int int_value;
    double double_value;
};

typedef std::function<void(const std::string& command, const std::vector<AtArgumentValue>& arguments)> CommandResponseCallback;

class Ml307AtModem {
public:
    Ml307AtModem(int tx_pin = GPIO_NUM_17, int rx_pin = GPIO_NUM_18, size_t rx_buffer_size = 2048);
    ~Ml307AtModem();

    std::string EncodeHex(const std::string& data);
    std::string DecodeHex(const std::string& data);
    void EncodeHexAppend(std::string& dest, const char* data, size_t length);
    void DecodeHexAppend(std::string& dest, const char* data, size_t length);

    bool Command(const std::string command, int timeout_ms = DEFAULT_COMMAND_TIMEOUT);
    std::list<CommandResponseCallback>::iterator RegisterCommandResponseCallback(CommandResponseCallback callback);
    void UnregisterCommandResponseCallback(std::list<CommandResponseCallback>::iterator iterator);

    void OnMaterialReady(std::function<void()> callback);
    void Reset();
    void ResetConnections();
    void SetDebug(bool debug);
    bool SetBaudRate(int new_baud_rate);
    int WaitForNetworkReady();

    std::string GetImei();
    std::string GetIccid();
    std::string GetModuleName();
    std::string GetCarrierName();
    int GetCsq();

    const std::string& ip_address() const { return ip_address_; }
    bool network_ready() const { return network_ready_; }
    int registration_state() const { return registration_state_; }
    int pin_ready() const { return pin_ready_; }
private:
    std::mutex mutex_;
    std::mutex command_mutex_;
    bool debug_ = false;
    bool network_ready_ = false;
    std::string ip_address_;
    std::string iccid_;
    std::string carrier_name_;
    int csq_ = -1;
    int registration_state_ = 0;
    int pin_ready_ = 0;

    std::string rx_buffer_;
    size_t rx_buffer_size_;
    uart_port_t uart_num_;
    int tx_pin_;
    int rx_pin_;
    int baud_rate_;
    TaskHandle_t event_task_handle_ = nullptr;
    TaskHandle_t receive_task_handle_ = nullptr;
    QueueHandle_t event_queue_handle_ = nullptr;
    EventGroupHandle_t event_group_handle_ = nullptr;
    std::string last_command_;
    std::string response_;

    void EventTask();
    void ReceiveTask();
    bool ParseResponse();
    bool DetectBaudRate();
    void NotifyCommandResponse(const std::string& command, const std::vector<AtArgumentValue>& arguments);

    std::list<CommandResponseCallback> on_data_received_;
    std::function<void()> on_material_ready_;
};


#endif // _ML307_AT_MODEM_H_