#include "assets.h"
#include "board.h"
#include "display.h"
#include "application.h"
#include "lvgl_theme.h"
#include "emote_display.h"
#include "expression_emote.h"
#if HAVE_LVGL
#include "display/lcd_display.h"
#include <spi_flash_mmap.h>
#endif

#include <esp_log.h>
#include <esp_timer.h>
#include <cbin_font.h>


#define TAG "Assets"
#define PARTITION_LABEL "assets"

struct mmap_assets_table {
    char asset_name[32];          /*!< Name of the asset */
    uint32_t asset_size;          /*!< Size of the asset */
    uint32_t asset_offset;        /*!< Offset of the asset */
    uint16_t asset_width;         /*!< Width of the asset */
    uint16_t asset_height;        /*!< Height of the asset */
};

Assets::Assets() {
#if HAVE_LVGL
    strategy_ = std::make_unique<Assets::LvglStrategy>();
#else
    strategy_ = std::make_unique<Assets::EmoteStrategy>();
#endif
    // Initialize the partition
    InitializePartition();
}

Assets::~Assets() {
    UnApplyPartition();
}

bool Assets::FindPartition(Assets* assets) {
    assets->partition_ = esp_partition_find_first(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, PARTITION_LABEL);
    if (assets->partition_ == nullptr) {
        ESP_LOGI(TAG, "No assets partition found");
        return false;
    }
    return true;
}

bool Assets::Apply() {
    return strategy_ ? strategy_->Apply(this) : false;
}

bool Assets::InitializePartition() {
    return strategy_ ? strategy_->InitializePartition(this) : false;
}

void Assets::UnApplyPartition() {
    if (strategy_) {
        strategy_->UnApplyPartition(this);
    }
}

bool Assets::GetAssetData(const std::string& name, void*& ptr, size_t& size) {
    return strategy_ ? strategy_->GetAssetData(this, name, ptr, size) : false;
}

bool Assets::LoadSrmodelsFromIndex(Assets* assets, cJSON* root) {
    void* ptr = nullptr;
    size_t size = 0;
    bool need_delete_root = false;

    // If root is not provided, parse index.json
    if (root == nullptr) {
        if (!assets->GetAssetData("index.json", ptr, size)) {
            ESP_LOGE(TAG, "The index.json file is not found");
            return false;
        }

        root = cJSON_ParseWithLength(static_cast<char*>(ptr), size);
        if (root == nullptr) {
            ESP_LOGE(TAG, "The index.json file is not valid");
            return false;
        }
        need_delete_root = true;
    }

    cJSON* srmodels = cJSON_GetObjectItem(root, "srmodels");
    if (cJSON_IsString(srmodels)) {
        std::string srmodels_file = srmodels->valuestring;
        if (assets->GetAssetData(srmodels_file, ptr, size)) {
            if (assets->models_list_ != nullptr) {
                esp_srmodel_deinit(assets->models_list_);
                assets->models_list_ = nullptr;
            }
            assets->models_list_ = srmodel_load(static_cast<uint8_t*>(ptr));
            if (assets->models_list_ != nullptr) {
                auto& app = Application::GetInstance();
                app.GetAudioService().SetModelsList(assets->models_list_);
                if (need_delete_root) {
                    cJSON_Delete(root);
                }
                return true;
            } else {
                ESP_LOGE(TAG, "Failed to load srmodels.bin");
            }
        } else {
            ESP_LOGE(TAG, "The srmodels file %s is not found", srmodels_file.c_str());
        }
    }

    if (need_delete_root) {
        cJSON_Delete(root);
    }
    return false;
}

#if HAVE_LVGL
uint32_t Assets::LvglStrategy::CalculateChecksum(const char* data, uint32_t length) {
    uint32_t checksum = 0;
    for (uint32_t i = 0; i < length; i++) {
        checksum += data[i];
    }
    return checksum & 0xFFFF;
}

bool Assets::LvglStrategy::InitializePartition(Assets* assets) {
    assets->partition_valid_ = false;
    assets_.clear();

    if (!Assets::FindPartition(assets)) {
        return false;
    }

    int free_pages = spi_flash_mmap_get_free_pages(SPI_FLASH_MMAP_DATA);
    uint32_t storage_size = free_pages * 64 * 1024;
    ESP_LOGI(TAG, "The storage free size is %ld KB", storage_size / 1024);
    ESP_LOGI(TAG, "The partition size is %ld KB", assets->partition_->size / 1024);
    if (storage_size < assets->partition_->size) {
        ESP_LOGE(TAG, "The free size %ld KB is less than assets partition required %ld KB", storage_size / 1024, assets->partition_->size / 1024);
        return false;
    }

    esp_err_t err = esp_partition_mmap(assets->partition_, 0, assets->partition_->size, ESP_PARTITION_MMAP_DATA, (const void**)&mmap_root_, &mmap_handle_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mmap assets partition: %s", esp_err_to_name(err));
        return false;
    }

    assets->partition_valid_ = true;

    uint32_t stored_files = *(uint32_t*)(mmap_root_ + 0);
    uint32_t stored_chksum = *(uint32_t*)(mmap_root_ + 4);
    uint32_t stored_len = *(uint32_t*)(mmap_root_ + 8);

    if (stored_len > assets->partition_->size - 12) {
        ESP_LOGD(TAG, "The stored_len (0x%lx) is greater than the partition size (0x%lx) - 12", stored_len, assets->partition_->size);
        return false;
    }

    auto start_time = esp_timer_get_time();
    uint32_t calculated_checksum = CalculateChecksum(mmap_root_ + 12, stored_len);
    auto end_time = esp_timer_get_time();
    ESP_LOGI(TAG, "The checksum calculation time is %d ms", int((end_time - start_time) / 1000));

    if (calculated_checksum != stored_chksum) {
        ESP_LOGE(TAG, "The calculated checksum (0x%lx) does not match the stored checksum (0x%lx)", calculated_checksum, stored_chksum);
        return false;
    }

    checksum_valid_ = true;

    for (uint32_t i = 0; i < stored_files; i++) {
        auto item = (const mmap_assets_table*)(mmap_root_ + 12 + i * sizeof(mmap_assets_table));
        auto asset = Asset{
            .size = static_cast<size_t>(item->asset_size),
            .offset = static_cast<size_t>(12 + sizeof(mmap_assets_table) * stored_files + item->asset_offset)
        };
        assets_[item->asset_name] = asset;
    }
    return checksum_valid_;
}

void Assets::LvglStrategy::UnApplyPartition(Assets* assets) {
    if (mmap_handle_ != 0) {
        esp_partition_munmap(mmap_handle_);
        mmap_handle_ = 0;
        mmap_root_ = nullptr;
    }
    checksum_valid_ = false;
    assets_.clear();
    (void)assets; // Unused parameter
}

bool Assets::LvglStrategy::GetAssetData(Assets* assets, const std::string& name, void*& ptr, size_t& size) {
    auto asset = assets_.find(name);
    if (asset == assets_.end()) {
        return false;
    }
    auto data = (const char*)(mmap_root_ + asset->second.offset);
    if (data[0] != 'Z' || data[1] != 'Z') {
        ESP_LOGE(TAG, "The asset %s is not valid with magic %02x%02x", name.c_str(), data[0], data[1]);
        return false;
    }

    ptr = static_cast<void*>(const_cast<char*>(data + 2));
    size = asset->second.size;
    return true;
}

bool Assets::LvglStrategy::Apply(Assets* assets) {
    void* ptr = nullptr;
    size_t size = 0;
    if (!assets->GetAssetData("index.json", ptr, size)) {
        ESP_LOGE(TAG, "The index.json file is not found");
        return false;
    }

    cJSON* root = cJSON_ParseWithLength(static_cast<char*>(ptr), size);
    if (root == nullptr) {
        ESP_LOGE(TAG, "The index.json file is not valid");
        return false;
    }

    cJSON* version = cJSON_GetObjectItem(root, "version");
    if (cJSON_IsNumber(version)) {
        if (version->valuedouble > 1) {
            ESP_LOGE(TAG, "The assets version %d is not supported, please upgrade the firmware", version->valueint);
            return false;
        }
    }

    Assets::LoadSrmodelsFromIndex(assets, root);

    auto& theme_manager = LvglThemeManager::GetInstance();
    auto light_theme = theme_manager.GetTheme("light");
    auto dark_theme = theme_manager.GetTheme("dark");

    cJSON* font = cJSON_GetObjectItem(root, "text_font");
    if (cJSON_IsString(font)) {
        std::string fonts_text_file = font->valuestring;
        if (assets->GetAssetData(fonts_text_file, ptr, size)) {
            auto text_font = std::make_shared<LvglCBinFont>(ptr);
            if (text_font->font() == nullptr) {
                ESP_LOGE(TAG, "Failed to load fonts.bin");
                return false;
            }
            if (light_theme != nullptr) {
                light_theme->set_text_font(text_font);
            }
            if (dark_theme != nullptr) {
                dark_theme->set_text_font(text_font);
            }
        } else {
            ESP_LOGE(TAG, "The font file %s is not found", fonts_text_file.c_str());
        }
    }

    cJSON* emoji_collection = cJSON_GetObjectItem(root, "emoji_collection");
    if (cJSON_IsArray(emoji_collection)) {
        auto custom_emoji_collection = std::make_shared<EmojiCollection>();
        int emoji_count = cJSON_GetArraySize(emoji_collection);
        for (int i = 0; i < emoji_count; i++) {
            cJSON* emoji = cJSON_GetArrayItem(emoji_collection, i);
            if (cJSON_IsObject(emoji)) {
                cJSON* name = cJSON_GetObjectItem(emoji, "name");
                cJSON* file = cJSON_GetObjectItem(emoji, "file");
                cJSON* eaf = cJSON_GetObjectItem(emoji, "eaf");
                if (cJSON_IsString(name) && cJSON_IsString(file) && (NULL== eaf)) {
                    if (!assets->GetAssetData(file->valuestring, ptr, size)) {
                        ESP_LOGE(TAG, "Emoji %s image file %s is not found", name->valuestring, file->valuestring);
                        continue;
                    }
                    custom_emoji_collection->AddEmoji(name->valuestring, new LvglRawImage(ptr, size));
                }
            }
        }
        if (light_theme != nullptr) {
            light_theme->set_emoji_collection(custom_emoji_collection);
        }
        if (dark_theme != nullptr) {
            dark_theme->set_emoji_collection(custom_emoji_collection);
        }
    }

    cJSON* skin = cJSON_GetObjectItem(root, "skin");
    if (cJSON_IsObject(skin)) {
        cJSON* light_skin = cJSON_GetObjectItem(skin, "light");
        if (cJSON_IsObject(light_skin) && light_theme != nullptr) {
            cJSON* text_color = cJSON_GetObjectItem(light_skin, "text_color");
            cJSON* background_color = cJSON_GetObjectItem(light_skin, "background_color");
            cJSON* background_image = cJSON_GetObjectItem(light_skin, "background_image");
            if (cJSON_IsString(text_color)) {
                light_theme->set_text_color(LvglTheme::ParseColor(text_color->valuestring));
            }
            if (cJSON_IsString(background_color)) {
                light_theme->set_background_color(LvglTheme::ParseColor(background_color->valuestring));
                light_theme->set_chat_background_color(LvglTheme::ParseColor(background_color->valuestring));
            }
            if (cJSON_IsString(background_image)) {
                if (!assets->GetAssetData(background_image->valuestring, ptr, size)) {
                    ESP_LOGE(TAG, "The background image file %s is not found", background_image->valuestring);
                    return false;
                }
                auto background_image = std::make_shared<LvglCBinImage>(ptr);
                light_theme->set_background_image(background_image);
            }
        }
        cJSON* dark_skin = cJSON_GetObjectItem(skin, "dark");
        if (cJSON_IsObject(dark_skin) && dark_theme != nullptr) {
            cJSON* text_color = cJSON_GetObjectItem(dark_skin, "text_color");
            cJSON* background_color = cJSON_GetObjectItem(dark_skin, "background_color");
            cJSON* background_image = cJSON_GetObjectItem(dark_skin, "background_image");
            if (cJSON_IsString(text_color)) {
                dark_theme->set_text_color(LvglTheme::ParseColor(text_color->valuestring));
            }
            if (cJSON_IsString(background_color)) {
                dark_theme->set_background_color(LvglTheme::ParseColor(background_color->valuestring));
                dark_theme->set_chat_background_color(LvglTheme::ParseColor(background_color->valuestring));
            }
            if (cJSON_IsString(background_image)) {
                if (!assets->GetAssetData(background_image->valuestring, ptr, size)) {
                    ESP_LOGE(TAG, "The background image file %s is not found", background_image->valuestring);
                    return false;
                }
                auto background_image = std::make_shared<LvglCBinImage>(ptr);
                dark_theme->set_background_image(background_image);
            }
        }
    }

    auto display = Board::GetInstance().GetDisplay();
    ESP_LOGI(TAG, "Refreshing display theme...");

    auto current_theme = display->GetTheme();
    if (current_theme != nullptr) {
        display->SetTheme(current_theme);
    }

    // Parse hide_subtitle configuration
    cJSON* hide_subtitle = cJSON_GetObjectItem(root, "hide_subtitle");
    if (cJSON_IsBool(hide_subtitle)) {
        bool hide = cJSON_IsTrue(hide_subtitle);
        auto lcd_display = dynamic_cast<LcdDisplay*>(display);
        if (lcd_display != nullptr) {
            lcd_display->SetHideSubtitle(hide);
            ESP_LOGI(TAG, "Set hide_subtitle to %s", hide ? "true" : "false");
        }
    }
    
    cJSON_Delete(root);
    return true;
}
#endif // HAVE_LVGL

bool Assets::EmoteStrategy::InitializePartition(Assets* assets) {
    assets->partition_valid_ = false;

    if (!Assets::FindPartition(assets)) {
        return false;
    }

    esp_err_t ret = ESP_ERR_INVALID_STATE;
    auto display = Board::GetInstance().GetDisplay();
    auto* emote_display = dynamic_cast<emote::EmoteDisplay*>(display);
    if (emote_display && emote_display->GetEmoteHandle() != nullptr) {
        const emote_data_t data = {
            .type = EMOTE_SOURCE_PARTITION,
            .source = {
                .partition_label = PARTITION_LABEL,
            },
            .flags = {
                .mmap_enable = true, //must be true here!!!
            },
        };
        ret = emote_mount_assets(emote_display->GetEmoteHandle(), &data);
    } else {
        ESP_LOGE(TAG, "Emote display is not initialized");
    }
    assets->partition_valid_ = ((ret == ESP_OK) ? true : false);
    return assets->partition_valid_;
}

void Assets::EmoteStrategy::UnApplyPartition(Assets* assets) {
    auto display = Board::GetInstance().GetDisplay();
    auto* emote_display = dynamic_cast<emote::EmoteDisplay*>(display);
    if (emote_display && emote_display->GetEmoteHandle() != nullptr) {
        emote_unmount_assets(emote_display->GetEmoteHandle());
    }
    (void)assets; // Unused parameter
}

bool Assets::EmoteStrategy::GetAssetData(Assets* assets, const std::string& name, void*& ptr, size_t& size) {
    auto display = Board::GetInstance().GetDisplay();
    auto* emote_display = dynamic_cast<emote::EmoteDisplay*>(display);
    if (emote_display && emote_display->GetEmoteHandle() != nullptr) {
        const uint8_t* data = nullptr;
        size_t data_size = 0;
        if (ESP_OK == emote_get_asset_data_by_name(emote_display->GetEmoteHandle(), name.c_str(), &data, &data_size)) {
            ptr = const_cast<void*>(static_cast<const void*>(data));
            size = data_size;
            return true;
        }
        ESP_LOGE(TAG, "Failed to get asset data by name: %s", name.c_str());
        return false;
    }
    (void)assets; // Unused parameter
    return false;
}

bool Assets::EmoteStrategy::Apply(Assets* assets) {
    Assets::LoadSrmodelsFromIndex(assets);

    auto display = Board::GetInstance().GetDisplay();
    auto* emote_display = dynamic_cast<emote::EmoteDisplay*>(display);

    if (emote_display && emote_display->GetEmoteHandle() != nullptr) {
        emote_load_assets(emote_display->GetEmoteHandle());
    }
    return true;
}

bool Assets::Download(std::string url, std::function<void(int progress, size_t speed)> progress_callback) {
    ESP_LOGI(TAG, "Downloading new version of assets from %s", url.c_str());

    // 取消当前资源分区的内存映射
    UnApplyPartition();

    // 下载新的资源文件
    auto network = Board::GetInstance().GetNetwork();
    auto http = network->CreateHttp(0);
    
    if (!http->Open("GET", url)) {
        ESP_LOGE(TAG, "Failed to open HTTP connection");
        return false;
    }

    if (http->GetStatusCode() != 200) {
        ESP_LOGE(TAG, "Failed to get assets, status code: %d", http->GetStatusCode());
        return false;
    }

    size_t content_length = http->GetBodyLength();
    if (content_length == 0) {
        ESP_LOGE(TAG, "Failed to get content length");
        return false;
    }

    if (content_length > partition_->size) {
        ESP_LOGE(TAG, "Assets file size (%u) is larger than partition size (%lu)", content_length, partition_->size);
        return false;
    }

    // 定义扇区大小为4KB（ESP32的标准扇区大小）
    const size_t SECTOR_SIZE = esp_partition_get_main_flash_sector_size();
    
    // 计算需要擦除的扇区数量
    size_t sectors_to_erase = (content_length + SECTOR_SIZE - 1) / SECTOR_SIZE; // 向上取整
    size_t total_erase_size = sectors_to_erase * SECTOR_SIZE;
    
    ESP_LOGI(TAG, "Sector size: %u, content length: %u, sectors to erase: %u, total erase size: %u", 
             SECTOR_SIZE, content_length, sectors_to_erase, total_erase_size);
    
    // 写入新的资源文件到分区，一边erase一边写入
    char buffer[512];
    size_t total_written = 0;
    size_t recent_written = 0;
    size_t current_sector = 0;
    auto last_calc_time = esp_timer_get_time();
    
    while (true) {
        int ret = http->Read(buffer, sizeof(buffer));
        if (ret < 0) {
            ESP_LOGE(TAG, "Failed to read HTTP data: %s", esp_err_to_name(ret));
            return false;
        }

        if (ret == 0) {
            break;
        }

        // 检查是否需要擦除新的扇区
        size_t write_end_offset = total_written + ret;
        size_t needed_sectors = (write_end_offset + SECTOR_SIZE - 1) / SECTOR_SIZE;
        
        // 擦除需要的新扇区
        while (current_sector < needed_sectors) {
            size_t sector_start = current_sector * SECTOR_SIZE;
            size_t sector_end = (current_sector + 1) * SECTOR_SIZE;
            
            // 确保擦除范围不超过分区大小
            if (sector_end > partition_->size) {
                ESP_LOGE(TAG, "Sector end (%u) exceeds partition size (%lu)", sector_end, partition_->size);
                return false;
            }
            
            ESP_LOGD(TAG, "Erasing sector %u (offset: %u, size: %u)", current_sector, sector_start, SECTOR_SIZE);
            esp_err_t err = esp_partition_erase_range(partition_, sector_start, SECTOR_SIZE);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to erase sector %u at offset %u: %s", current_sector, sector_start, esp_err_to_name(err));
                return false;
            }
            
            current_sector++;
        }

        // 写入数据到分区
        esp_err_t err = esp_partition_write(partition_, total_written, buffer, ret);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to write to assets partition at offset %u: %s", total_written, esp_err_to_name(err));
            return false;
        }

        total_written += ret;
        recent_written += ret;

        // 计算进度和速度
        if (esp_timer_get_time() - last_calc_time >= 1000000 || total_written == content_length || ret == 0) {
            size_t progress = total_written * 100 / content_length;
            size_t speed = recent_written; // 每秒的字节数
            ESP_LOGI(TAG, "Progress: %u%% (%u/%u), Speed: %u B/s, Sectors erased: %u", 
                     progress, total_written, content_length, speed, current_sector);
            if (progress_callback) {
                progress_callback(progress, speed);
            }
            last_calc_time = esp_timer_get_time();
            recent_written = 0; // 重置最近写入的字节数
        }
    }
    
    http->Close();

    if (total_written != content_length) {
        ESP_LOGE(TAG, "Downloaded size (%u) does not match expected size (%u)", total_written, content_length);
        return false;
    }

    ESP_LOGI(TAG, "Assets download completed, total written: %u bytes, total sectors erased: %u", 
             total_written, current_sector);

    // 重新初始化资源分区
    if (!InitializePartition()) {
        ESP_LOGE(TAG, "Failed to re-initialize assets partition");
        return false;
    }

    return true;
}
