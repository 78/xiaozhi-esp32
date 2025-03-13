#include "mqtt_protocol.h"
#include "board.h"
#include "application.h"
#include "settings.h"

#include <esp_log.h>
#include <ml307_mqtt.h>
#include <ml307_udp.h>
#include <cstring>
#include <arpa/inet.h>
#include "assets/lang_config.h"

#define TAG "MQTT"  // 定义日志标签

// MqttProtocol 构造函数
MqttProtocol::MqttProtocol() {
    event_group_handle_ = xEventGroupCreate();  // 创建一个事件组，用于任务间同步
}

// MqttProtocol 析构函数
MqttProtocol::~MqttProtocol() {
    ESP_LOGI(TAG, "MqttProtocol deinit");  // 记录日志，表示 MqttProtocol 正在销毁
    if (udp_ != nullptr) {
        delete udp_;  // 删除 UDP 对象
    }
    if (mqtt_ != nullptr) {
        delete mqtt_;  // 删除 MQTT 对象
    }
    vEventGroupDelete(event_group_handle_);  // 删除事件组
}

// 启动 MQTT 协议
void MqttProtocol::Start() {
    StartMqttClient(false);  // 启动 MQTT 客户端，不报告错误
}

// 启动 MQTT 客户端
bool MqttProtocol::StartMqttClient(bool report_error) {
    if (mqtt_ != nullptr) {
        ESP_LOGW(TAG, "Mqtt client already started");  // 如果 MQTT 客户端已经启动，记录警告日志
        delete mqtt_;  // 删除现有的 MQTT 客户端
    }

    // 从设置中获取 MQTT 配置
    Settings settings("mqtt", false);
    endpoint_ = settings.GetString("endpoint");  // 获取 MQTT 服务器地址
    client_id_ = settings.GetString("client_id");  // 获取客户端 ID
    username_ = settings.GetString("username");  // 获取用户名
    password_ = settings.GetString("password");  // 获取密码
    publish_topic_ = settings.GetString("publish_topic");  // 获取发布主题

    if (endpoint_.empty()) {
        ESP_LOGW(TAG, "MQTT endpoint is not specified");  // 如果 MQTT 服务器地址未指定，记录警告日志
        if (report_error) {
            SetError(Lang::Strings::SERVER_NOT_FOUND);  // 如果需要报告错误，设置错误信息
        }
        return false;
    }

    mqtt_ = Board::GetInstance().CreateMqtt();  // 创建 MQTT 客户端实例
    mqtt_->SetKeepAlive(90);  // 设置 MQTT 心跳间隔为 90 秒

    // 设置 MQTT 断开连接时的回调函数
    mqtt_->OnDisconnected([this]() {
        ESP_LOGI(TAG, "Disconnected from endpoint");  // 记录断开连接的日志
    });

    // 设置 MQTT 消息到达时的回调函数
    mqtt_->OnMessage([this](const std::string& topic, const std::string& payload) {
        cJSON* root = cJSON_Parse(payload.c_str());  // 解析 JSON 格式的消息
        if (root == nullptr) {
            ESP_LOGE(TAG, "Failed to parse json message %s", payload.c_str());  // 如果解析失败，记录错误日志
            return;
        }
        cJSON* type = cJSON_GetObjectItem(root, "type");  // 获取消息类型
        if (type == nullptr) {
            ESP_LOGE(TAG, "Message type is not specified");  // 如果消息类型未指定，记录错误日志
            cJSON_Delete(root);  // 删除 JSON 对象
            return;
        }

        // 根据消息类型处理不同的消息
        if (strcmp(type->valuestring, "hello") == 0) {
            ParseServerHello(root);  // 处理服务器 hello 消息
        } else if (strcmp(type->valuestring, "goodbye") == 0) {
            auto session_id = cJSON_GetObjectItem(root, "session_id");  // 获取会话 ID
            ESP_LOGI(TAG, "Received goodbye message, session_id: %s", session_id ? session_id->valuestring : "null");  // 记录 goodbye 消息日志
            if (session_id == nullptr || session_id_ == session_id->valuestring) {
                Application::GetInstance().Schedule([this]() {
                    CloseAudioChannel();  // 关闭音频通道
                });
            }
        } else if (on_incoming_json_ != nullptr) {
            on_incoming_json_(root);  // 调用自定义的 JSON 消息处理函数
        }
        cJSON_Delete(root);  // 删除 JSON 对象
        last_incoming_time_ = std::chrono::steady_clock::now();  // 更新最后接收消息的时间
    });

    ESP_LOGI(TAG, "Connecting to endpoint %s", endpoint_.c_str());  // 记录连接日志
    if (!mqtt_->Connect(endpoint_, 8883, client_id_, username_, password_)) {
        ESP_LOGE(TAG, "Failed to connect to endpoint");  // 如果连接失败，记录错误日志
        SetError(Lang::Strings::SERVER_NOT_CONNECTED);  // 设置错误信息
        return false;
    }

    ESP_LOGI(TAG, "Connected to endpoint");  // 记录连接成功日志
    return true;
}

// 发送文本消息
void MqttProtocol::SendText(const std::string& text) {
    if (publish_topic_.empty()) {
        return;  // 如果发布主题为空，直接返回
    }
    if (!mqtt_->Publish(publish_topic_, text)) {
        ESP_LOGE(TAG, "Failed to publish message: %s", text.c_str());  // 如果发布失败，记录错误日志
        SetError(Lang::Strings::SERVER_ERROR);  // 设置错误信息
    }
}

// 发送音频数据
void MqttProtocol::SendAudio(const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(channel_mutex_);  // 加锁，保护共享资源
    if (udp_ == nullptr) {
        return;  // 如果 UDP 对象为空，直接返回
    }

    // 生成 nonce 并填充数据大小和序列号
    std::string nonce(aes_nonce_);
    *(uint16_t*)&nonce[2] = htons(data.size());
    *(uint32_t*)&nonce[12] = htonl(++local_sequence_);

    // 加密音频数据
    std::string encrypted;
    encrypted.resize(aes_nonce_.size() + data.size());
    memcpy(encrypted.data(), nonce.data(), nonce.size());

    size_t nc_off = 0;
    uint8_t stream_block[16] = {0};
    if (mbedtls_aes_crypt_ctr(&aes_ctx_, data.size(), &nc_off, (uint8_t*)nonce.c_str(), stream_block,
        (uint8_t*)data.data(), (uint8_t*)&encrypted[nonce.size()]) != 0) {
        ESP_LOGE(TAG, "Failed to encrypt audio data");  // 如果加密失败，记录错误日志
        return;
    }
    udp_->Send(encrypted);  // 发送加密后的音频数据
}

// 关闭音频通道
void MqttProtocol::CloseAudioChannel() {
    {
        std::lock_guard<std::mutex> lock(channel_mutex_);  // 加锁，保护共享资源
        if (udp_ != nullptr) {
            delete udp_;  // 删除 UDP 对象
            udp_ = nullptr;
        }
    }

    // 发送 goodbye 消息
    std::string message = "{";
    message += "\"session_id\":\"" + session_id_ + "\",";
    message += "\"type\":\"goodbye\"";
    message += "}";
    SendText(message);

    if (on_audio_channel_closed_ != nullptr) {
        on_audio_channel_closed_();  // 调用音频通道关闭回调函数
    }
}

// 打开音频通道
bool MqttProtocol::OpenAudioChannel() {
    if (mqtt_ == nullptr || !mqtt_->IsConnected()) {
        ESP_LOGI(TAG, "MQTT is not connected, try to connect now");  // 如果 MQTT 未连接，尝试重新连接
        if (!StartMqttClient(true)) {
            return false;  // 如果连接失败，返回 false
        }
    }

    error_occurred_ = false;  // 重置错误标志
    session_id_ = "";  // 清空会话 ID
    xEventGroupClearBits(event_group_handle_, MQTT_PROTOCOL_SERVER_HELLO_EVENT);  // 清除事件组中的 SERVER_HELLO 事件

    // 发送 hello 消息申请 UDP 通道
    std::string message = "{";
    message += "\"type\":\"hello\",";
    message += "\"version\": 3,";
    message += "\"transport\":\"udp\",";
    message += "\"audio_params\":{";
    message += "\"format\":\"opus\", \"sample_rate\":16000, \"channels\":1, \"frame_duration\":" + std::to_string(OPUS_FRAME_DURATION_MS);
    message += "}}";
    SendText(message);

    // 等待服务器响应
    EventBits_t bits = xEventGroupWaitBits(event_group_handle_, MQTT_PROTOCOL_SERVER_HELLO_EVENT, pdTRUE, pdFALSE, pdMS_TO_TICKS(10000));
    if (!(bits & MQTT_PROTOCOL_SERVER_HELLO_EVENT)) {
        ESP_LOGE(TAG, "Failed to receive server hello");  // 如果未收到服务器 hello 消息，记录错误日志
        SetError(Lang::Strings::SERVER_TIMEOUT);  // 设置错误信息
        return false;
    }

    std::lock_guard<std::mutex> lock(channel_mutex_);  // 加锁，保护共享资源
    if (udp_ != nullptr) {
        delete udp_;  // 删除现有的 UDP 对象
    }
    udp_ = Board::GetInstance().CreateUdp();  // 创建新的 UDP 对象
    udp_->OnMessage([this](const std::string& data) {
        if (data.size() < sizeof(aes_nonce_)) {
            ESP_LOGE(TAG, "Invalid audio packet size: %zu", data.size());  // 如果音频包大小无效，记录错误日志
            return;
        }
        if (data[0] != 0x01) {
            ESP_LOGE(TAG, "Invalid audio packet type: %x", data[0]);  // 如果音频包类型无效，记录错误日志
            return;
        }
        uint32_t sequence = ntohl(*(uint32_t*)&data[12]);  // 获取序列号
        if (sequence < remote_sequence_) {
            ESP_LOGW(TAG, "Received audio packet with old sequence: %lu, expected: %lu", sequence, remote_sequence_);  // 如果收到旧序列号，记录警告日志
            return;
        }
        if (sequence != remote_sequence_ + 1) {
            ESP_LOGW(TAG, "Received audio packet with wrong sequence: %lu, expected: %lu", sequence, remote_sequence_ + 1);  // 如果收到错误的序列号，记录警告日志
        }

        // 解密音频数据
        std::vector<uint8_t> decrypted;
        size_t decrypted_size = data.size() - aes_nonce_.size();
        size_t nc_off = 0;
        uint8_t stream_block[16] = {0};
        decrypted.resize(decrypted_size);
        auto nonce = (uint8_t*)data.data();
        auto encrypted = (uint8_t*)data.data() + aes_nonce_.size();
        int ret = mbedtls_aes_crypt_ctr(&aes_ctx_, decrypted_size, &nc_off, nonce, stream_block, encrypted, (uint8_t*)decrypted.data());
        if (ret != 0) {
            ESP_LOGE(TAG, "Failed to decrypt audio data, ret: %d", ret);  // 如果解密失败，记录错误日志
            return;
        }
        if (on_incoming_audio_ != nullptr) {
            on_incoming_audio_(std::move(decrypted));  // 调用音频数据接收回调函数
        }
        remote_sequence_ = sequence;  // 更新远程序列号
        last_incoming_time_ = std::chrono::steady_clock::now();  // 更新最后接收消息的时间
    });

    udp_->Connect(udp_server_, udp_port_);  // 连接 UDP 服务器

    if (on_audio_channel_opened_ != nullptr) {
        on_audio_channel_opened_();  // 调用音频通道打开回调函数
    }
    return true;
}

// 解析服务器 hello 消息
void MqttProtocol::ParseServerHello(const cJSON* root) {
    auto transport = cJSON_GetObjectItem(root, "transport");  // 获取传输方式
    if (transport == nullptr || strcmp(transport->valuestring, "udp") != 0) {
        ESP_LOGE(TAG, "Unsupported transport: %s", transport->valuestring);  // 如果不支持该传输方式，记录错误日志
        return;
    }

    auto session_id = cJSON_GetObjectItem(root, "session_id");  // 获取会话 ID
    if (session_id != nullptr) {
        session_id_ = session_id->valuestring;  // 设置会话 ID
        ESP_LOGI(TAG, "Session ID: %s", session_id_.c_str());  // 记录会话 ID 日志
    }

    // 从 hello 消息中获取采样率
    auto audio_params = cJSON_GetObjectItem(root, "audio_params");
    if (audio_params != NULL) {
        auto sample_rate = cJSON_GetObjectItem(audio_params, "sample_rate");
        if (sample_rate != NULL) {
            server_sample_rate_ = sample_rate->valueint;  // 设置服务器采样率
        }
    }

    auto udp = cJSON_GetObjectItem(root, "udp");  // 获取 UDP 配置
    if (udp == nullptr) {
        ESP_LOGE(TAG, "UDP is not specified");  // 如果 UDP 配置未指定，记录错误日志
        return;
    }
    udp_server_ = cJSON_GetObjectItem(udp, "server")->valuestring;  // 获取 UDP 服务器地址
    udp_port_ = cJSON_GetObjectItem(udp, "port")->valueint;  // 获取 UDP 端口
    auto key = cJSON_GetObjectItem(udp, "key")->valuestring;  // 获取加密密钥
    auto nonce = cJSON_GetObjectItem(udp, "nonce")->valuestring;  // 获取 nonce

    aes_nonce_ = DecodeHexString(nonce);  // 解码 nonce
    mbedtls_aes_init(&aes_ctx_);  // 初始化 AES 上下文
    mbedtls_aes_setkey_enc(&aes_ctx_, (const unsigned char*)DecodeHexString(key).c_str(), 128);  // 设置 AES 加密密钥
    local_sequence_ = 0;  // 重置本地序列号
    remote_sequence_ = 0;  // 重置远程序列号
    xEventGroupSetBits(event_group_handle_, MQTT_PROTOCOL_SERVER_HELLO_EVENT);  // 设置 SERVER_HELLO 事件
}

// 辅助函数，将单个十六进制字符转换为对应的数值
static inline uint8_t CharToHex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;  // 对于无效输入，返回0
}

// 解码十六进制字符串
std::string MqttProtocol::DecodeHexString(const std::string& hex_string) {
    std::string decoded;
    decoded.reserve(hex_string.size() / 2);  // 预分配内存
    for (size_t i = 0; i < hex_string.size(); i += 2) {
        char byte = (CharToHex(hex_string[i]) << 4) | CharToHex(hex_string[i + 1]);  // 将两个十六进制字符转换为一个字节
        decoded.push_back(byte);  // 将字节添加到解码后的字符串中
    }
    return decoded;
}

// 检查音频通道是否已打开
bool MqttProtocol::IsAudioChannelOpened() const {
    return udp_ != nullptr && !error_occurred_ && !IsTimeout();  // 如果 UDP 对象存在且没有错误发生且未超时，返回 true
}