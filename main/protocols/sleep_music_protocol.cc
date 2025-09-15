#include "sleep_music_protocol.h"
#include "board.h"
#include "application.h"
#include "protocol.h"

#include <cstring>
#include <esp_log.h>

#define TAG "SleepMusic"

SleepMusicProtocol& SleepMusicProtocol::GetInstance() {
    static SleepMusicProtocol instance;
    return instance;
}

SleepMusicProtocol::SleepMusicProtocol() {
    event_group_handle_ = xEventGroupCreate();
}

SleepMusicProtocol::~SleepMusicProtocol() {
    vEventGroupDelete(event_group_handle_);
}

bool SleepMusicProtocol::IsAudioChannelOpened() const {
    return is_connected_ && websocket_ != nullptr && websocket_->IsConnected();
}

void SleepMusicProtocol::CloseAudioChannel() {
    if (websocket_) {
        ESP_LOGI(TAG, "Closing sleep music audio channel");
        
        // 清理状态
        is_connected_ = false;
        
        // 关闭WebSocket连接
        websocket_.reset();
        
        ESP_LOGI(TAG, "Sleep music audio channel closed");
    }
}

bool SleepMusicProtocol::OpenAudioChannel() {
    std::string url = "ws://180.76.190.230:8765";
    
    ESP_LOGI(TAG, "Connecting to sleep music server: %s", url.c_str());

    auto network = Board::GetInstance().GetNetwork();
    websocket_ = network->CreateWebSocket(2); // 使用不同的WebSocket实例ID
    if (websocket_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create websocket for sleep music");
        return false;
    }

    // 设置WebSocket数据接收回调
    websocket_->OnData([this](const char* data, size_t len, bool binary) {
        if (binary) {
            // 接收到的二进制数据是OPUS编码的音频帧
            OnAudioDataReceived(data, len);
        } else {
            ESP_LOGW(TAG, "Received non-binary data from sleep music server, ignoring");
        }
    });

    websocket_->OnDisconnected([this]() {
        ESP_LOGI(TAG, "Sleep music websocket disconnected");
    });

    // 连接到睡眠音乐服务器
    if (!websocket_->Connect(url.c_str())) {
        ESP_LOGE(TAG, "Failed to connect to sleep music server");
        return false;
    }

    // 设置连接成功事件
    xEventGroupSetBits(event_group_handle_, SLEEP_MUSIC_PROTOCOL_CONNECTED_EVENT);

    ESP_LOGI(TAG, "Successfully connected to sleep music server");
    is_connected_ = true;
    return true;
}

void SleepMusicProtocol::OnAudioDataReceived(const char* data, size_t len) {
    if (len == 0) {
        ESP_LOGW(TAG, "Received empty audio data");
        return;
    }

    ESP_LOGD(TAG, "Received audio frame: %zu bytes", len);

    // 创建AudioStreamPacket
    auto packet = std::make_unique<AudioStreamPacket>();
    packet->sample_rate = SAMPLE_RATE;
    packet->frame_duration = FRAME_DURATION_MS;
    packet->timestamp = 0; // 睡眠音乐不需要时间戳同步
    packet->payload.resize(len);
    std::memcpy(packet->payload.data(), data, len);

    // 将音频包推入解码队列
    auto& app = Application::GetInstance();
    auto& audio_service = app.GetAudioService();
    
    if (!audio_service.PushPacketToDecodeQueue(std::move(packet), false)) {
        ESP_LOGW(TAG, "Audio decode queue is full, dropping packet");
    }
}
