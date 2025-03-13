#include "thing_manager.h" // 包含ThingManager类的头文件

#include <esp_log.h> // 包含ESP日志库，用于日志输出

#define TAG "ThingManager" // 定义日志标签

namespace iot { // 定义命名空间iot

// ThingManager类的成员函数：添加Thing对象到管理器中
// 参数：
// - thing: 指向Thing对象的指针
void ThingManager::AddThing(Thing* thing) {
    things_.push_back(thing); // 将Thing对象添加到things_列表中
}

// ThingManager类的成员函数：获取所有Thing对象的描述符JSON字符串
// 返回值：
// - 返回一个包含所有Thing对象描述符的JSON数组字符串
std::string ThingManager::GetDescriptorsJson() {
    std::string json_str = "["; // 初始化JSON字符串，以数组形式开始
    for (auto& thing : things_) { // 遍历所有Thing对象
        json_str += thing->GetDescriptorJson() + ","; // 将每个Thing对象的描述符JSON字符串添加到结果中
    }
    if (json_str.back() == ',') { // 如果最后一个字符是逗号
        json_str.pop_back(); // 移除多余的逗号
    }
    json_str += "]"; // 结束JSON数组
    return json_str; // 返回最终的JSON字符串
}

// ThingManager类的成员函数：获取所有Thing对象的状态JSON字符串
// 返回值：
// - 返回一个包含所有Thing对象状态的JSON数组字符串
std::string ThingManager::GetStatesJson() {
    std::string json_str = "["; // 初始化JSON字符串，以数组形式开始
    for (auto& thing : things_) { // 遍历所有Thing对象
        json_str += thing->GetStateJson() + ","; // 将每个Thing对象的状态JSON字符串添加到结果中
    }
    if (json_str.back() == ',') { // 如果最后一个字符是逗号
        json_str.pop_back(); // 移除多余的逗号
    }
    json_str += "]"; // 结束JSON数组
    return json_str; // 返回最终的JSON字符串
}

// ThingManager类的成员函数：根据命令调用指定Thing对象的操作
// 参数：
// - command: 指向cJSON对象的指针，表示要执行的命令
void ThingManager::Invoke(const cJSON* command) {
    auto name = cJSON_GetObjectItem(command, "name"); // 从命令中获取"name"字段
    for (auto& thing : things_) { // 遍历所有Thing对象
        if (thing->name() == name->valuestring) { // 如果Thing对象的名称与命令中的名称匹配
            thing->Invoke(command); // 调用该Thing对象的Invoke方法执行命令
            return; // 结束函数
        }
    }
}

} // namespace iot