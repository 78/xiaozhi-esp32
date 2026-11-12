#include "mqtt_protocol.h"
#include "application.h"
#include "board.h"
#include "settings.h"

#include <esp_log.h>
#include <esp_random.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <algorithm>
#include <charconv>
#include <cstring>
#include <system_error>
#include "assets/lang_config.h"

#define TAG "MQTT"

MqttProtocol::MqttProtocol() {
    event_group_handle_ = xEventGroupCreate();

    // Initialize reconnect timer
    esp_timer_create_args_t reconnect_timer_args = {
        .callback =
            [](void* arg) {
                MqttProtocol* protocol = (MqttProtocol*)arg;
                auto& app = Application::GetInstance();
                if (app.GetDeviceState() == kDeviceStateIdle) {
                    ESP_LOGI(TAG, "Reconnecting to MQTT server");
                    auto alive = protocol->alive_;  // Capture alive flag
                    app.Schedule([protocol, alive]() {
                        if (*alive) {
                            protocol->StartMqttClient(false);
                        }
                    });
                }
            },
        .arg = this,
    };
    esp_timer_create(&reconnect_timer_args, &reconnect_timer_);
}

MqttProtocol::~MqttProtocol() {
    ESP_LOGI(TAG, "MqttProtocol deinit");

    // Mark as dead first to prevent any pending scheduled tasks from executing
    *alive_ = false;

    if (reconnect_timer_ != nullptr) {
        esp_timer_stop(reconnect_timer_);
        esp_timer_delete(reconnect_timer_);
    }

    std::shared_ptr<Udp> udp;
    {
        std::lock_guard<std::mutex> lock(channel_mutex_);
        ++channel_generation_;
        probe_channel_.reset();
        udp = std::move(udp_);
        if (probe_timer_ != nullptr) {
            esp_timer_stop(probe_timer_);
            esp_timer_delete(probe_timer_);
            probe_timer_ = nullptr;
        }
    }
    udp.reset();
    mqtt_.reset();

    {
        std::lock_guard<std::mutex> lock(crypto_mutex_);
        if (aes_key_id_ != PSA_KEY_ID_NULL) {
            psa_destroy_key(aes_key_id_);
            aes_key_id_ = PSA_KEY_ID_NULL;
        }
    }

    if (event_group_handle_ != nullptr) {
        vEventGroupDelete(event_group_handle_);
    }
}

bool MqttProtocol::Start() { return StartMqttClient(false); }

bool MqttProtocol::StartMqttClient(bool report_error) {
    if (mqtt_ != nullptr) {
        ESP_LOGW(TAG, "Mqtt client already started");
        mqtt_.reset();
    }

    Settings settings("mqtt", false);
    auto endpoint = settings.GetString("endpoint");
    auto client_id = settings.GetString("client_id");
    auto username = settings.GetString("username");
    auto password = settings.GetString("password");
    int keepalive_interval = settings.GetInt("keepalive", 240);
    publish_topic_ = settings.GetString("publish_topic");

    if (endpoint.empty()) {
        ESP_LOGW(TAG, "MQTT endpoint is not specified");
        if (report_error) {
            SetError(Lang::Strings::SERVER_NOT_FOUND);
        }
        return false;
    }

    auto network = Board::GetInstance().GetNetwork();
    mqtt_ = network->CreateMqtt(0);
    mqtt_->SetKeepAlive(keepalive_interval);

    mqtt_->OnDisconnected([this]() {
        if (on_disconnected_ != nullptr) {
            on_disconnected_();
        }
        ESP_LOGI(TAG, "MQTT disconnected, schedule reconnect in %d seconds",
                 MQTT_RECONNECT_INTERVAL_MS / 1000);
        esp_timer_start_once(reconnect_timer_, MQTT_RECONNECT_INTERVAL_MS * 1000);
    });

    mqtt_->OnConnected([this]() {
        if (on_connected_ != nullptr) {
            on_connected_();
        }
        esp_timer_stop(reconnect_timer_);
    });

    mqtt_->OnMessage([this](const std::string& topic, const std::string& payload) {
        cJSON* root = cJSON_Parse(payload.c_str());
        if (root == nullptr) {
            ESP_LOGE(TAG, "Failed to parse json message %s", payload.c_str());
            return;
        }
        cJSON* type = cJSON_GetObjectItem(root, "type");
        if (!cJSON_IsString(type)) {
            ESP_LOGE(TAG, "Message type is invalid");
            cJSON_Delete(root);
            return;
        }

        if (strcmp(type->valuestring, "hello") == 0) {
            ParseServerHello(root);
        } else if (strcmp(type->valuestring, "goodbye") == 0) {
            auto session_id = cJSON_GetObjectItem(root, "session_id");
            ESP_LOGI(TAG, "Received goodbye message, session_id: %s",
                     cJSON_IsString(session_id) ? session_id->valuestring : "null");
            if (cJSON_IsString(session_id) && session_id_ == session_id->valuestring) {
                auto alive = alive_;  // Capture alive flag
                Application::GetInstance().Schedule([this, alive]() {
                    if (*alive) {
                        // Server initiated goodbye, don't send goodbye back to avoid ping-pong
                        CloseAudioChannel(false);
                    }
                });
            }
        } else if (on_incoming_json_ != nullptr) {
            on_incoming_json_(root);
        }
        cJSON_Delete(root);
        last_incoming_time_ = std::chrono::steady_clock::now();
    });

    std::string broker_address;
    int broker_port = 8883;
    size_t pos = endpoint.find(':');
    if (pos != std::string::npos) {
        broker_address = endpoint.substr(0, pos);
        auto port_str = endpoint.substr(pos + 1);
        int parsed_port = 0;
        auto [ptr, ec] =
            std::from_chars(port_str.data(), port_str.data() + port_str.size(), parsed_port);
        if (ec == std::errc() && ptr == port_str.data() + port_str.size() && parsed_port > 0 &&
            parsed_port <= UINT16_MAX) {
            broker_port = parsed_port;
        } else {
            ESP_LOGW(TAG, "Invalid port in MQTT endpoint \"%s\", using %d", endpoint.c_str(),
                     broker_port);
        }
    } else {
        broker_address = endpoint;
    }
    ESP_LOGI(TAG, "Connecting to endpoint %s:%d", broker_address.c_str(), broker_port);
    if (Board::GetInstance().GetBoardType() == "wifi") {
        struct hostent* server = gethostbyname(broker_address.c_str());
        if (server != nullptr && server->h_addr != nullptr) {
            ESP_LOGI(TAG, "Resolved MQTT host %s -> %s", broker_address.c_str(),
                     inet_ntoa(*reinterpret_cast<struct in_addr*>(server->h_addr)));
        } else {
            ESP_LOGW(TAG, "Failed to resolve MQTT host %s", broker_address.c_str());
        }
    }
    if (auto connected = mqtt_->Connect(broker_address, broker_port, client_id, username, password);
        !connected) {
        ESP_LOGE(TAG, "Failed to connect to endpoint: %s", connected.error().ToString().c_str());
        SetError(Lang::Strings::SERVER_NOT_CONNECTED,
                 broker_address + ":" + std::to_string(broker_port));
        return false;
    }

    ESP_LOGI(TAG, "Connected to endpoint");
    return true;
}

bool MqttProtocol::SendText(const std::string& text) {
    if (publish_topic_.empty()) {
        return false;
    }
    if (!mqtt_->Publish(publish_topic_, text)) {
        ESP_LOGE(TAG, "Failed to publish message: %s", text.c_str());
        SetError(Lang::Strings::SERVER_ERROR);
        return false;
    }
    return true;
}

bool MqttProtocol::SendAudio(std::unique_ptr<AudioStreamPacket> packet) {
    constexpr size_t kAudioHeaderSize = 16;

    // Keep the critical section short: only snapshot the channel state here.
    // udp->Send() below may block waiting for the modem, and the UDP receive
    // callback needs channel_mutex_ on the modem's AT event task.
    std::shared_ptr<Udp> udp;
    std::string nonce;
    uint32_t sequence;
    {
        std::lock_guard<std::mutex> lock(channel_mutex_);
        if (udp_ == nullptr) {
            return false;
        }
        if (aes_nonce_.size() != kAudioHeaderSize || packet->payload.size() > UINT16_MAX) {
            ESP_LOGE(TAG, "Invalid AES nonce or audio payload length: %zu", packet->payload.size());
            return false;
        }
        udp = udp_;
        nonce = aes_nonce_;
        sequence = htonl(++local_sequence_);
    }

    const uint16_t payload_len = htons(static_cast<uint16_t>(packet->payload.size()));
    const uint32_t timestamp = htonl(packet->timestamp);
    memcpy(nonce.data() + 2, &payload_len, sizeof(payload_len));
    memcpy(nonce.data() + 8, &timestamp, sizeof(timestamp));
    memcpy(nonce.data() + 12, &sequence, sizeof(sequence));

    std::string encrypted;
    encrypted.resize(nonce.size() + packet->payload.size());
    memcpy(encrypted.data(), nonce.data(), nonce.size());

    if (!CryptAesCtr(reinterpret_cast<const uint8_t*>(packet->payload.data()),
                     packet->payload.size(), reinterpret_cast<const uint8_t*>(nonce.data()),
                     reinterpret_cast<uint8_t*>(&encrypted[nonce.size()]))) {
        ESP_LOGE(TAG, "Failed to encrypt audio data");
        return false;
    }

    // Send without holding channel_mutex_ (see above).
    return udp->Send(encrypted) > 0;
}

void MqttProtocol::CloseAudioChannel(bool send_goodbye) {
    awaiting_hello_ = false;
    std::shared_ptr<Udp> udp;
    {
        std::lock_guard<std::mutex> lock(channel_mutex_);
        ++channel_generation_;
        probe_channel_.reset();
        if (probe_timer_ != nullptr) {
            esp_timer_stop(probe_timer_);
        }
        udp = std::move(udp_);
    }
    udp.reset();

    ESP_LOGI(TAG, "Closing audio channel, send_goodbye: %d", send_goodbye);

    // Only send goodbye when client initiates the close
    // Don't send if server already sent goodbye (to avoid ping-pong)
    if (send_goodbye) {
        std::string message = "{";
        message += "\"session_id\":\"" + session_id_ + "\",";
        message += "\"type\":\"goodbye\"";
        message += "}";
        SendText(message);
    }

    if (on_audio_channel_closed_ != nullptr) {
        on_audio_channel_closed_();
    }
}

bool MqttProtocol::OpenAudioChannel() {
    if (mqtt_ == nullptr || !mqtt_->IsConnected()) {
        ESP_LOGI(TAG, "MQTT is not connected, try to connect now");
        if (!StartMqttClient(true)) {
            return false;
        }
    }

    error_occurred_ = false;
    session_id_ = "";
    uint64_t generation;
    std::shared_ptr<Udp> previous_udp;
    {
        std::lock_guard<std::mutex> lock(channel_mutex_);
        generation = ++channel_generation_;
        probe_channel_.reset();
        previous_udp = std::move(udp_);
    }
    previous_udp.reset();
    if (probe_timer_ != nullptr) {
        esp_timer_stop(probe_timer_);
    }
    hello_valid_ = false;
    awaiting_hello_ = true;
    xEventGroupClearBits(event_group_handle_, MQTT_PROTOCOL_SERVER_HELLO_EVENT);

    auto message = GetHelloMessage();
    if (!SendText(message)) {
        awaiting_hello_ = false;
        return false;
    }

    // 等待服务器响应
    EventBits_t bits = xEventGroupWaitBits(event_group_handle_, MQTT_PROTOCOL_SERVER_HELLO_EVENT,
                                           pdTRUE, pdFALSE, pdMS_TO_TICKS(10000));
    awaiting_hello_ = false;
    if (!(bits & MQTT_PROTOCOL_SERVER_HELLO_EVENT)) {
        ESP_LOGE(TAG, "Failed to receive server hello");
        SetError(Lang::Strings::SERVER_TIMEOUT);
        return false;
    }

    if (!hello_valid_) {
        SetError(Lang::Strings::SERVER_ERROR, "udp_probe negotiation failed");
        return false;
    }
    auto network = Board::GetInstance().GetNetwork();
    auto udp = network->CreateUdp(2);
    std::shared_ptr<ProbeChannel> probe;
    {
        std::lock_guard<std::mutex> lock(channel_mutex_);
        probe = probe_channel_;
    }
    udp->OnMessage([this, generation, probe](const std::string& data) {
        // Record/claim PONG reception before any channel-lock wait. ReceivePong
        // checks the captured channel identity under that lock, including after
        // close/reopen, so the audio generation gate must not precede it.
        if (!data.empty() && static_cast<uint8_t>(data[0]) == 0x03) {
            ReceivePong(probe, data);
            return;
        }
        {
            std::lock_guard<std::mutex> lock(channel_mutex_);
            if (generation != channel_generation_) {
                return;
            }
        }
        /*
         * UDP Encrypted OPUS Packet Format:
         * |type 1u|flags 1u|payload_len 2u|ssrc 4u|timestamp 4u|sequence 4u|
         * |payload payload_len|
         */
        constexpr size_t kAudioHeaderSize = 16;
        if (data.size() < kAudioHeaderSize) {
            ESP_LOGE(TAG, "Invalid audio packet size: %zu", data.size());
            return;
        }
        if (static_cast<uint8_t>(data[0]) != 0x01) {
            ESP_LOGE(TAG, "Invalid audio packet type: %x", static_cast<uint8_t>(data[0]));
            return;
        }
        uint16_t payload_len = 0;
        uint32_t timestamp = 0;
        uint32_t sequence = 0;
        memcpy(&payload_len, data.data() + 2, sizeof(payload_len));
        memcpy(&timestamp, data.data() + 8, sizeof(timestamp));
        memcpy(&sequence, data.data() + 12, sizeof(sequence));
        payload_len = ntohs(payload_len);
        timestamp = ntohl(timestamp);
        sequence = ntohl(sequence);
        if (data.size() != kAudioHeaderSize + payload_len) {
            ESP_LOGE(TAG, "Audio payload length mismatch: header=%u, datagram=%zu",
                     static_cast<unsigned>(payload_len), data.size() - kAudioHeaderSize);
            return;
        }

        {
            std::lock_guard<std::mutex> lock(channel_mutex_);
            if (sequence <= remote_sequence_) {
                ESP_LOGW(TAG, "Received duplicate/old audio sequence: %lu, last: %lu", sequence,
                         remote_sequence_);
                return;
            }
            if (sequence != remote_sequence_ + 1) {
                ESP_LOGW(TAG, "Received audio packet with wrong sequence: %lu, expected: %lu",
                         sequence, remote_sequence_ + 1);
            }
        }

        const size_t decrypted_size = payload_len;
        auto nonce = reinterpret_cast<const uint8_t*>(data.data());
        auto encrypted = reinterpret_cast<const uint8_t*>(data.data() + kAudioHeaderSize);
        auto packet = std::make_unique<AudioStreamPacket>();
        packet->sample_rate = server_sample_rate_;
        packet->frame_duration = server_frame_duration_;
        packet->timestamp = timestamp;
        packet->payload.resize(decrypted_size);
        if (!CryptAesCtr(encrypted, decrypted_size, nonce,
                         reinterpret_cast<uint8_t*>(packet->payload.data()))) {
            ESP_LOGE(TAG, "Failed to decrypt audio data");
            return;
        }
        {
            std::lock_guard<std::mutex> lock(channel_mutex_);
            if (sequence <= remote_sequence_) {
                return;
            }
            remote_sequence_ = sequence;
        }
        last_incoming_time_ = std::chrono::steady_clock::now();
        if (on_incoming_audio_ != nullptr) {
            on_incoming_audio_(std::move(packet));
        }
    });

    if (auto connected = udp->Connect(udp_server_, udp_port_); !connected) {
        ESP_LOGE(TAG, "Failed to connect UDP audio channel: %s",
                 connected.error().ToString().c_str());
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(channel_mutex_);
        udp_ = std::move(udp);
    }
    if (probe != nullptr) {
        if (probe_timer_ == nullptr) {
            const esp_timer_create_args_t args = {
                .callback =
                    [](void*) {
                        // No protocol pointer escapes: an in-flight callback after close
                        // can only set a coalesced event on the application singleton.
                        Application::GetInstance().NotifyProtocolPending();
                    },
                .name = "udp_probe",
            };
            if (esp_timer_create(&args, &probe_timer_) != ESP_OK) {
                CloseAudioChannel();
                SetError(Lang::Strings::SERVER_ERROR, "udp_probe timer creation failed");
                return false;
            }
        }
        // First PING is sent once at socket readiness; do not wait for PONG.
        ProcessPending();
        if (error_occurred_) {
            return false;
        }
    }
    if (on_audio_channel_opened_ != nullptr) {
        on_audio_channel_opened_();
    }
    return true;
}

std::string MqttProtocol::GetHelloMessage() {
    // 发送 hello 消息申请 UDP 通道
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "hello");
    cJSON_AddNumberToObject(root, "version", 3);
    cJSON_AddStringToObject(root, "transport", "udp");
    AddClientCapabilities(root);
    cJSON* audio_params = cJSON_CreateObject();
    cJSON_AddStringToObject(audio_params, "format", "opus");
    cJSON_AddNumberToObject(audio_params, "sample_rate", 16000);
    cJSON_AddNumberToObject(audio_params, "channels", 1);
    cJSON_AddNumberToObject(audio_params, "frame_duration", OPUS_FRAME_DURATION_MS);
    cJSON_AddItemToObject(root, "audio_params", audio_params);
    auto json_str = cJSON_PrintUnformatted(root);
    std::string message(json_str);
    cJSON_free(json_str);
    cJSON_Delete(root);
    return message;
}

void MqttProtocol::ParseServerHello(const cJSON* root) {
    if (!awaiting_hello_.exchange(false)) {
        return;
    }
    // Always wake the opener, including malformed or unsupported responses.
    struct HelloCompletion {
        EventGroupHandle_t events;
        ~HelloCompletion() { xEventGroupSetBits(events, MQTT_PROTOCOL_SERVER_HELLO_EVENT); }
    } completion{event_group_handle_};
    auto features = cJSON_GetObjectItem(root, "features");
    auto capability = cJSON_GetObjectItem(features, "udp_probe");
    if ((features != nullptr && !cJSON_IsObject(features)) ||
        (capability != nullptr && !cJSON_IsBool(capability))) {
        ESP_LOGE(TAG, "Invalid udp_probe capability");
        return;
    }
    const bool enable_probe = cJSON_IsTrue(capability);
    auto transport = cJSON_GetObjectItem(root, "transport");
    if (!cJSON_IsString(transport) || strcmp(transport->valuestring, "udp") != 0) {
        ESP_LOGE(TAG, "Unsupported or missing transport");
        return;
    }

    auto session_id = cJSON_GetObjectItem(root, "session_id");
    if (cJSON_IsString(session_id)) {
        session_id_ = session_id->valuestring;
        ESP_LOGI(TAG, "Session ID: %s", session_id_.c_str());
    }

    // Get sample rate from hello message
    auto audio_params = cJSON_GetObjectItem(root, "audio_params");
    if (cJSON_IsObject(audio_params)) {
        auto sample_rate = cJSON_GetObjectItem(audio_params, "sample_rate");
        if (cJSON_IsNumber(sample_rate)) {
            server_sample_rate_ = sample_rate->valueint;
        }
        auto frame_duration = cJSON_GetObjectItem(audio_params, "frame_duration");
        if (cJSON_IsNumber(frame_duration)) {
            server_frame_duration_ = frame_duration->valueint;
        }
    }

    auto udp = cJSON_GetObjectItem(root, "udp");
    if (!cJSON_IsObject(udp)) {
        ESP_LOGE(TAG, "UDP is not specified");
        return;
    }
    auto server = cJSON_GetObjectItem(udp, "server");
    auto port = cJSON_GetObjectItem(udp, "port");
    auto key_item = cJSON_GetObjectItem(udp, "key");
    auto nonce_item = cJSON_GetObjectItem(udp, "nonce");
    if (!cJSON_IsString(server) || !cJSON_IsNumber(port) || port->valueint <= 0 ||
        port->valueint > UINT16_MAX || !cJSON_IsString(key_item) || !cJSON_IsString(nonce_item)) {
        ESP_LOGE(TAG, "Invalid UDP server, port, key, or nonce");
        return;
    }
    const std::string udp_server = server->valuestring;
    const int udp_port = port->valueint;

    // auto encryption = cJSON_GetObjectItem(udp, "encryption")->valuestring;
    // ESP_LOGI(TAG, "UDP server: %s, port: %d, encryption: %s", udp_server_.c_str(), udp_port_,
    // encryption);
    std::string aes_nonce;
    std::string aes_key;
    if (!DecodeHexString(nonce_item->valuestring, aes_nonce) ||
        !DecodeHexString(key_item->valuestring, aes_key) || aes_nonce.size() != 16 ||
        aes_key.size() != 16) {
        ESP_LOGE(TAG, "Invalid AES key or nonce length");
        return;
    }

    std::shared_ptr<ProbeChannel> probe;
    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "Failed to initialize PSA Crypto, status: %ld", static_cast<long>(status));
        return;
    }

    if (enable_probe) {
        probe =
            std::make_shared<ProbeChannel>(reinterpret_cast<const uint8_t*>(aes_nonce.data() + 4));
        psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
        psa_set_key_type(&attributes, PSA_KEY_TYPE_HMAC);
        psa_set_key_bits(&attributes, 128);
        psa_set_key_usage_flags(&attributes,
                                PSA_KEY_USAGE_SIGN_MESSAGE | PSA_KEY_USAGE_VERIFY_MESSAGE);
        psa_set_key_algorithm(&attributes,
                              PSA_ALG_TRUNCATED_MAC(PSA_ALG_HMAC(PSA_ALG_SHA_256), 16));
        status = psa_import_key(&attributes, reinterpret_cast<const uint8_t*>(aes_key.data()),
                                aes_key.size(), &probe->key);
        psa_reset_key_attributes(&attributes);
        if (status != PSA_SUCCESS) {
            ESP_LOGE(TAG, "Failed to import UDP HMAC key: %ld", static_cast<long>(status));
            return;
        }
    } else {
        ESP_LOGI(TAG, "Server has no udp_probe support; using legacy UDP channel");
    }
    {
        std::lock_guard<std::mutex> lock(crypto_mutex_);
        if (aes_key_id_ != PSA_KEY_ID_NULL) {
            psa_destroy_key(aes_key_id_);
            aes_key_id_ = PSA_KEY_ID_NULL;
        }

        psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
        psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT);
        psa_set_key_algorithm(&attributes, PSA_ALG_CTR);
        psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
        psa_set_key_bits(&attributes, 128);
        status = psa_import_key(&attributes, reinterpret_cast<const uint8_t*>(aes_key.data()),
                                aes_key.size(), &aes_key_id_);
        psa_reset_key_attributes(&attributes);
    }
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "Failed to import AES key, status: %ld", static_cast<long>(status));
        return;
    }
    {
        std::lock_guard<std::mutex> lock(channel_mutex_);
        probe_channel_ = std::move(probe);
        udp_server_ = udp_server;
        udp_port_ = udp_port;
        aes_nonce_ = std::move(aes_nonce);
        local_sequence_ = 0;
        remote_sequence_ = 0;
    }
    hello_valid_ = true;
}

bool MqttProtocol::CryptAesCtr(const uint8_t* input, size_t input_size, const uint8_t* nonce,
                               uint8_t* output) {
    std::lock_guard<std::mutex> lock(crypto_mutex_);
    if (aes_key_id_ == PSA_KEY_ID_NULL || input == nullptr || nonce == nullptr ||
        output == nullptr) {
        return false;
    }

    psa_cipher_operation_t operation = PSA_CIPHER_OPERATION_INIT;
    psa_status_t status = psa_cipher_encrypt_setup(&operation, aes_key_id_, PSA_ALG_CTR);
    if (status == PSA_SUCCESS) {
        status = psa_cipher_set_iv(&operation, nonce, 16);
    }

    size_t output_len = 0;
    if (status == PSA_SUCCESS) {
        status = psa_cipher_update(&operation, input, input_size, output, input_size, &output_len);
    }

    uint8_t finish_output[16];
    size_t finish_len = 0;
    if (status == PSA_SUCCESS) {
        status = psa_cipher_finish(&operation, finish_output, sizeof(finish_output), &finish_len);
    }
    psa_cipher_abort(&operation);

    if (status != PSA_SUCCESS || output_len != input_size || finish_len != 0) {
        ESP_LOGE(TAG, "AES-CTR operation failed, status: %ld", static_cast<long>(status));
        return false;
    }
    return true;
}

static inline int CharToHex(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

bool MqttProtocol::DecodeHexString(const std::string& hex_string, std::string& decoded) {
    decoded.clear();
    if ((hex_string.size() % 2) != 0) {
        return false;
    }
    decoded.reserve(hex_string.size() / 2);
    for (size_t i = 0; i < hex_string.size(); i += 2) {
        int high = CharToHex(hex_string[i]);
        int low = CharToHex(hex_string[i + 1]);
        if (high < 0 || low < 0) {
            decoded.clear();
            return false;
        }
        decoded.push_back(static_cast<char>((high << 4) | low));
    }
    return true;
}

bool MqttProtocol::IsAudioChannelOpened() const {
    std::lock_guard<std::mutex> lock(channel_mutex_);
    return udp_ != nullptr && !error_occurred_ && !IsTimeout() &&
           (probe_channel_ == nullptr || !probe_channel_->probe.Failed());
}

void MqttProtocol::ReceivePong(const std::shared_ptr<ProbeChannel>& channel,
                               const std::string& data) {
    if (channel == nullptr || data.size() != 32) {
        return;
    }
    int64_t elapsed_us;
    unsigned attempts;
    {
        // Claim reception before taking the state lock so deadline processing cannot
        // overtake an already arriving response while its authentication is pending.
        channel->receiving_pong = true;
        const int64_t now = esp_timer_get_time();
        struct Reception {
            std::atomic<bool>& active;
            ~Reception() { active = false; }
        } reception{channel->receiving_pong};
        // Authentication and timeout completion share the same short critical section.
        std::lock_guard<std::mutex> lock(channel_mutex_);
        if (probe_channel_ != channel ||
            !channel->probe.Matches(reinterpret_cast<const uint8_t*>(data.data()), now)) {
            return;
        }
        const auto algorithm = PSA_ALG_TRUNCATED_MAC(PSA_ALG_HMAC(PSA_ALG_SHA_256), 16);
        if (psa_mac_verify(channel->key, algorithm, reinterpret_cast<const uint8_t*>(data.data()),
                           16, reinterpret_cast<const uint8_t*>(data.data() + 16),
                           16) != PSA_SUCCESS) {
            return;
        }
        elapsed_us = channel->probe.ElapsedUs(now);
        attempts = channel->probe.Attempts();
        channel->probe.Complete(now, esp_random() % (UdpProbe::kIntervalUs / 10 + 1));
        // Timer operations are non-blocking; the receive task never sends a PING.
        ArmProbeTimer(channel->probe.NextActionAt());
    }
    // Serial logging must not hold up channel state or timeout processing.
    if (attempts == 1) {
        ESP_LOGI(TAG, "UDP RTT: %lld.%03lld ms", static_cast<long long>(elapsed_us / 1000),
                 static_cast<long long>(elapsed_us % 1000));
    } else {
        ESP_LOGI(TAG, "UDP RTT: unknown after retransmission (attempts=%u, elapsed=%lld.%03lld ms)",
                 attempts, static_cast<long long>(elapsed_us / 1000),
                 static_cast<long long>(elapsed_us % 1000));
    }
}

void MqttProtocol::ArmProbeTimer(int64_t due) {
    if (probe_timer_ == nullptr) {
        return;
    }
    esp_timer_stop(probe_timer_);
    esp_timer_start_once(probe_timer_, std::max<int64_t>(1, due - esp_timer_get_time()));
}

void MqttProtocol::ProcessPending() {
    // Socket readiness and timer events are the only callers. Both run in the
    // application's existing send context, so no additional sender lock is needed.
    std::shared_ptr<Udp> udp;
    std::string packet;
    bool failed = false;
    bool crypto_error = false;
    {
        std::lock_guard<std::mutex> lock(channel_mutex_);
        const auto channel = probe_channel_;
        if (channel == nullptr || udp_ == nullptr) {
            return;
        }
        const auto now = esp_timer_get_time();
        if (channel->receiving_pong.load()) {
            // Give an already arriving PONG the opportunity to finish validation.
            ArmProbeTimer(now + 10000);
            return;
        }
        auto action = channel->probe.Poll(now);
        failed = action == UdpProbe::Action::Failed;
        if (action == UdpProbe::Action::Send) {
            auto& bytes = channel->probe.Packet();
            size_t length = 0;
            const auto algorithm = PSA_ALG_TRUNCATED_MAC(PSA_ALG_HMAC(PSA_ALG_SHA_256), 16);
            if (psa_mac_compute(channel->key, algorithm, bytes.data(), 16, bytes.data() + 16, 16,
                                &length) != PSA_SUCCESS ||
                length != 16) {
                failed = true;
                crypto_error = true;
            } else {
                udp = udp_;
                packet.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
            }
        }
        if (failed) {
            esp_timer_stop(probe_timer_);
        } else {
            // Arm before Send: a fast PONG may replace this with the next-period timer.
            ArmProbeTimer(channel->probe.NextActionAt());
        }
    }
    if (failed) {
        // Already on the main task; report and close directly, without another Schedule.
        SetError(crypto_error ? Lang::Strings::SERVER_ERROR : Lang::Strings::SERVER_TIMEOUT,
                 crypto_error ? "udp_probe_auth_error" : "udp_probe_timeout");
        CloseAudioChannel();
    } else if (udp != nullptr) {
        // A modem send may wait for AT, but never retain channel_mutex_ or wait for PONG.
        udp->Send(packet);
    }
}
