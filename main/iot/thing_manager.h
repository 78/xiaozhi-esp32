#ifndef THING_MANAGER_H
#define THING_MANAGER_H


#include "thing.h"

#include <cJSON.h>

#include <vector>
#include <memory>
#include <functional>
#include <map>

namespace iot {

class ThingManager {
public:
    static ThingManager& GetInstance() {
        static ThingManager instance;
        return instance;
    }
    ThingManager(const ThingManager&) = delete;
    ThingManager& operator=(const ThingManager&) = delete;

    void AddThing(Thing* thing);

    std::string GetDescriptorsJson();
    bool GetStatesJson(std::string& json, bool delta = false);
    void Invoke(const cJSON* command);

private:
    ThingManager() = default;
    ~ThingManager() = default;

    std::vector<Thing*> things_;
    std::map<std::string, std::string> last_states_;
};


} // namespace iot

#endif // THING_MANAGER_H
