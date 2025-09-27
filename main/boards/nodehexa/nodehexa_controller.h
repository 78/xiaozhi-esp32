#ifndef _NODEHEXA_CONTROLLER_H_
#define _NODEHEXA_CONTROLLER_H_

#include <cJSON.h>
#include <driver/uart.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <string>

#define TAG "NodeHexaController"

class NodeHexaController {
public:
    NodeHexaController();
    ~NodeHexaController();

    void Initialize();
    cJSON* SendCommand(const std::string& command);

private:
    bool SendUartCommand(const std::string& command);
    std::string ReceiveUartResponse();
    int16_t CommandToMovementMode(const std::string& command);

    static constexpr int UART_TIMEOUT_MS = 1000;
    static constexpr int UART_BUFFER_SIZE = 256;
};

#endif // _NODEHEXA_CONTROLLER_H_ 