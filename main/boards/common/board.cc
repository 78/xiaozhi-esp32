#include "board.h"
#include "system_info.h"
#include "settings.h"
#include "display/no_display.h"
#include "assets/lang_config.h"

#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_chip_info.h>
#include <esp_random.h>

#define TAG "Board"  // 定义日志标签

// Board类的构造函数
Board::Board() {
    // 初始化Settings对象，使用"board"命名空间，并启用自动保存
    Settings settings("board", true);
    
    // 从设置中获取UUID
    uuid_ = settings.GetString("uuid");
    
    // 如果UUID为空，则生成一个新的UUID并保存到设置中
    if (uuid_.empty()) {
        uuid_ = GenerateUuid();
        settings.SetString("uuid", uuid_);
    }
    
    // 打印UUID和SKU信息到日志
    ESP_LOGI(TAG, "UUID=%s SKU=%s", uuid_.c_str(), BOARD_NAME);
}

// 生成UUID的函数
std::string Board::GenerateUuid() {
    // UUID v4 需要 16 字节的随机数据
    uint8_t uuid[16];
    
    // 使用ESP32的硬件随机数生成器填充UUID数组
    esp_fill_random(uuid, sizeof(uuid));
    
    // 设置UUID版本 (版本 4) 和变体位
    uuid[6] = (uuid[6] & 0x0F) | 0x40;    // 版本 4
    uuid[8] = (uuid[8] & 0x3F) | 0x80;    // 变体 1
    
    // 将字节转换为标准的UUID字符串格式
    char uuid_str[37];
    snprintf(uuid_str, sizeof(uuid_str),
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        uuid[0], uuid[1], uuid[2], uuid[3],
        uuid[4], uuid[5], uuid[6], uuid[7],
        uuid[8], uuid[9], uuid[10], uuid[11],
        uuid[12], uuid[13], uuid[14], uuid[15]);
    
    // 返回生成的UUID字符串
    return std::string(uuid_str);
}

// 获取电池电量的函数（未实现）
bool Board::GetBatteryLevel(int &level, bool& charging) {
    return false;
}

// 获取显示器的函数
Display* Board::GetDisplay() {
    static NoDisplay display;  // 使用NoDisplay作为默认显示器
    return &display;
}

// 获取LED的函数
Led* Board::GetLed() {
    static NoLed led;  // 使用NoLed作为默认LED
    return &led;
}

// 获取系统信息的JSON格式字符串
std::string Board::GetJson() {
    // 构建JSON字符串
    std::string json = "{";
    json += "\"version\":2,";
    json += "\"language\":\"" + std::string(Lang::CODE) + "\",";
    json += "\"flash_size\":" + std::to_string(SystemInfo::GetFlashSize()) + ",";
    json += "\"minimum_free_heap_size\":" + std::to_string(SystemInfo::GetMinimumFreeHeapSize()) + ",";
    json += "\"mac_address\":\"" + SystemInfo::GetMacAddress() + "\",";
    json += "\"uuid\":\"" + uuid_ + "\",";
    json += "\"chip_model_name\":\"" + SystemInfo::GetChipModelName() + "\",";
    json += "\"chip_info\":{";

    // 获取芯片信息并添加到JSON中
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    json += "\"model\":" + std::to_string(chip_info.model) + ",";
    json += "\"cores\":" + std::to_string(chip_info.cores) + ",";
    json += "\"revision\":" + std::to_string(chip_info.revision) + ",";
    json += "\"features\":" + std::to_string(chip_info.features);
    json += "},";

    // 获取应用程序信息并添加到JSON中
    json += "\"application\":{";
    auto app_desc = esp_app_get_description();
    json += "\"name\":\"" + std::string(app_desc->project_name) + "\",";
    json += "\"version\":\"" + std::string(app_desc->version) + "\",";
    json += "\"compile_time\":\"" + std::string(app_desc->date) + "T" + std::string(app_desc->time) + "Z\",";
    json += "\"idf_version\":\"" + std::string(app_desc->idf_ver) + "\",";

    // 计算ELF文件的SHA256哈希并添加到JSON中
    char sha256_str[65];
    for (int i = 0; i < 32; i++) {
        snprintf(sha256_str + i * 2, sizeof(sha256_str) - i * 2, "%02x", app_desc->app_elf_sha256[i]);
    }
    json += "\"elf_sha256\":\"" + std::string(sha256_str) + "\"";
    json += "},";

    // 获取分区表信息并添加到JSON中
    json += "\"partition_table\": [";
    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
    while (it) {
        const esp_partition_t *partition = esp_partition_get(it);
        json += "{";
        json += "\"label\":\"" + std::string(partition->label) + "\",";
        json += "\"type\":" + std::to_string(partition->type) + ",";
        json += "\"subtype\":" + std::to_string(partition->subtype) + ",";
        json += "\"address\":" + std::to_string(partition->address) + ",";
        json += "\"size\":" + std::to_string(partition->size);
        json += "},";
        it = esp_partition_next(it);
    }
    json.pop_back(); // 移除最后一个逗号
    json += "],";

    // 获取OTA分区信息并添加到JSON中
    json += "\"ota\":{";
    auto ota_partition = esp_ota_get_running_partition();
    json += "\"label\":\"" + std::string(ota_partition->label) + "\"";
    json += "},";

    // 获取板级信息并添加到JSON中
    json += "\"board\":" + GetBoardJson();

    // 关闭JSON对象
    json += "}";
    return json;
}