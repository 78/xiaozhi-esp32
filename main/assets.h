#ifndef ASSETS_H
#define ASSETS_H

#include "emoji_collection.h"

#include <map>
#include <string>
#include <functional>

#include <cJSON.h>
#include <esp_partition.h>
#include <lvgl.h>
#include <model_path.h>


// All combinations of wakenet_model, text_font, emoji_collection can be found from the following url:
// https://github.com/78/xiaozhi-fonts/releases/tag/assets

#define ASSETS_PUHUI_COMMON_14_1                    "none-font_puhui_common_14_1-none.bin"
#define ASSETS_XIAOZHI_WAKENET                      "wn9_nihaoxiaozhi_tts-none-none.bin"
#define ASSETS_XIAOZHI_WAKENET_SMALL                "wn9s_nihaoxiaozhi-none-none.bin"
#define ASSETS_XIAOZHI_PUHUI_COMMON_14_1            "wn9_nihaoxiaozhi_tts-font_puhui_common_14_1-none.bin"
#define ASSETS_XIAOZHI_PUHUI_COMMON_16_4_EMOJI_32   "wn9_nihaoxiaozhi_tts-font_puhui_common_16_4-emojis_32.bin"
#define ASSETS_XIAOZHI_PUHUI_COMMON_16_4_EMOJI_64   "wn9_nihaoxiaozhi_tts-font_puhui_common_16_4-emojis_64.bin"
#define ASSETS_XIAOZHI_PUHUI_COMMON_20_4_EMOJI_64   "wn9_nihaoxiaozhi_tts-font_puhui_common_20_4-emojis_64.bin"
#define ASSETS_XIAOZHI_PUHUI_COMMON_30_4_EMOJI_64   "wn9_nihaoxiaozhi_tts-font_puhui_common_30_4-emojis_64.bin"
#define ASSETS_XIAOZHI_S_PUHUI_COMMON_14_1          "wn9s_nihaoxiaozhi-font_puhui_common_14_1-none.bin"
#define ASSETS_XIAOZHI_S_PUHUI_COMMON_16_4_EMOJI_32 "wn9s_nihaoxiaozhi-font_puhui_common_16_4-emojis_32.bin"
#define ASSETS_XIAOZHI_S_PUHUI_COMMON_20_4_EMOJI_32 "wn9s_nihaoxiaozhi-font_puhui_common_20_4-emojis_32.bin"
#define ASSETS_XIAOZHI_S_PUHUI_COMMON_20_4_EMOJI_64 "wn9s_nihaoxiaozhi-font_puhui_common_20_4-emojis_64.bin"
#define ASSETS_XIAOZHI_S_PUHUI_COMMON_30_4_EMOJI_64 "wn9s_nihaoxiaozhi-font_puhui_common_30_4-emojis_64.bin"

struct Asset {
    size_t size;
    size_t offset;
};

class Assets {
public:
    Assets(std::string default_assets_url);
    ~Assets();

    bool Download(std::string url, std::function<void(int progress, size_t speed)> progress_callback);
    bool Apply();

    inline bool partition_valid() const { return partition_valid_; }
    inline bool checksum_valid() const { return checksum_valid_; }
    inline std::string default_assets_url() const { return default_assets_url_; }

private:
    Assets(const Assets&) = delete;
    Assets& operator=(const Assets&) = delete;

    bool InitializePartition();
    uint32_t CalculateChecksum(const char* data, uint32_t length);
    bool GetAssetData(const std::string& name, void*& ptr, size_t& size);
    lv_color_t ParseColor(const std::string& color);

    const esp_partition_t* partition_ = nullptr;
    esp_partition_mmap_handle_t mmap_handle_ = 0;
    const char* mmap_root_ = nullptr;
    bool partition_valid_ = false;
    bool checksum_valid_ = false;
    std::string default_assets_url_;
    srmodel_list_t* models_list_ = nullptr;
    std::map<std::string, Asset> assets_;
};

#endif
