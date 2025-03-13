#include "settings.h"

#include <esp_log.h>
#include <nvs_flash.h>

#define TAG "Settings"

// Settings类的构造函数
// 参数:
// - ns: 命名空间名称，用于区分不同的NVS存储区域
// - read_write: 是否以读写模式打开命名空间
Settings::Settings(const std::string& ns, bool read_write) : ns_(ns), read_write_(read_write) {
    // 打开NVS命名空间
    nvs_open(ns.c_str(), read_write_ ? NVS_READWRITE : NVS_READONLY, &nvs_handle_);
}

// Settings类的析构函数
Settings::~Settings() {
    // 如果NVS句柄有效
    if (nvs_handle_ != 0) {
        // 如果以写模式打开且数据有修改，提交更改
        if (read_write_ && dirty_) {
            ESP_ERROR_CHECK(nvs_commit(nvs_handle_));
        }
        // 关闭NVS句柄
        nvs_close(nvs_handle_);
    }
}

// 获取字符串类型的配置值
// 参数:
// - key: 配置项的键
// - default_value: 如果键不存在时返回的默认值
// 返回值: 配置项的值，如果键不存在则返回默认值
std::string Settings::GetString(const std::string& key, const std::string& default_value) {
    // 如果NVS句柄无效，返回默认值
    if (nvs_handle_ == 0) {
        return default_value;
    }

    // 获取字符串的长度
    size_t length = 0;
    if (nvs_get_str(nvs_handle_, key.c_str(), nullptr, &length) != ESP_OK) {
        return default_value;
    }

    // 分配内存并读取字符串
    std::string value;
    value.resize(length);
    ESP_ERROR_CHECK(nvs_get_str(nvs_handle_, key.c_str(), value.data(), &length));
    // 去除字符串末尾的空字符
    while (!value.empty() && value.back() == '\0') {
        value.pop_back();
    }
    return value;
}

// 设置字符串类型的配置值
// 参数:
// - key: 配置项的键
// - value: 配置项的值
void Settings::SetString(const std::string& key, const std::string& value) {
    // 如果以写模式打开
    if (read_write_) {
        // 设置字符串值
        ESP_ERROR_CHECK(nvs_set_str(nvs_handle_, key.c_str(), value.c_str()));
        // 标记数据为已修改
        dirty_ = true;
    } else {
        // 如果以只读模式打开，记录警告日志
        ESP_LOGW(TAG, "Namespace %s is not open for writing", ns_.c_str());
    }
}

// 获取整数类型的配置值
// 参数:
// - key: 配置项的键
// - default_value: 如果键不存在时返回的默认值
// 返回值: 配置项的值，如果键不存在则返回默认值
int32_t Settings::GetInt(const std::string& key, int32_t default_value) {
    // 如果NVS句柄无效，返回默认值
    if (nvs_handle_ == 0) {
        return default_value;
    }

    // 读取整数值
    int32_t value;
    if (nvs_get_i32(nvs_handle_, key.c_str(), &value) != ESP_OK) {
        return default_value;
    }
    return value;
}

// 设置整数类型的配置值
// 参数:
// - key: 配置项的键
// - value: 配置项的值
void Settings::SetInt(const std::string& key, int32_t value) {
    // 如果以写模式打开
    if (read_write_) {
        // 设置整数值
        ESP_ERROR_CHECK(nvs_set_i32(nvs_handle_, key.c_str(), value));
        // 标记数据为已修改
        dirty_ = true;
    } else {
        // 如果以只读模式打开，记录警告日志
        ESP_LOGW(TAG, "Namespace %s is not open for writing", ns_.c_str());
    }
}

// 删除指定的配置项
// 参数:
// - key: 配置项的键
void Settings::EraseKey(const std::string& key) {
    // 如果以写模式打开
    if (read_write_) {
        // 删除指定的键
        auto ret = nvs_erase_key(nvs_handle_, key.c_str());
        // 如果键不存在，忽略错误
        if (ret != ESP_ERR_NVS_NOT_FOUND) {
            ESP_ERROR_CHECK(ret);
        }
    } else {
        // 如果以只读模式打开，记录警告日志
        ESP_LOGW(TAG, "Namespace %s is not open for writing", ns_.c_str());
    }
}

// 删除命名空间中的所有配置项
void Settings::EraseAll() {
    // 如果以写模式打开
    if (read_write_) {
        // 删除命名空间中的所有键
        ESP_ERROR_CHECK(nvs_erase_all(nvs_handle_));
    } else {
        // 如果以只读模式打开，记录警告日志
        ESP_LOGW(TAG, "Namespace %s is not open for writing", ns_.c_str());
    }
}