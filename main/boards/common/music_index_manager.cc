#include "music_index_manager.h"
#include <dirent.h>
#include <sys/stat.h>
#include <cstring>
#include <algorithm>
#include <esp_heap_caps.h>
#include <esp_timer.h>

const char* MusicIndexManager::TAG = "MusicIndex";

MusicIndexManager::MusicIndexManager() 
    : index_data_(nullptr), index_size_(0), max_index_size_(10000), index_built_(false) {
    ESP_LOGI(TAG, "Music index manager initialized");
}

MusicIndexManager::~MusicIndexManager() {
    if (index_data_) {
        heap_caps_free(index_data_);
        index_data_ = nullptr;
    }
    ESP_LOGI(TAG, "Music index manager destroyed");
}

bool MusicIndexManager::BuildIndex() {
    if (index_built_) {
        ESP_LOGW(TAG, "Index already built, size: %u", (unsigned int)index_size_);
        return true;
    }
    
    ESP_LOGI(TAG, "Starting to build music index...");
    int64_t t0 = esp_timer_get_time();
    
    // 构建索引
    if (!ScanSdCardAndBuildIndex()) {
        ESP_LOGE(TAG, "Failed to build music index");
        return false;
    }
    
    index_built_ = true;
    double elapsed_ms = (esp_timer_get_time() - t0) / 1000.0;
    ESP_LOGI(TAG, "Music index built successfully, size: %u, memory: %.1f KB, time: %.2f ms", 
              (unsigned int)index_size_, GetMemoryUsage() / 1024.0, elapsed_ms);
    
    return true;
}

bool MusicIndexManager::ScanSdCardAndBuildIndex() {
    const char* root_path = "/sdcard/";
    
    // 打开SD卡目录
    DIR* dir = opendir(root_path);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open SD card directory: %s", root_path);
        return false;
    }
    
    // 第一遍：统计MP3文件数量
    size_t mp3_count = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_REG) {  // 只处理文件
            std::string filename = entry->d_name;
            std::string ext = filename.substr(filename.find_last_of(".") + 1);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == "mp3") {
                mp3_count++;
            }
        }
    }
    
    if (mp3_count == 0) {
        ESP_LOGW(TAG, "No MP3 files found in SD card");
        closedir(dir);
        return false;
    }
    
    ESP_LOGI(TAG, "Found %u MP3 files, allocating index memory", mp3_count);
    
    // 分配索引内存（使用PSRAM）
    size_t required_memory = mp3_count * sizeof(CompactMusicIndex);
    index_data_ = (CompactMusicIndex*)heap_caps_malloc(
        required_memory, 
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
    );
    
    if (!index_data_) {
        ESP_LOGE(TAG, "Failed to allocate %u bytes in PSRAM for index", required_memory);
        closedir(dir);
        return false;
    }
    
    // 重新打开目录进行索引构建
    closedir(dir);
    dir = opendir(root_path);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to reopen SD card directory");
        heap_caps_free(index_data_);
        index_data_ = nullptr;
        return false;
    }
    
    // 第二遍：构建索引
    index_size_ = 0;
    while ((entry = readdir(dir)) != nullptr && index_size_ < mp3_count) {
        if (entry->d_type == DT_REG) {  // 只处理文件
            std::string filename = entry->d_name;
            std::string ext = filename.substr(filename.find_last_of(".") + 1);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == "mp3") {
                CompactMusicIndex& index = index_data_[index_size_];
                
                // 解析文件名获取歌名和歌手
                std::string song_name, artist;
                if (ParseFileName(filename, song_name, artist)) {
                    // 复制到索引结构
                    strncpy(index.song_name, song_name.c_str(), sizeof(index.song_name) - 1);
                    strncpy(index.artist, artist.c_str(), sizeof(index.artist) - 1);
                    strncpy(index.file_path, (std::string(root_path) + filename).c_str(), sizeof(index.file_path) - 1);
                    
                    index_size_++;
                    ESP_LOGD(TAG, "Indexed: %s - %s (%s)", artist.c_str(), song_name.c_str(), filename.c_str());
                } else {
                    ESP_LOGW(TAG, "Failed to parse filename: %s", filename.c_str());
                }
            }
        }
    }
    
    closedir(dir);
    
    if (index_size_ == 0) {
        ESP_LOGW(TAG, "No valid MP3 files indexed");
        heap_caps_free(index_data_);
        index_data_ = nullptr;
        return false;
    }
    
    ESP_LOGI(TAG, "Successfully indexed %u MP3 files", index_size_);
    return true;
}

bool MusicIndexManager::ParseFileName(const std::string& filename, std::string& song_name, std::string& artist) {
    // 去除扩展名
    std::string name_without_ext = filename.substr(0, filename.find_last_of("."));
    
    // 查找分隔符 "-" 或 " - " 或 "_" 或 " _ "
    size_t pos = name_without_ext.find(" - ");
    if (pos != std::string::npos) {
        artist = name_without_ext.substr(0, pos);
        song_name = name_without_ext.substr(pos + 3);
    } else {
        pos = name_without_ext.find("-");
        if (pos != std::string::npos) {
            artist = name_without_ext.substr(0, pos);
            song_name = name_without_ext.substr(pos + 1);
        } else {
            pos = name_without_ext.find(" _ ");
            if (pos != std::string::npos) {
                artist = name_without_ext.substr(0, pos);
                song_name = name_without_ext.substr(pos + 3);
            } else {
                pos = name_without_ext.find("_");
                if (pos != std::string::npos) {
                    artist = name_without_ext.substr(0, pos);
                    song_name = name_without_ext.substr(pos + 1);
                } else {
                    // 没有分隔符，整个文件名作为歌名，歌手为空
                    song_name = name_without_ext;
                    artist = "未知歌手";
                }
            }
        }
    }
    
    // 清理字符串（去除首尾空格）
    song_name.erase(0, song_name.find_first_not_of(" \t\r\n"));
    song_name.erase(song_name.find_last_not_of(" \t\r\n") + 1);
    artist.erase(0, artist.find_first_not_of(" \t\r\n"));
    artist.erase(artist.find_last_not_of(" \t\r\n") + 1);
    
    // 如果歌名为空，使用文件名
    if (song_name.empty()) {
        song_name = name_without_ext;
    }
    
    return true;
}

std::vector<std::string> MusicIndexManager::Search(const std::string& keyword) {
    if (!index_built_ || !index_data_) {
        ESP_LOGW(TAG, "Index not built, cannot search");
        return {};
    }
    
    std::vector<std::string> results;
    std::string keyword_lower = ToLower(keyword);
    
    ESP_LOGI(TAG, "Searching for: %s", keyword.c_str());
    
    // 在歌名、歌手、文件路径中搜索
    for (size_t i = 0; i < index_size_; i++) {
        const CompactMusicIndex& index = index_data_[i];
        
        if (ToLower(index.song_name).find(keyword_lower) != std::string::npos ||
            ToLower(index.artist).find(keyword_lower) != std::string::npos ||
            ToLower(index.file_path).find(keyword_lower) != std::string::npos) {
            results.push_back(index.file_path);
        }
    }
    
    ESP_LOGI(TAG, "Search completed, found %u results", results.size());
    return results;
}

bool MusicIndexManager::RebuildIndex() {
    ESP_LOGI(TAG, "Rebuilding music index...");
    
    // 释放旧索引
    if (index_data_) {
        heap_caps_free(index_data_);
        index_data_ = nullptr;
    }
    
    index_built_ = false;
    index_size_ = 0;
    
    // 重新构建
    return BuildIndex();
}

size_t MusicIndexManager::GetMemoryUsage() const {
    if (!index_data_) return 0;
    return index_size_ * sizeof(CompactMusicIndex);
}

void MusicIndexManager::PrintMemoryStats() const {
    ESP_LOGI(TAG, "=== Music Index Memory Stats ===");
    ESP_LOGI(TAG, "Index built: %s", index_built_ ? "Yes" : "No");
    ESP_LOGI(TAG, "Index size: %u entries", (unsigned int)index_size_);
    ESP_LOGI(TAG, "Memory usage: %.1f KB", GetMemoryUsage() / 1024.0);
    ESP_LOGI(TAG, "================================");
}

void MusicIndexManager::PrintIndex(size_t start, size_t count) const {
    if (!index_built_ || !index_data_) {
        ESP_LOGW(TAG, "Index not built, cannot print");
        return;
    }
    
    if (start >= index_size_) {
        ESP_LOGW(TAG, "Start %u out of range (size=%u)", (unsigned int)start, (unsigned int)index_size_);
        return;
    }
    
    size_t end = std::min(start + count, index_size_);
    ESP_LOGI(TAG, "=== Music Index Entries [%u..%u) / %u ===", (unsigned int)start, (unsigned int)end, (unsigned int)index_size_);
    for (size_t i = start; i < end; ++i) {
        const CompactMusicIndex& idx = index_data_[i];
        ESP_LOGI(TAG, "#%u | song='%s' | artist='%s' | path='%s'",
                 (unsigned int)i,
                 idx.song_name,
                 idx.artist,
                 idx.file_path);
    }
    ESP_LOGI(TAG, "==========================================");
}

std::string MusicIndexManager::ToLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

std::string MusicIndexManager::GetBaseNameNoExt(const char* path) {
    if (!path) return "";
    
    std::string path_str(path);
    size_t last_slash = path_str.find_last_of("/\\");
    size_t last_dot = path_str.find_last_of(".");
    
    if (last_slash == std::string::npos) {
        // 没有路径分隔符
        if (last_dot == std::string::npos) {
            return path_str; // 没有扩展名
        } else {
            return path_str.substr(0, last_dot);
        }
    } else {
        // 有路径分隔符
        if (last_dot == std::string::npos || last_dot < last_slash) {
            return path_str.substr(last_slash + 1); // 没有扩展名
        } else {
            return path_str.substr(last_slash + 1, last_dot - last_slash - 1);
        }
    }
} 