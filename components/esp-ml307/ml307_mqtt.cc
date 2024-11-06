#include "ml307_mqtt.h"
#include <esp_log.h>

static const char *TAG = "Ml307Mqtt";

Ml307Mqtt::Ml307Mqtt(Ml307AtModem& modem, int mqtt_id) : modem_(modem), mqtt_id_(mqtt_id) {
    event_group_handle_ = xEventGroupCreate();

    command_callback_it_ = modem_.RegisterCommandResponseCallback([this](const std::string command, const std::vector<AtArgumentValue>& arguments) {
        if (command == "MQTTURC" && arguments.size() >= 2) {
            if (arguments[1].int_value == mqtt_id_) {
                auto type = arguments[0].string_value;
                if (type == "conn") {
                    connected_ = arguments[2].int_value == 0;
                    xEventGroupSetBits(event_group_handle_, connected_ ? MQTT_CONNECTED_EVENT : MQTT_DISCONNECTED_EVENT);
                    ESP_LOGI(TAG, "MQTT connection state: %s", ErrorToString(arguments[2].int_value).c_str());
                } else if (type == "suback") {
                } else if (type == "publish" && arguments.size() >= 7) {
                    auto topic = arguments[3].string_value;
                    if (arguments[4].int_value == arguments[5].int_value) {
                        if (on_message_callback_) {
                            on_message_callback_(topic, modem_.DecodeHex(arguments[6].string_value));
                        }
                    } else {
                        message_payload_.append(modem_.DecodeHex(arguments[6].string_value));
                        if (message_payload_.size() >= arguments[4].int_value && on_message_callback_) {
                            on_message_callback_(topic, message_payload_);
                            message_payload_.clear();
                        }
                    }
                } else {
                    ESP_LOGI(TAG, "unhandled MQTT event: %s", type.c_str());
                }
            }
        } else if (command == "MQTTSTATE" && arguments.size() == 1) {
            connected_ = arguments[0].int_value != 3;
            xEventGroupSetBits(event_group_handle_, MQTT_INITIALIZED_EVENT);
        }
    });
}

Ml307Mqtt::~Ml307Mqtt() {
    modem_.UnregisterCommandResponseCallback(command_callback_it_);
    vEventGroupDelete(event_group_handle_);
}

void Ml307Mqtt::OnMessage(std::function<void(const std::string& topic, const std::string& payload)> callback) {
    on_message_callback_ = callback;
}

bool Ml307Mqtt::Connect(const std::string broker_address, int broker_port, const std::string client_id, const std::string username, const std::string password) {
    broker_address_ = broker_address;
    broker_port_ = broker_port;
    client_id_ = client_id;
    username_ = username;
    password_ = password;

    // 检查这个 id 是否已经连接
    modem_.Command(std::string("AT+MQTTSTATE=") + std::to_string(mqtt_id_));
    auto bits = xEventGroupWaitBits(event_group_handle_, MQTT_INITIALIZED_EVENT, pdTRUE, pdFALSE, pdMS_TO_TICKS(MQTT_CONNECT_TIMEOUT_MS));
    if (!(bits & MQTT_INITIALIZED_EVENT)) {
        ESP_LOGE(TAG, "Failed to initialize MQTT connection");
        return false;
    }

    // 断开之前的连接
    if (connected_) {
        Disconnect();
        bits = xEventGroupWaitBits(event_group_handle_, MQTT_DISCONNECTED_EVENT, pdTRUE, pdFALSE, pdMS_TO_TICKS(MQTT_CONNECT_TIMEOUT_MS));
        if (!(bits & MQTT_DISCONNECTED_EVENT)) {
            ESP_LOGE(TAG, "Failed to disconnect from previous connection");
            return false;
        }
    }

    if (broker_port_ == 8883) {
        if (!modem_.Command(std::string("AT+MQTTCFG=\"ssl\",") + std::to_string(mqtt_id_) + ",1")) {
            ESP_LOGE(TAG, "Failed to set MQTT to use SSL");
            return false;
        }
    }

    // Set HEX encoding
    if (!modem_.Command("AT+MQTTCFG=\"encoding\"," + std::to_string(mqtt_id_) + ",1,1")) {
        ESP_LOGE(TAG, "Failed to set MQTT to use HEX encoding");
        return false;
    }

    // 创建MQTT连接
    std::string command = "AT+MQTTCONN=" + std::to_string(mqtt_id_) + ",\"" + broker_address_ + "\"," + std::to_string(broker_port_) + ",\"" + client_id_ + "\",\"" + username_ + "\",\"" + password_ + "\"";
    if (!modem_.Command(command)) {
        ESP_LOGE(TAG, "Failed to create MQTT connection");
        return false;
    }

    // 等待连接完成
    bits = xEventGroupWaitBits(event_group_handle_, MQTT_CONNECTED_EVENT | MQTT_DISCONNECTED_EVENT, pdTRUE, pdFALSE, pdMS_TO_TICKS(MQTT_CONNECT_TIMEOUT_MS));
    if (!(bits & MQTT_CONNECTED_EVENT)) {
        ESP_LOGE(TAG, "Failed to connect to MQTT broker");
        return false;
    }

    return true;
}

void Ml307Mqtt::Disconnect() {
    if (!connected_) {
        return;
    }
    modem_.Command(std::string("AT+MQTTDISC=") + std::to_string(mqtt_id_));
}

bool Ml307Mqtt::Publish(const std::string topic, const std::string payload) {
    if (!connected_) {
        return false;
    }
    std::string command = "AT+MQTTPUB=" + std::to_string(mqtt_id_) + ",\"" + topic + "\",0,0,0," + std::to_string(payload.size()) + "," + modem_.EncodeHex(payload);
    return modem_.Command(command);
}

bool Ml307Mqtt::Subscribe(const std::string topic) {
    if (!connected_) {
        return false;
    }
    std::string command = "AT+MQTTSUB=" + std::to_string(mqtt_id_) + ",\"" + topic + "\",0";
    return modem_.Command(command);
}

bool Ml307Mqtt::Unsubscribe(const std::string topic) {
    if (!connected_) {
        return false;
    }
    std::string command = "AT+MQTTUNSUB=" + std::to_string(mqtt_id_) + ",\"" + topic + "\"";
    return modem_.Command(command);
}

std::string Ml307Mqtt::ErrorToString(int error_code) {
    switch (error_code) {
        case 0:
            return "连接成功";
        case 1:
            return "正在重连";
        case 2:
            return "断开：用户主动断开";
        case 3:
            return "断开：拒绝连接（协议版本、标识符、用户名或密码错误）";
        case 4:
            return "断开：服务器断开";
        case 5:
            return "断开：Ping包超时断开";
        case 6:
            return "断开：网络异常断开";
        case 255:
            return "断开：未知错误";
        default:
            return "未知错误";
    }
}
