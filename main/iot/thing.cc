#include "thing.h" // 包含Thing类的头文件
#include "application.h" // 包含Application类的头文件

#include <esp_log.h> // 包含ESP日志库，用于日志输出

#define TAG "Thing" // 定义日志标签

namespace iot { // 定义命名空间iot

// 静态全局变量：用于存储Thing类型的创建函数映射
static std::map<std::string, std::function<Thing*()>>* thing_creators = nullptr;

// 注册Thing类型的创建函数
// 参数：
// - type: Thing类型的名称
// - creator: 创建该类型Thing对象的函数
void RegisterThing(const std::string& type, std::function<Thing*()> creator) {
    if (thing_creators == nullptr) { // 如果映射未初始化
        thing_creators = new std::map<std::string, std::function<Thing*()>>(); // 初始化映射
    }
    (*thing_creators)[type] = creator; // 将类型和创建函数添加到映射中
}

// 根据类型创建Thing对象
// 参数：
// - type: Thing类型的名称
// 返回值：
// - 返回指向新创建的Thing对象的指针，如果类型未找到则返回nullptr
Thing* CreateThing(const std::string& type) {
    auto creator = thing_creators->find(type); // 查找类型的创建函数
    if (creator == thing_creators->end()) { // 如果未找到
        ESP_LOGE(TAG, "Thing type not found: %s", type.c_str()); // 记录错误日志
        return nullptr; // 返回空指针
    }
    return creator->second(); // 调用创建函数并返回新创建的Thing对象
}

// Thing类的成员函数：获取描述符的JSON字符串
// 返回值：
// - 返回描述Thing对象的JSON字符串
std::string Thing::GetDescriptorJson() {
    std::string json_str = "{"; // 初始化JSON字符串
    json_str += "\"name\":\"" + name_ + "\","; // 添加名称字段
    json_str += "\"description\":\"" + description_ + "\","; // 添加描述字段
    json_str += "\"properties\":" + properties_.GetDescriptorJson() + ","; // 添加属性描述
    json_str += "\"methods\":" + methods_.GetDescriptorJson(); // 添加方法描述
    json_str += "}"; // 结束JSON对象
    return json_str; // 返回最终的JSON字符串
}

// Thing类的成员函数：获取状态的JSON字符串
// 返回值：
// - 返回描述Thing对象状态的JSON字符串
std::string Thing::GetStateJson() {
    std::string json_str = "{"; // 初始化JSON字符串
    json_str += "\"name\":\"" + name_ + "\","; // 添加名称字段
    json_str += "\"state\":" + properties_.GetStateJson(); // 添加状态字段
    json_str += "}"; // 结束JSON对象
    return json_str; // 返回最终的JSON字符串
}

// Thing类的成员函数：执行命令
// 参数：
// - command: 指向cJSON对象的指针，表示要执行的命令
void Thing::Invoke(const cJSON* command) {
    auto method_name = cJSON_GetObjectItem(command, "method"); // 从命令中获取方法名称
    auto input_params = cJSON_GetObjectItem(command, "parameters"); // 从命令中获取输入参数

    try {
        auto& method = methods_[method_name->valuestring]; // 查找方法
        for (auto& param : method.parameters()) { // 遍历方法的参数
            auto input_param = cJSON_GetObjectItem(input_params, param.name().c_str()); // 查找输入参数
            if (param.required() && input_param == nullptr) { // 如果参数是必需的但未提供
                throw std::runtime_error("Parameter " + param.name() + " is required"); // 抛出异常
            }
            if (param.type() == kValueTypeNumber) { // 如果参数类型是数字
                param.set_number(input_param->valueint); // 设置参数值
            } else if (param.type() == kValueTypeString) { // 如果参数类型是字符串
                param.set_string(input_param->valuestring); // 设置参数值
            } else if (param.type() == kValueTypeBoolean) { // 如果参数类型是布尔值
                param.set_boolean(input_param->valueint == 1); // 设置参数值
            }
        }

        // 将方法调用调度到应用程序的主循环中执行
        Application::GetInstance().Schedule([&method]() {
            method.Invoke(); // 调用方法
        });
    } catch (const std::runtime_error& e) { // 捕获异常
        ESP_LOGE(TAG, "Method not found: %s", method_name->valuestring); // 记录错误日志
        return; // 结束函数
    }
}

} // namespace iot