#include "thing_manager.h"

#include <esp_log.h>

#define TAG "ThingManager"

namespace iot {

void ThingManager::AddThing(Thing* thing) {
    things_.push_back(thing);
}

std::string ThingManager::GetDescriptorsJson() {
    std::string json_str = "[";
    for (auto& thing : things_) {
        json_str += thing->GetDescriptorJson() + ",";
    }
    if (json_str.back() == ',') {
        json_str.pop_back();
    }
    json_str += "]";
    return json_str;
}

std::string ThingManager::GetStatesJson() {
    std::string json_str = "[";
    for (auto& thing : things_) {
        json_str += thing->GetStateJson() + ",";
    }
    if (json_str.back() == ',') {
        json_str.pop_back();
    }
    json_str += "]";
    return json_str;
}

void ThingManager::Invoke(const cJSON* command) {
    auto name = cJSON_GetObjectItem(command, "name");
    for (auto& thing : things_) {
        if (thing->name() == name->valuestring) {
            thing->Invoke(command);
            return;
        }
    }
}

} // namespace iot
