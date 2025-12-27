#ifndef MUSIC_H
#define MUSIC_H

#include <string>
#include <vector>

class Music {
public:
    virtual ~Music() = default;  // 添加虚析构函数
    
    virtual bool Download(const std::string& song_name) = 0;
    virtual bool Play() = 0;
    virtual bool Stop() = 0;
    virtual std::string GetDownloadResult() = 0;
    
    // 新增流式播放相关方法
    virtual bool StartStreaming(const std::string& music_url) = 0;
    virtual bool StopStreaming() = 0;  // 停止流式播放
    virtual size_t GetBufferSize() const = 0;
    virtual bool IsDownloading() const = 0;

    virtual bool PlaySdCardMusic(const std::string& file_path) = 0;
    virtual std::vector<std::string> SearchSdCardMusic(const std::string& song_name) = 0;
    
    // 新增：支持歌手+歌名的搜索方法
    virtual std::vector<std::string> SearchSdCardMusicWithArtist(const std::string& song_name, const std::string& artist = "") = 0;

    // 新增：SD卡多曲目连续播放（播完自动播放下一首）
    virtual bool PlaySdCardPlaylist(const std::vector<std::string>& file_paths, bool loop = false) = 0;

    // 新增：播放列表控制（上一曲/下一曲）
    virtual bool NextTrack() = 0;
    virtual bool PrevTrack() = 0;
};

#endif // MUSIC_H 
