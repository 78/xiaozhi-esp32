#include "thing.h"
#include "application.h"

#include <esp_log.h>

#define TAG "Thing"


namespace iot {

#if CONFIG_IOT_PROTOCOL_XIAOZHI
static std::map<std::string, std::function<Thing*()>>* thing_creators = nullptr;
#endif

void RegisterThing(const std::string& type, std::function<Thing*()> creator) {
#if CONFIG_IOT_PROTOCOL_XIAOZHI
    if (thing_creators == nullptr) {
        thing_creators = new std::map<std::string, std::function<Thing*()>>();
    }
    (*thing_creators)[type] = creator;
#endif
}

Thing* CreateThing(const std::string& type) {
#if CONFIG_IOT_PROTOCOL_XIAOZHI
    auto creator = thing_creators->find(type);
    if (creator == thing_creators->end()) {
        ESP_LOGE(TAG, "Thing type not found: %s", type.c_str());
        return nullptr;
    }
    return creator->second();
#else
    return nullptr;
#endif
}

std::string Thing::GetDescriptorJson() {
    std::string json_str = "{";
    json_str += "\"name\":\"" + name_ + "\",";
    json_str += "\"description\":\"" + description_ + "\",";
    json_str += "\"properties\":" + properties_.GetDescriptorJson() + ",";
    json_str += "\"methods\":" + methods_.GetDescriptorJson();
    json_str += "}";
    return json_str;
}

std::string Thing::GetStateJson() {
    std::string json_str = "{";
    json_str += "\"name\":\"" + name_ + "\",";
    json_str += "\"state\":" + properties_.GetStateJson();
    json_str += "}";
    return json_str;
}

void Thing::Invoke(const cJSON* command) {
    auto method_name = cJSON_GetObjectItem(command, "method");
    auto input_params = cJSON_GetObjectItem(command, "parameters");

    try {
        auto& method = methods_[method_name->valuestring];
        for (auto& param : method.parameters()) {
            auto input_param = cJSON_GetObjectItem(input_params, param.name().c_str());
            if (param.required() && input_param == nullptr) {
                throw std::runtime_error("Parameter " + param.name() + " is required");
            }
            if (param.type() == kValueTypeNumber) {
                param.set_number(input_param->valueint);
            } else if (param.type() == kValueTypeString) {
                param.set_string(input_param->valuestring);
            } else if (param.type() == kValueTypeBoolean) {
                param.set_boolean(input_param->valueint == 1);
            }
        }

        Application::GetInstance().Schedule([&method]() {
            method.Invoke();
        });
    } catch (const std::runtime_error& e) {
        ESP_LOGE(TAG, "Method not found: %s", method_name->valuestring);
        return;
    }
}


} // namespace iot
