#include "web_socket.h"
#include <esp_log.h>
#include <cstdlib>
#include <cstring>

static const char *TAG = "WebSocket";

static std::string base64_encode(const unsigned char *data, size_t len) {
    const char *base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string encoded;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    size_t i = 0;
    while (i < len) {
        size_t chunk_size = std::min((size_t)3, len - i);
        
        for (size_t j = 0; j < 3; j++) {
            char_array_3[j] = (j < chunk_size) ? data[i + j] : 0;
        }

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (size_t j = 0; j < 4; j++) {
            if (j <= chunk_size) {
                encoded.push_back(base64_chars[char_array_4[j]]);
            } else {
                encoded.push_back('=');
            }
        }

        i += chunk_size;
    }
    return encoded;
}


WebSocket::WebSocket(Transport *transport) : transport_(transport) {
}

WebSocket::~WebSocket() {
    if (transport_->connected()) {
        transport_->Disconnect();
    }

    if (receive_thread_.joinable()) {
        receive_thread_.join();
    }
    delete transport_;
}

void WebSocket::SetHeader(const char* key, const char* value) {
    headers_[key] = value;
}

void WebSocket::SetReceiveBufferSize(size_t size) {
    receive_buffer_size_ = size;
}

bool WebSocket::IsConnected() const {
    return transport_->connected();
}

bool WebSocket::Connect(const char* uri) {
    std::string uri_str(uri);
    std::string protocol, host, port, path;
    size_t pos = 0;
    size_t next_pos = 0;

    // 解析协议
    next_pos = uri_str.find("://");
    if (next_pos == std::string::npos) {
        ESP_LOGE(TAG, "Invalid URI format");
        return false;
    }
    protocol = uri_str.substr(0, next_pos);
    pos = next_pos + 3;

    // 解析主机
    next_pos = uri_str.find(':', pos);
    if (next_pos == std::string::npos) {
        next_pos = uri_str.find('/', pos);
        if (next_pos == std::string::npos) {
            host = uri_str.substr(pos);
            path = "/";
        } else {
            host = uri_str.substr(pos, next_pos - pos);
            path = uri_str.substr(next_pos);
        }
        port = (protocol == "wss") ? "443" : "80";
    } else {
        host = uri_str.substr(pos, next_pos - pos);
        pos = next_pos + 1;
        
        // 解析端口
        next_pos = uri_str.find('/', pos);
        if (next_pos == std::string::npos) {
            port = uri_str.substr(pos);
            path = "/";
        } else {
            port = uri_str.substr(pos, next_pos - pos);
            path = uri_str.substr(next_pos);
        }
    }

    ESP_LOGI(TAG, "Connecting to %s://%s:%s%s", protocol.c_str(), host.c_str(), port.c_str(), path.c_str());

    // 设置 WebSocket 特定的头部
    SetHeader("Upgrade", "websocket");
    SetHeader("Connection", "Upgrade");
    SetHeader("Sec-WebSocket-Version", "13");
    
    // 生成随机的 Sec-WebSocket-Key
    char key[25];
    for (int i = 0; i < 16; ++i) {
        key[i] = rand() % 256;
    }
    std::string base64_key = base64_encode(reinterpret_cast<const unsigned char*>(key), 16);
    SetHeader("Sec-WebSocket-Key", base64_key.c_str());

    // 使用 transport 建立连接
    if (!transport_->Connect(host.c_str(), std::stoi(port))) {
        ESP_LOGE(TAG, "Failed to connect to server");
        return false;
    }

    // 发送 WebSocket 握手请求
    std::string request = "GET " + path + " HTTP/1.1\r\n";
    if (headers_.find("Host") == headers_.end()) {
        request += "Host: " + host + "\r\n";
    }
    for (const auto& header : headers_) {
        request += header.first + ": " + header.second + "\r\n";
    }
    request += "\r\n";

    if (!SendAllRaw(request.c_str(), request.length())) {
        ESP_LOGE(TAG, "Failed to send WebSocket handshake request");
        return false;
    }

    std::string buffer;
    // Read byte by byte until \r\n\r\n
    while (transport_->connected()) {
        char c = 0;
        if (transport_->Receive(&c, 1) == 1) {
            buffer.push_back(c);
            if (buffer.size() >= 4 && buffer.substr(buffer.size() - 4) == "\r\n\r\n") {
                break;
            }
        }
    }

    if (buffer.find("HTTP/1.1 101") == std::string::npos) {
        ESP_LOGE(TAG, "WebSocket handshake failed");
        return false;
    }

    if (on_connected_) {
        on_connected_();
    }

    // Start a task to receive data
    receive_thread_ = std::thread([this]() {
        ReceiveTask();
    });
    return true;
}


bool WebSocket::Send(const std::string& data) {
    return Send(data.data(), data.size(), false);
}

bool WebSocket::Send(const void* data, size_t len, bool binary, bool fin) {
    if (len > 65535) {
        ESP_LOGE(TAG, "Data too large, maximum supported size is 65535 bytes");
        return false;
    }

    std::vector<uint8_t> frame;
    frame.reserve(len + 8);  // 最大可能的帧大小（2字节帧头 + 2字节长度 + 4字节mask）

    // 第一个字节：FIN 位 + 操作码
    uint8_t first_byte = (fin ? 0x80 : 0x00);
    if (binary) {
        first_byte |= 0x02;  // 二进制帧
    } else if (!continuation_) {
        first_byte |= 0x01;  // 文本帧
    } // 否则，操作码为0（延续帧）

    frame.push_back(first_byte);

    // 第二个字节：MASK 位 + 有效载荷长度
    if (len < 126) {
        frame.push_back(0x80 | len);  // 设置MASK位
    } else {
        frame.push_back(0x80 | 126);  // 设置MASK位
        frame.push_back((len >> 8) & 0xFF);
        frame.push_back(len & 0xFF);
    }

    // 生成随机的4字节mask
    uint8_t mask[4];
    for (int i = 0; i < 4; ++i) {
        mask[i] = rand() & 0xFF;
    }
    frame.insert(frame.end(), mask, mask + 4);

    // 添加并mask处理有效载荷
    const uint8_t* payload = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < len; ++i) {
        frame.push_back(payload[i] ^ mask[i % 4]);
    }

    // 更新continuation_状态
    continuation_ = !fin;

    // 发送帧
    return SendAllRaw(frame.data(), frame.size());
}

void WebSocket::Ping() {
    SendControlFrame(0x9, nullptr, 0);
}

void WebSocket::Close() {
    if (transport_->connected()) {
        SendControlFrame(0x8, nullptr, 0);
    }
}

void WebSocket::OnConnected(std::function<void()> callback) {
    on_connected_ = callback;
}

void WebSocket::OnDisconnected(std::function<void()> callback) {
    on_disconnected_ = callback;
}

void WebSocket::OnData(std::function<void(const char*, size_t, bool binary)> callback) {
    on_data_ = callback;
}

void WebSocket::OnError(std::function<void(int)> callback) {
    on_error_ = callback;
}

void WebSocket::ReceiveTask() {
    size_t buffer_offset = 0;
    char* buffer = new char[receive_buffer_size_];
    
    std::vector<char> current_message;
    bool is_fragmented = false;
    bool is_binary = false;

    while (transport_->connected()) {
        int ret = transport_->Receive(buffer + buffer_offset, receive_buffer_size_ - buffer_offset);
        if (ret < 0) {
            if (on_error_) {
                on_error_(ret);
            }
            break;
        }
        if (ret > 0) {
            buffer_offset += ret;
            size_t frame_start = 0;

            while (frame_start < buffer_offset) {
                if (buffer_offset - frame_start < 2) break; // 需要更多数据

                uint8_t opcode = buffer[frame_start] & 0x0F;
                bool fin = (buffer[frame_start] & 0x80) != 0;
                uint8_t mask = buffer[frame_start + 1] & 0x80;
                uint64_t payload_length = buffer[frame_start + 1] & 0x7F;

                size_t header_length = 2;
                if (payload_length == 126) {
                    if (buffer_offset - frame_start < 4) break; // 需要更多数据
                    payload_length = (buffer[frame_start + 2] << 8) | buffer[frame_start + 3];
                    header_length += 2;
                } else if (payload_length == 127) {
                    if (buffer_offset - frame_start < 10) break; // 需要更多数据
                    payload_length = 0;
                    for (int i = 0; i < 8; ++i) {
                        payload_length = (payload_length << 8) | buffer[frame_start + 2 + i];
                    }
                    header_length += 8;
                }

                uint8_t mask_key[4] = {0};
                if (mask) {
                    if (buffer_offset - frame_start < header_length + 4) break; // 需要更多数据
                    memcpy(mask_key, buffer + frame_start + header_length, 4);
                    header_length += 4;
                }

                if (buffer_offset - frame_start < header_length + payload_length) break; // 需要更多数据

                // 解码有效载荷
                char* payload = buffer + frame_start + header_length;
                if (mask) {
                    for (size_t i = 0; i < payload_length; ++i) {
                        payload[i] ^= mask_key[i % 4];
                    }
                }

                // 处理帧
                switch (opcode) {
                    case 0x0: // 延续帧
                    case 0x1: // 文本帧
                    case 0x2: // 二进制帧
                        if (opcode != 0x0 && is_fragmented) {
                            ESP_LOGE(TAG, "Received new message frame while still fragmenting");
                            break;
                        }
                        if (opcode != 0x0) {
                            is_fragmented = !fin;
                            is_binary = (opcode == 0x2);
                            current_message.clear();
                        }
                        current_message.insert(current_message.end(), payload, payload + payload_length);
                        if (fin) {
                            on_data_(current_message.data(), current_message.size(), is_binary);
                            current_message.clear();
                            is_fragmented = false;
                        }
                        break;
                    case 0x8: // 关闭帧
                        transport_->Disconnect();
                        break;
                    case 0x9: // Ping
                        // 发送 Pong
                        SendControlFrame(0xA, payload, payload_length);
                        break;
                    case 0xA: // Pong
                        // 可以在这里处理 Pong
                        break;
                    default:
                        ESP_LOGE(TAG, "Unknown opcode: %d", opcode);
                        break;
                }

                frame_start += header_length + payload_length;
            }

            // 移动未处理的数据到缓冲区开始
            if (frame_start < buffer_offset) {
                memmove(buffer, buffer + frame_start, buffer_offset - frame_start);
            }
            buffer_offset -= frame_start;

            if (buffer_offset >= receive_buffer_size_) {
                ESP_LOGE(TAG, "Receive buffer overflow");
                transport_->Disconnect();
            }
        }
    }

    if (on_disconnected_) {
        on_disconnected_();
    }
    delete[] buffer;
}

bool WebSocket::SendAllRaw(const void* data, size_t len) {
    auto ptr = (char*)data;
    while (transport_->connected() && len > 0) {
        int sent = transport_->Send(ptr, len);
        if (sent < 0) {
            return false;
        }
        ptr += sent;
        len -= sent;
    }
    return true;
}

bool WebSocket::SendControlFrame(uint8_t opcode, const void* data, size_t len) {
    if (len > 125) {
        ESP_LOGE(TAG, "控制帧有效载荷过大");
        return false;
    }

    std::vector<uint8_t> frame;
    frame.reserve(len + 6);  // 帧头 + 掩码 + 有效载荷

    // 第一个字节：FIN 位 + 操作码
    frame.push_back(0x80 | opcode);

    // 第二个字节：MASK 位 + 有效载荷长度
    frame.push_back(0x80 | len);

    // 生成随机的4字节掩码
    uint8_t mask[4];
    for (int i = 0; i < 4; ++i) {
        mask[i] = rand() & 0xFF;
    }
    frame.insert(frame.end(), mask, mask + 4);

    // 添加并掩码处理有效载荷
    const uint8_t* payload = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < len; ++i) {
        frame.push_back(payload[i] ^ mask[i % 4]);
    }

    // 发送帧
    return SendAllRaw(frame.data(), frame.size());
}