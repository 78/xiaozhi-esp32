// choreo_assets.cpp - C/C++ 桥接层
//
// 将官方 Assets::GetAssetData() C++ API 封装为 C 接口，
// 供 choreo.c（纯 C 文件）调用。
//
// 原理：Assets::GetInstance().GetAssetData() 返回 flash 只读指针（mmap），
//       本函数将 flash 数据拷贝到 malloc 缓冲区，保持与原 SPIFFS fread 相同的语义。

#include "choreo_assets.h"
#include "assets.h"
#include <esp_log.h>
#include <string.h>
#include <stdlib.h>

static const char* TAG = "choreo_assets";

/* 去掉 /assets/ 前缀，返回纯文件名指针。
 * Assets::GetAssetData() 期望纯文件名（如 "dance_01.json"），
 * 而舞蹈 JSON 中 "music" 字段使用 "/assets/dance_01.ogg" 格式。 */
static const char* strip_assets_prefix(const char* name) {
    if (!name) return NULL;
    if (strncmp(name, "/assets/", 8) == 0) {
        return name + 8;
    }
    return name;
}

bool choreo_assets_read(const char* name, uint8_t** out_data, size_t* out_size) {
    *out_data = NULL;
    *out_size = 0;

    const char* asset_name = strip_assets_prefix(name);
    if (!asset_name || asset_name[0] == '\0') return false;

    void* flash_ptr = NULL;
    size_t flash_size = 0;

    if (!Assets::GetInstance().GetAssetData(asset_name, flash_ptr, flash_size)) {
        ESP_LOGW(TAG, "Asset not found: %s", asset_name);
        return false;
    }

    /* 拷贝 flash 只读数据到 malloc 缓冲区（调用者负责 free） */
    uint8_t* buf = (uint8_t*)malloc(flash_size);
    if (!buf) {
        ESP_LOGE(TAG, "malloc failed for %s (%u bytes)", asset_name, (unsigned)flash_size);
        return false;
    }

    memcpy(buf, flash_ptr, flash_size);

    *out_data = buf;
    *out_size = flash_size;
    return true;
}
