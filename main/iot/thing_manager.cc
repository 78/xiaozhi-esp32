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

bool ThingManager::GetStatesJson(std::string& json, bool delta) {
    if (!delta) {
        last_states_.clear();
    }
    bool changed = false;
    json = "[";
    // 枚举thing，获取每个thing的state，如果发生变化，则更新，保存到last_states_
    // 如果delta为true，则只返回变化的部分
    for (auto& thing : things_) {
        std::string state = thing->GetStateJson();
        if (delta) {
            // 如果delta为true，则只返回变化的部分
            auto it = last_states_.find(thing->name());
            if (it != last_states_.end() && it->second == state) {
                continue;
            }
            changed = true;
            last_states_[thing->name()] = state;
        }
        json += state + ",";
    }
    if (json.back() == ',') {
        json.pop_back();
    }
    json += "]";
    return changed;
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
