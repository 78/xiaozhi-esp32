#include "nodehexa_controller.h"

#include <cstring>

NodeHexaController::NodeHexaController() {
    ESP_LOGI(TAG, "NodeHexaController 构造函数");
}

NodeHexaController::~NodeHexaController() {
    ESP_LOGI(TAG, "NodeHexaController 析构函数");
}

void NodeHexaController::Initialize() {
    ESP_LOGI(TAG, "初始化 NodeHexaController");
}

cJSON* NodeHexaController::SendCommand(const std::string& command) {
    ESP_LOGI(TAG, "发送命令: %s", command.c_str());
    
    cJSON* result = cJSON_CreateObject();
    
    // 将命令转换为运动模式
    int16_t movement_mode = CommandToMovementMode(command);
    
    if (movement_mode >= 0) {
        // 构造JSON命令
        cJSON* json_cmd = cJSON_CreateObject();
        cJSON_AddNumberToObject(json_cmd, "movementMode", movement_mode);
        
        char* json_str = cJSON_PrintUnformatted(json_cmd);
        std::string uart_command = "$";  // 添加起始标志
        uart_command += json_str;
        uart_command += "\n";  // 添加换行符作为终止标志
        cJSON_free(json_str);
        cJSON_Delete(json_cmd);
        
        // 发送UART命令
        if (SendUartCommand(uart_command)) {
            // 接收响应
            std::string response = ReceiveUartResponse();
            
            cJSON_AddStringToObject(result, "status", "success");
            cJSON_AddStringToObject(result, "command", command.c_str());
            cJSON_AddNumberToObject(result, "movementMode", movement_mode);
            cJSON_AddStringToObject(result, "response", response.c_str());
            
            ESP_LOGI(TAG, "命令执行成功: %s -> 模式 %d", command.c_str(), movement_mode);
        } else {
            cJSON_AddStringToObject(result, "status", "error");
            cJSON_AddStringToObject(result, "message", "UART发送失败");
            ESP_LOGE(TAG, "UART发送失败: %s", command.c_str());
        }
    } else {
        cJSON_AddStringToObject(result, "status", "error");
        cJSON_AddStringToObject(result, "message", "未知命令");
        ESP_LOGE(TAG, "未知命令: %s", command.c_str());
    }
    
    return result;
}

bool NodeHexaController::SendUartCommand(const std::string& command) {
    int written = uart_write_bytes(UART_NUM_1, command.c_str(), command.length());
    if (written == command.length()) {
        ESP_LOGD(TAG, "UART发送成功: %s", command.c_str());
        return true;
    } else {
        ESP_LOGE(TAG, "UART发送失败: 期望 %zu 字节, 实际发送 %d 字节", command.length(), written);
        return false;
    }
}

std::string NodeHexaController::ReceiveUartResponse() {
    uint8_t buffer[UART_BUFFER_SIZE];
    int length = uart_read_bytes(UART_NUM_1, buffer, UART_BUFFER_SIZE - 1, pdMS_TO_TICKS(UART_TIMEOUT_MS));
    
    if (length > 0) {
        buffer[length] = '\0';
        std::string response(reinterpret_cast<char*>(buffer), length);
        ESP_LOGD(TAG, "UART接收响应: %s", response.c_str());
        return response;
    } else {
        ESP_LOGW(TAG, "UART接收超时或无数据");
        return "";
    }
}

int16_t NodeHexaController::CommandToMovementMode(const std::string& command) {
    // 根据命令字符串返回对应的运动模式（位移后的值）
    if (command == "FORWARD") {
        return 1 << 1;  // MOVEMENT_FORWARD: 1 << 1 = 2
    } else if (command == "FORWARDFAST") {
        return 1 << 2;  // MOVEMENT_FORWARDFAST: 1 << 2 = 4
    } else if (command == "BACKWARD") {
        return 1 << 3;  // MOVEMENT_BACKWARD: 1 << 3 = 8
    } else if (command == "TURNLEFT") {
        return 1 << 4;  // MOVEMENT_TURNLEFT: 1 << 4 = 16
    } else if (command == "TURNRIGHT") {
        return 1 << 5;  // MOVEMENT_TURNRIGHT: 1 << 5 = 32
    } else if (command == "SHIFTLEFT") {
        return 1 << 6;  // MOVEMENT_SHIFTLEFT: 1 << 6 = 64
    } else if (command == "SHIFTRIGHT") {
        return 1 << 7;  // MOVEMENT_SHIFTRIGHT: 1 << 7 = 128
    } else if (command == "CLIMB") {
        return 1 << 8;  // MOVEMENT_CLIMB: 1 << 8 = 256
    } else if (command == "ROTATEX") {
        return 1 << 9;  // MOVEMENT_ROTATEX: 1 << 9 = 512
    } else if (command == "ROTATEY") {
        return 1 << 10; // MOVEMENT_ROTATEY: 1 << 10 = 1024
    } else if (command == "ROTATEZ") {
        return 1 << 11; // MOVEMENT_ROTATEZ: 1 << 11 = 2048
    } else if (command == "TWIST") {
        return 1 << 12; // MOVEMENT_TWIST: 1 << 12 = 4096
    } else if (command == "STANDBY") {
        return 1 << 0;  // MOVEMENT_STANDBY: 1 << 0 = 1
    } else {
        return 1 << 0; // 未知命令则待命: 1 << 0 = 1
    }
} 