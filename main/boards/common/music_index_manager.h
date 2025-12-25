#ifndef MUSIC_INDEX_MANAGER_H
#define MUSIC_INDEX_MANAGER_H

#include <string>
#include <vector>
#include <cstring>
#include <esp_log.h>

// 简化的音乐索引结构，只保留核心字段
struct CompactMusicIndex {
    char song_name[64];      // 歌名，固定64字节
    char artist[48];         // 歌手，固定48字节
    char file_path[128];     // 文件路径，固定128字节
    
    CompactMusicIndex() {
        memset(song_name, 0, sizeof(song_name));
        memset(artist, 0, sizeof(artist));
        memset(file_path, 0, sizeof(file_path));
    }
};

class MusicIndexManager {
public:
    MusicIndexManager();
    ~MusicIndexManager();
    
    // 构建索引
    bool BuildIndex();
    
    // 搜索功能
    std::vector<std::string> Search(const std::string& keyword);
    
    // 索引状态
    bool IsIndexBuilt() const { return index_built_; }
    size_t GetIndexSize() const { return index_size_; }
    size_t GetMemoryUsage() const;
    
    // 重新构建索引
    bool RebuildIndex();
    
    // 调试功能
    void PrintMemoryStats() const;
    void PrintIndex(size_t start = 0, size_t count = 20) const;

private:
    // 扫描SD卡并构建索引
    bool ScanSdCardAndBuildIndex();
    
    // 解析文件名获取歌名和歌手
    bool ParseFileName(const std::string& filename, std::string& song_name, std::string& artist);
    
    // 工具方法
    std::string ToLower(const std::string& str);
    std::string GetBaseNameNoExt(const char* path);

private:
    CompactMusicIndex* index_data_;     // 索引数据指针
    size_t index_size_;                 // 索引大小
    size_t max_index_size_;             // 最大索引大小
    bool index_built_;                  // 索引是否已构建
    
    static const char* TAG;
};

#endif // MUSIC_INDEX_MANAGER_H 