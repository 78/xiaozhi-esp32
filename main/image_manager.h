#pragma once

#include <string>
#include <vector>
#include <stdint.h>
#include <functional>
#include "esp_err.h"

// 下载策略配置
#define ENABLE_SERIAL_DOWNLOAD 1  // 启用串行下载，避免并发网络请求
#define DOWNLOAD_RETRY_BASE_DELAY_MS 5000  // 基础重试延时5秒

// 二进制图片格式相关常量
#define BINARY_IMAGE_MAGIC UINT32_C(0x42494D47)  // "BIMG" 魔数
#define BINARY_IMAGE_VERSION UINT32_C(1)         // 版本号

// 二进制图片文件头结构
struct BinaryImageHeader {
    uint32_t magic;        // 魔数 0x42494D47 ("BIMG")
    uint32_t version;      // 版本号 1
    uint32_t width;        // 图片宽度
    uint32_t height;       // 图片高度
    uint32_t data_size;    // 数据大小（字节）
    uint32_t reserved[3];  // 保留字段，便于未来扩展
};

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
    
    // 图片资源网络功能已移除 - 仅支持本地图片加载
    
    // 获取图片数组（用于动画）
    const std::vector<const uint8_t*>& GetImageArray() const;
    
    // 获取logo图片（用于静态显示）
    const uint8_t* GetLogoImage() const;
    
    // 按需加载指定索引的图片（延迟加载策略）
    bool LoadImageOnDemand(int image_index);
    
    // 检查指定索引的图片是否已加载
    bool IsImageLoaded(int image_index) const;
    
    // 预加载所有剩余图片（在系统初始化完成后调用）
    esp_err_t PreloadRemainingImages();
    
    // 设置下载进度回调函数
    using ProgressCallback = std::function<void(int current, int total, const char* message)>;
    void SetDownloadProgressCallback(ProgressCallback callback) {
        progress_callback_ = callback;
    }
    
    // 设置预加载进度回调函数
    void SetPreloadProgressCallback(ProgressCallback callback) {
        preload_progress_callback_ = callback;
    }
    
    // 调试功能：清理所有损坏的图片文件
    bool ClearAllImageFiles();

private:
    ImageResourceManager();
    ~ImageResourceManager();
    
    // 禁用拷贝构造和赋值
    ImageResourceManager(const ImageResourceManager&) = delete;
    ImageResourceManager& operator=(const ImageResourceManager&) = delete;
    
    // 内部方法 - 仅保留本地文件系统相关功能
    esp_err_t MountResourcesPartition();
    bool CheckImagesExist();
    bool CheckLogoExists(); // 检查logo是否存在
    void CreateDirectoryIfNotExists(const char* path);
    void LoadImageData();
    bool LoadImageFile(int image_index);
    bool LoadLogoFile(); // 加载logo文件
    bool ConvertHFileToBinary(const char* h_filepath, const char* bin_filepath); // 转换.h文件为二进制格式
    bool LoadBinaryImageFile(int image_index); // 从二进制文件加载图片数据
    bool LoadRawImageFile(int image_index, size_t file_size); // 从原始RGB数据文件加载图片
    
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
    
    // 下载进度回调函数
    ProgressCallback progress_callback_;
    
    // 预加载进度回调函数
    ProgressCallback preload_progress_callback_;
};