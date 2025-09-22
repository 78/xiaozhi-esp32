#include <cJSON.h>
#include <driver/uart.h>
#include <esp_log.h>
#include <wifi_station.h>

#include "application.h"
#include "audio/codecs/no_audio_codec.h"
#include "button.h"
#include "config.h"
#include "led/single_led.h"
#include "mcp_server.h"
#include "nodehexa_controller.h"
#include "system_reset.h"
#include "wifi_board.h"

#define TAG "NodeHexa"

extern void InitializeNodeHexaController();

class NodeHexaBoard : public WifiBoard {
private:
    Button boot_button_;
    NodeHexaController* nodehexa_controller_;

    void InitializeUart() {
        // 初始化UART1用于与六足机器人通信 (ESP32-S3默认引脚: GPIO17-TX, GPIO18-RX)
        uart_config_t uart_config = {
            .baud_rate = 115200,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .source_clk = UART_SCLK_APB,
        };
        ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, 1024, 1024, 0, NULL, 0));
        ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart_config));
        ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, GPIO_NUM_17, GPIO_NUM_18, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting &&
                !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState();
        });
    }

    void InitializeNodeHexaController() {
        nodehexa_controller_ = new NodeHexaController();
        nodehexa_controller_->Initialize();
    }

public:
    NodeHexaBoard() : boot_button_(BOOT_BUTTON_GPIO) {
        ESP_LOGI(TAG, "初始化 NodeHexa 六足机器人主板");

        InitializeUart();
        InitializeButtons();
        InitializeNodeHexaController();
        InitializeTools();
    }

    ~NodeHexaBoard() {
        if (nodehexa_controller_) {
            delete nodehexa_controller_;
        }
    }

    std::string GetBoardType() override {
        return "nodehexa";
    }

    AudioCodec* GetAudioCodec() override {
        // 右声道配置
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                               AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK,
                                               AUDIO_I2S_SPK_GPIO_DOUT, I2S_STD_SLOT_RIGHT,
                                               AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS,
                                               AUDIO_I2S_MIC_GPIO_DIN, I2S_STD_SLOT_LEFT);
        
        // 双声道配置（如果需要同时输出左右声道）
        // static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
        //                                        AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK,
        //                                        AUDIO_I2S_SPK_GPIO_DOUT, I2S_STD_SLOT_BOTH,
        //                                        AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS,
        //                                        AUDIO_I2S_MIC_GPIO_DIN, I2S_STD_SLOT_LEFT);
        
        return &audio_codec;
    }

    void InitializeTools() {
        auto& mcp = McpServer::GetInstance();
        
        // 机器人待机状态
        mcp.AddTool("self.robot.standby", "机器人待机状态。通常在命令停止运动时调用。", PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
            cJSON* result = nodehexa_controller_->SendCommand("STANDBY");
            bool success = (result != nullptr && cJSON_HasObjectItem(result, "status") && 
                           strcmp(cJSON_GetObjectItem(result, "status")->valuestring, "success") == 0);
            cJSON_Delete(result);
            return success;
        });
        
        // 机器人位置控制
        mcp.AddTool("self.robot.position_control", "机器人的位置控制。机器人可以做以下位置控制动作：\n"
            "forward: 前进\nbackward: 后退\nturn_left: 左转\nturn_right: 右转\nshift_left: 左移\nshift_right: 右移\nforward_fast: 快速前进\nclimb: 攀爬", 
            PropertyList({
                Property("action", kPropertyTypeString),
            }), [this](const PropertyList& properties) -> ReturnValue {
                const std::string& action = properties["action"].value<std::string>();
                std::string command;
                
                if (action == "forward") {
                    command = "FORWARD";
                } else if (action == "backward") {
                    command = "BACKWARD";
                } else if (action == "turn_left") {
                    command = "TURNLEFT";
                } else if (action == "turn_right") {
                    command = "TURNRIGHT";
                } else if (action == "shift_left") {
                    command = "SHIFTLEFT";
                } else if (action == "shift_right") {
                    command = "SHIFTRIGHT";
                } else if (action == "forward_fast") {
                    command = "FORWARDFAST";
                } else if (action == "climb") {
                    command = "CLIMB";
                } else {
                    return false;
                }
                
                cJSON* result = nodehexa_controller_->SendCommand(command.c_str());
                bool success = (result != nullptr && cJSON_HasObjectItem(result, "status") && 
                               strcmp(cJSON_GetObjectItem(result, "status")->valuestring, "success") == 0);
                cJSON_Delete(result);
                return success;
            });
        
        // 机器人姿态控制
        mcp.AddTool("self.robot.orientation_control", "机器人的姿态控制。机器人可以做以下姿态控制动作：\n"
            "rotate_x: 绕机身X轴旋转\nrotate_y: 绕机身Y轴旋转\nrotate_z: 绕机身Z轴旋转\ntwist: 扭动身体", 
            PropertyList({
                Property("action", kPropertyTypeString),
            }), [this](const PropertyList& properties) -> ReturnValue {
                const std::string& action = properties["action"].value<std::string>();
                std::string command;
                
                if (action == "rotate_x") {
                    command = "ROTATEX";
                } else if (action == "rotate_y") {
                    command = "ROTATEY";
                } else if (action == "rotate_z") {
                    command = "ROTATEZ";
                } else if (action == "twist") {
                    command = "TWIST";
                } else {
                    return false;
                }
                
                cJSON* result = nodehexa_controller_->SendCommand(command.c_str());
                bool success = (result != nullptr && cJSON_HasObjectItem(result, "status") && 
                               strcmp(cJSON_GetObjectItem(result, "status")->valuestring, "success") == 0);
                cJSON_Delete(result);
                return success;
            });
    }
};

DECLARE_BOARD(NodeHexaBoard);
