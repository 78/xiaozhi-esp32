# XiaoZhi ESP32 云端通信架构详解

本文档详细介绍XiaoZhi项目的云端通信机制，包括协议层设计、消息格式、MCP实现和事件上报流程。

## 1. 架构概览

XiaoZhi采用分层架构设计，实现了协议无关的云端通信：

```
┌─────────────────────────────────────┐
│         Application Layer           │
│  (application.cc, mcp_server.cc)    │
├─────────────────────────────────────┤
│         Protocol Layer              │
│        (protocol.h/cc)              │
├─────────────┬───────────────────────┤
│    MQTT     │      WebSocket        │
│ Protocol    │      Protocol         │
├─────────────┴───────────────────────┤
│         Network Layer               │
│    (ESP-IDF TCP/IP Stack)          │
└─────────────────────────────────────┘
```

## 2. 协议层设计

### 2.1 Protocol基类
```cpp
// main/protocols/protocol.h
class Protocol {
public:
    // 音频通道管理
    virtual bool OpenAudioChannel() = 0;
    virtual void CloseAudioChannel() = 0;
    virtual bool IsAudioChannelOpened() const = 0;
    
    // 消息发送接口
    virtual bool SendAudio(std::unique_ptr<AudioStreamPacket> packet) = 0;
    virtual void SendWakeWordDetected(const std::string& wake_word);
    virtual void SendStartListening(ListeningMode mode);
    virtual void SendStopListening();
    virtual void SendAbortSpeaking(AbortReason reason);
    virtual void SendMcpMessage(const std::string& message);
    
    // 回调注册
    void OnIncomingJson(std::function<void(const cJSON* root)> callback);
    void OnIncomingAudio(std::function<void(std::unique_ptr<AudioStreamPacket>)> callback);
    void OnAudioChannelOpened(std::function<void()> callback);
    void OnAudioChannelClosed(std::function<void()> callback);
    void OnNetworkError(std::function<void(const std::string&)> callback);
    
protected:
    virtual bool SendText(const std::string& text) = 0;
    std::string session_id_;  // 每个音频会话的唯一标识
};
```

### 2.2 MQTT协议实现
```cpp
// main/protocols/mqtt_protocol.cc
class MqttProtocol : public Protocol {
private:
    std::unique_ptr<Mqtt> mqtt_;
    std::unique_ptr<Udp> udp_;  // 音频数据通过UDP传输
    std::string publish_topic_;  // MQTT发布主题
    
    bool SendText(const std::string& text) override {
        if (!mqtt_->Publish(publish_topic_, text)) {
            ESP_LOGE(TAG, "Failed to publish message: %s", text.c_str());
            SetError(Lang::Strings::SERVER_ERROR);
            return false;
        }
        return true;
    }
};
```

### 2.3 WebSocket协议实现
```cpp
// main/protocols/websocket_protocol.cc
class WebsocketProtocol : public Protocol {
private:
    std::unique_ptr<WebSocket> websocket_;
    int version_ = 1;  // 协议版本
    
    bool SendText(const std::string& text) override {
        if (!websocket_->Send(text)) {
            ESP_LOGE(TAG, "Failed to send text: %s", text.c_str());
            SetError(Lang::Strings::SERVER_ERROR);
            return false;
        }
        return true;
    }
};
```

## 3. 消息格式

### 3.1 标准消息结构
所有发送到云端的消息都包含以下基本字段：
```json
{
    "session_id": "uuid-string",  // 会话标识
    "type": "message-type",       // 消息类型
    // ... 其他字段根据type不同而变化
}
```

### 3.2 消息类型定义

#### 3.2.1 语音识别消息 (type: "listen")
```json
// 检测到唤醒词
{
    "session_id": "xxx",
    "type": "listen",
    "state": "detect",
    "text": "小智同学"  // 唤醒词
}

// 开始监听
{
    "session_id": "xxx",
    "type": "listen",
    "state": "start",
    "mode": "auto|manual|realtime"  // 监听模式
}

// 停止监听
{
    "session_id": "xxx",
    "type": "listen",
    "state": "stop"
}
```

#### 3.2.2 控制消息 (type: "abort")
```json
{
    "session_id": "xxx",
    "type": "abort",
    "reason": "wake_word_detected"  // 可选
}
```

#### 3.2.3 MCP消息 (type: "mcp")
```json
{
    "session_id": "xxx",
    "type": "mcp",
    "payload": {
        // MCP JSON-RPC 2.0格式的内容
        "jsonrpc": "2.0",
        "id": 1,
        "result": {...}
    }
}
```

#### 3.2.4 会话管理消息
```json
// 打开音频通道（Hello消息）
{
    "session_id": "xxx",
    "type": "hello",
    "version": 2,
    "device_id": "xxx",
    "sample_rate": 24000,
    "frame_duration": 60,
    "audio_codec": "opus",
    "capabilities": {...}
}

// 关闭音频通道（Goodbye消息）
{
    "session_id": "xxx",
    "type": "goodbye"
}
```

## 4. MCP（Model Context Protocol）实现

### 4.1 MCP服务器架构
```cpp
// main/mcp_server.h
class McpServer {
public:
    // 工具管理
    void AddTool(const std::string& name, 
                 const std::string& description,
                 const PropertyList& properties,
                 std::function<ReturnValue(const PropertyList&)> callback);
    
    // 消息处理
    void ParseMessage(const cJSON* json);
    
private:
    std::vector<McpTool*> tools_;  // 已注册的工具列表
    
    // JSON-RPC处理方法
    void GetToolsList(int id, const std::string& cursor);
    void DoToolCall(int id, const std::string& tool_name, 
                    const cJSON* arguments, int stack_size);
    void ReplyResult(int id, const std::string& result);
    void ReplyError(int id, const std::string& message);
};
```

### 4.2 工具定义示例
```cpp
// 音量控制工具
AddTool("self.audio_speaker.set_volume", 
    "Set the volume of the audio speaker.",
    PropertyList({
        Property("volume", kPropertyTypeInteger, 0, 100)
    }), 
    [&board](const PropertyList& properties) -> ReturnValue {
        auto codec = board.GetAudioCodec();
        codec->SetOutputVolume(properties["volume"].value<int>());
        return true;
    });

// 拍照工具
AddTool("self.camera.take_photo",
    "Take a photo and explain it.",
    PropertyList({
        Property("question", kPropertyTypeString)
    }),
    [camera](const PropertyList& properties) -> ReturnValue {
        if (!camera->Capture()) {
            return "{\"success\": false, \"message\": \"Failed to capture photo\"}";
        }
        // ... 处理图片并返回结果
    });
```

### 4.3 MCP消息流程
1. **云端请求** → WebSocket/MQTT → `OnIncomingJson`回调
2. **解析MCP消息**：检查type=="mcp"，提取payload
3. **调用ParseMessage**：解析JSON-RPC请求
4. **执行工具**：在独立线程中运行工具函数
5. **返回结果**：通过`SendMcpMessage`发送响应

## 5. 事件上报机制

### 5.1 应用层接口
```cpp
// application.cc
void Application::SendMcpMessage(const std::string& payload) {
    Schedule([this, payload]() {
        if (protocol_) {
            protocol_->SendMcpMessage(payload);
        }
    });
}
```

### 5.2 自定义事件上报
```cpp
// 运动检测事件示例
void ReportMotionEvent(MotionEvent event) {
    const char* event_name = GetEventName(event);
    
    // 构建JSON消息
    char json_buffer[256];
    snprintf(json_buffer, sizeof(json_buffer),
            "{\"type\":\"event\",\"event_id\":\"%s\",\"timestamp\":%lld}",
            event_name, esp_timer_get_time() / 1000);
    
    // 通过MCP通道发送
    Application::GetInstance().SendMcpMessage(std::string(json_buffer));
}
```

### 5.3 完整消息示例
发送的原始消息：
```json
{
    "type": "event",
    "event_id": "pickup",
    "timestamp": 1234567890
}
```

经过协议层封装后的最终消息：
```json
{
    "session_id": "f47ac10b-58cc-4372-a567-0e02b2c3d479",
    "type": "mcp",
    "payload": {
        "type": "event",
        "event_id": "pickup",
        "timestamp": 1234567890
    }
}
```

## 6. 线程模型与同步

### 6.1 主线程事件循环
```cpp
// application.cc
void Application::MainEventLoop() {
    while (true) {
        auto bits = xEventGroupWaitBits(event_group_, 
            MAIN_EVENT_SCHEDULE | MAIN_EVENT_SEND_AUDIO | ..., 
            pdTRUE, pdFALSE, portMAX_DELAY);
            
        if (bits & MAIN_EVENT_SCHEDULE) {
            // 执行Schedule()调度的任务
            std::unique_lock<std::mutex> lock(mutex_);
            auto tasks = std::move(main_tasks_);
            lock.unlock();
            for (auto& task : tasks) {
                task();
            }
        }
    }
}
```

### 6.2 Schedule机制
```cpp
void Application::Schedule(std::function<void()> callback) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        main_tasks_.push_back(std::move(callback));
    }
    xEventGroupSetBits(event_group_, MAIN_EVENT_SCHEDULE);
}
```
- 确保所有协议操作在主线程执行
- 避免多线程访问协议实例

### 6.3 MCP工具执行
```cpp
// MCP工具在独立线程中执行
tool_call_thread_ = std::thread([this, id, tool_iter, arguments]() {
    try {
        ReplyResult(id, (*tool_iter)->Call(arguments));
    } catch (const std::exception& e) {
        ReplyError(id, e.what());
    }
});
tool_call_thread_.detach();
```

## 7. 错误处理

### 7.1 网络错误
```cpp
protocol_->OnNetworkError([this](const std::string& message) {
    last_error_message_ = message;
    xEventGroupSetBits(event_group_, MAIN_EVENT_ERROR);
});
```

### 7.2 超时检测
```cpp
bool Protocol::IsTimeout() const {
    const int kTimeoutSeconds = 120;
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(
        now - last_incoming_time_);
    return duration.count() > kTimeoutSeconds;
}
```

## 8. 配置管理

### 8.1 协议选择
```cpp
// application.cc - 根据OTA配置选择协议
if (ota.HasMqttConfig()) {
    protocol_ = std::make_unique<MqttProtocol>();
} else if (ota.HasWebsocketConfig()) {
    protocol_ = std::make_unique<WebsocketProtocol>();
} else {
    protocol_ = std::make_unique<MqttProtocol>();  // 默认MQTT
}
```

### 8.2 MQTT配置
- Broker地址和端口
- 发布/订阅主题
- 客户端ID
- 认证信息

### 8.3 WebSocket配置
- 服务器URL
- 协议版本
- 认证令牌

## 9. 扩展指南

### 9.1 添加新的消息类型
1. 在Protocol基类添加发送方法
2. 实现消息格式化逻辑
3. 在应用层调用新方法

### 9.2 添加新的MCP工具
```cpp
board.AddTool("tool.name",
    "Tool description",
    PropertyList({
        Property("param1", kPropertyTypeString),
        Property("param2", kPropertyTypeInteger, 0, 100)
    }),
    [](const PropertyList& props) -> ReturnValue {
        // 实现工具逻辑
        return "{\"result\": \"success\"}";
    });
```

### 9.3 添加新的协议支持
1. 继承Protocol基类
2. 实现所有纯虚函数
3. 在应用层添加协议选择逻辑

## 10. 最佳实践

1. **使用Schedule()**：确保协议操作在主线程执行
2. **错误处理**：始终检查发送返回值
3. **资源管理**：使用智能指针管理协议实例
4. **日志记录**：记录关键操作用于调试
5. **超时处理**：实现合理的超时和重连机制
6. **消息大小**：注意MQTT和WebSocket的消息大小限制
7. **并发安全**：MCP工具实现必须线程安全

## 总结

XiaoZhi的云端通信架构设计灵活、扩展性强，通过协议抽象层实现了应用逻辑与通信细节的解耦。MCP机制提供了强大的远程控制能力，而事件上报功能则支持设备主动向云端推送状态变化。整个系统在保证功能完整性的同时，也充分考虑了嵌入式环境的资源限制。