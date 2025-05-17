#ifndef _IMAGE_RESOURCE_MANAGER_H_
#define _IMAGE_RESOURCE_MANAGER_H_

#include <string>
#include <vector>
#include <stdint.h>
#include "esp_err.h"

// 图片资源管理器单例类
class ImageResourceManager {
public:
    // 获取单例
    static ImageResourceManager& GetInstance() {
        static ImageResourceManager instance;
        return instance;
    }
    
    // 初始化，挂载文件系统
    esp_err_t Initialize();
    
    // 检查并更新图片资源 (返回ESP_OK表示成功)
    esp_err_t CheckAndUpdateResources(const char* api_url, const char* version_url);
    
    // 获取图片数组 (直接返回原始数据指针数组)
    const std::vector<const uint8_t*>& GetImageArray() const;
    
private:
    ImageResourceManager();
    ~ImageResourceManager();
    
    // 私有方法
    esp_err_t MountResourcesPartition();
    std::string ReadLocalVersion();
    bool CheckImagesExist();
    void CreateDirectoryIfNotExists(const char* path);
    esp_err_t CheckServerVersion(const char* version_url);
    esp_err_t DownloadImages(const char* api_url);
    esp_err_t DownloadFile(const char* url, const char* filepath);
    bool SaveVersion(const std::string& version);
    void LoadImageData();
    bool LoadImageFile(int image_index);
    
    // 成员变量
    bool mounted_;           // 分区是否已挂载
    bool initialized_;       // 是否已初始化
    bool has_valid_images_;  // 是否有有效图片
    std::string local_version_;   // 本地版本
    std::string server_version_;  // 服务器版本
    std::vector<const uint8_t*> image_array_; // 图片数据指针数组
    std::vector<void*> image_data_pointers_;  // 管理内存的指针数组
};

#endif // _IMAGE_RESOURCE_MANAGER_H_