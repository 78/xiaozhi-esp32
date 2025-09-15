#include "tools_manager.h"
#include "application.h"
#include "board.h"
#include "protocols/sleep_music_protocol.h"
#include <esp_log.h>

#define TAG "ToolsManager"

ToolsManager& ToolsManager::GetInstance() {
    static ToolsManager instance;
    return instance;
}

bool ToolsManager::Initialize() {
    if (initialized_) {
        ESP_LOGW(TAG, "ToolsManager already initialized");
        return true;
    }
    
    ESP_LOGI(TAG, "Initializing ToolsManager...");
    
    // 注册各种工具
    RegisterMcpTools();
    RegisterSystemTools();
    RegisterAudioTools();
    RegisterSensorTools();
    
    initialized_ = true;
    ESP_LOGI(TAG, "ToolsManager initialized successfully");
    return true;
}

void ToolsManager::RegisterMcpTools() {
    ESP_LOGI(TAG, "Registering MCP tools...");
    
    auto& mcp_server = McpServer::GetInstance();
    
    // 系统信息查询工具
    mcp_server.AddTool(
        "self.smart_speaker.get_system_info",
        "获取智能音箱系统信息，包括板卡类型、版本、功能特性等",
        PropertyList(),
        [](const PropertyList& properties) -> ReturnValue {
            auto& board = Board::GetInstance();
            return board.GetBoardJson();
        }
    );
    
    // 设备状态查询工具
    mcp_server.AddTool(
        "self.smart_speaker.get_device_state",
        "获取设备当前状态，包括启动状态、连接状态等",
        PropertyList(),
        [](const PropertyList& properties) -> ReturnValue {
            auto& app = Application::GetInstance();
            DeviceState state = app.GetDeviceState();
            const char* state_str = "unknown";
            switch (state) {
                case kDeviceStateStarting: state_str = "starting"; break;
                case kDeviceStateWifiConfiguring: state_str = "configuring"; break;
                case kDeviceStateIdle: state_str = "idle"; break;
                case kDeviceStateConnecting: state_str = "connecting"; break;
                case kDeviceStateListening: state_str = "listening"; break;
                case kDeviceStateSpeaking: state_str = "speaking"; break;
                case kDeviceStateUpgrading: state_str = "upgrading"; break;
                case kDeviceStateActivating: state_str = "activating"; break;
                case kDeviceStateAudioTesting: state_str = "audio_testing"; break;
                case kDeviceStateFatalError: state_str = "fatal_error"; break;
                default: state_str = "unknown"; break;
            }
            return std::string("{\"state\":\"") + state_str + "\"}";
        }
    );
    
    ESP_LOGI(TAG, "MCP tools registered successfully");
}

void ToolsManager::RegisterSystemTools() {
    ESP_LOGI(TAG, "Registering system tools...");
    
    auto& mcp_server = McpServer::GetInstance();
    
    // 系统重启工具
    mcp_server.AddTool(
        "self.smart_speaker.reboot",
        "重启智能音箱系统",
        PropertyList(),
        [](const PropertyList& properties) -> ReturnValue {
            auto& app = Application::GetInstance();
            app.Reboot();
            return "{\"message\":\"System reboot initiated\"}";
        }
    );
    
    // 设备控制工具
    mcp_server.AddTool(
        "self.smart_speaker.start_listening",
        "开始语音监听",
        PropertyList(),
        [](const PropertyList& properties) -> ReturnValue {
            auto& app = Application::GetInstance();
            app.StartListening();
            return "{\"message\":\"Started listening\"}";
        }
    );
    
    mcp_server.AddTool(
        "self.smart_speaker.stop_listening",
        "停止语音监听",
        PropertyList(),
        [](const PropertyList& properties) -> ReturnValue {
            auto& app = Application::GetInstance();
            app.StopListening();
            return "{\"message\":\"Stopped listening\"}";
        }
    );
    
    ESP_LOGI(TAG, "System tools registered successfully");
}

void ToolsManager::RegisterAudioTools() {
    ESP_LOGI(TAG, "Registering audio tools...");
    
    auto& mcp_server = McpServer::GetInstance();
    
    // 音频播放工具
    mcp_server.AddTool(
        "self.smart_speaker.play_sound",
        "播放指定音效。sound: 音效名称(activation, welcome, upgrade, wificonfig等)",
        PropertyList({Property("sound", kPropertyTypeString, std::string("activation"))}),
        [](const PropertyList& properties) -> ReturnValue {
            auto& app = Application::GetInstance();
            std::string sound = properties["sound"].value<std::string>();
            app.PlaySound(sound);
            return "{\"message\":\"Playing sound: " + sound + "\"}";
        }
    );
    
    // 语音检测状态工具
    mcp_server.AddTool(
        "self.smart_speaker.is_voice_detected",
        "检查是否检测到语音",
        PropertyList(),
        [](const PropertyList& properties) -> ReturnValue {
            auto& app = Application::GetInstance();
            bool voice_detected = app.IsVoiceDetected();
            return "{\"voice_detected\":" + std::string(voice_detected ? "true" : "false") + "}";
        }
    );
    
    // 睡眠音乐工具
    mcp_server.AddTool(
        "self.smart_speaker.start_sleep_music",
        "启动助眠模式，持续播放助眠音乐",
        PropertyList(),
        [](const PropertyList& properties) -> ReturnValue {
            // 获取睡眠音乐协议单例
            auto& sleep_protocol = SleepMusicProtocol::GetInstance();
            if (sleep_protocol.IsAudioChannelOpened()) {
                return std::string("{\"success\": true, \"message\": \"Sleep music already started\"}");
            }
            
            // 启动协议
            if (sleep_protocol.OpenAudioChannel()) {
                return std::string("{\"success\": true, \"message\": \"Sleep music started successfully\"}");
            } else {
                return std::string("{\"success\": false, \"message\": \"Failed to start sleep music\"}");
            }
        }
    );
    
    mcp_server.AddTool(
        "self.smart_speaker.stop_sleep_music",
        "停止助眠模式",
        PropertyList(),
        [](const PropertyList& properties) -> ReturnValue {
            // 获取睡眠音乐协议单例并停止
            auto& sleep_protocol = SleepMusicProtocol::GetInstance();
            sleep_protocol.CloseAudioChannel();
            return std::string("{\"success\": true, \"message\": \"Sleep music stopped\"}");
        }
    );
    
    ESP_LOGI(TAG, "Audio tools registered successfully");
}

void ToolsManager::RegisterSensorTools() {
    ESP_LOGI(TAG, "Registering sensor tools...");
    
    auto& mcp_server = McpServer::GetInstance();
    
    // 压感传感器读取工具
    mcp_server.AddTool(
        "self.smart_speaker.get_pressure_sensor",
        "获取压感传感器数据，包括当前值、ADC通道、样本数量等",
        PropertyList(),
        [](const PropertyList& properties) -> ReturnValue {
            auto& board = Board::GetInstance();
            std::string board_json = board.GetBoardJson();
            
            // 从board JSON中提取压感传感器信息
            // 这里简化处理，直接返回board信息中包含的传感器数据
            return board_json;
        }
    );
    
    // IMU传感器状态工具
    mcp_server.AddTool(
        "self.smart_speaker.get_imu_status",
        "获取IMU传感器状态信息",
        PropertyList(),
        [](const PropertyList& properties) -> ReturnValue {
            auto& board = Board::GetInstance();
            std::string board_json = board.GetBoardJson();
            
            // 从board JSON中提取IMU信息
            return board_json;
        }
    );
    
    // 传感器数据重置工具
    mcp_server.AddTool(
        "self.smart_speaker.reset_sensor_data",
        "重置传感器数据缓冲区",
        PropertyList(),
        [](const PropertyList& properties) -> ReturnValue {
            // TODO: 实现传感器数据重置
            return "{\"message\":\"Sensor data reset requested\"}";
        }
    );
    
    ESP_LOGI(TAG, "Sensor tools registered successfully");
}
