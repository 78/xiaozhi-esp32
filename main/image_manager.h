#pragma once

#include <string>
#include <vector>
#include <stdint.h>
#include <functional>
#include "esp_err.h"

/**
 * 图片资源管理器类
 * 负责从网络下载和管理图片资源
 */
class ImageResourceManager {
public:
    // 获取单例实例
    static ImageResourceManager& GetInstance() {
        static ImageResourceManager instance;
        return instance;
    }
    
    // 初始化资源管理器
    esp_err_t Initialize();
    
    // 检查并更新图片资源
    esp_err_t CheckAndUpdateResources(const char* api_url, const char* version_url);
    
    // 检查并更新logo图片（独立版本管理）
    esp_err_t CheckAndUpdateLogo(const char* api_url, const char* logo_version_url);
    
    // 获取图片数组（用于动画）
    const std::vector<const uint8_t*>& GetImageArray() const;
    
    // 获取logo图片（用于静态显示）
    const uint8_t* GetLogoImage() const;
    
    // 设置下载进度回调函数
    using ProgressCallback = std::function<void(int current, int total, const char* message)>;
    void SetDownloadProgressCallback(ProgressCallback callback) {
        progress_callback_ = callback;
    }

private:
    ImageResourceManager();
    ~ImageResourceManager();
    
    // 禁用拷贝构造和赋值
    ImageResourceManager(const ImageResourceManager&) = delete;
    ImageResourceManager& operator=(const ImageResourceManager&) = delete;
    
    // 内部方法
    esp_err_t MountResourcesPartition();
    std::string ReadLocalDynamicUrls(); // 重命名：读取本地动态图片URL
    std::string ReadLocalStaticUrl(); // 重命名：读取本地静态图片URL
    bool CheckImagesExist();
    bool CheckLogoExists(); // 检查logo是否存在
    void CreateDirectoryIfNotExists(const char* path);
    esp_err_t CheckServerVersion(const char* version_url);
    esp_err_t CheckServerLogoVersion(const char* logo_version_url); // 检查服务器logo版本
    esp_err_t DownloadImages(const char* api_url);
    esp_err_t DownloadLogo(const char* api_url); // 下载logo
    esp_err_t DownloadFile(const char* url, const char* filepath);
    bool SaveDynamicUrls(const std::vector<std::string>& urls); // 保存动态图片URL列表
    bool SaveStaticUrl(const std::string& url); // 保存静态图片URL
    void LoadImageData();
    bool LoadImageFile(int image_index);
    bool LoadLogoFile(); // 加载logo文件
    
    // 成员变量
    bool mounted_;           // 分区是否已挂载
    bool initialized_;       // 是否已初始化
    bool has_valid_images_;  // 是否有有效图片
    bool has_valid_logo_;    // 是否有有效logo
    std::string cached_static_url_;  // 缓存的静态图片URL
    std::vector<std::string> cached_dynamic_urls_; // 缓存的动态图片URL列表
    std::string server_static_url_;  // 服务器返回的静态图片URL
    std::vector<std::string> server_dynamic_urls_; // 服务器返回的动态图片URL列表
    std::vector<const uint8_t*> image_array_; // 图片数据指针数组
    std::vector<uint8_t*> image_data_pointers_;  // 管理内存的指针数组
    uint8_t* logo_data_; // logo图片数据
    ProgressCallback progress_callback_ = nullptr; // 进度回调函数
};