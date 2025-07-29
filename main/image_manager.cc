#include "image_manager.h"
#include <esp_log.h>
#include <esp_spiffs.h>
#include <esp_http_client.h>
#include <string.h>
#include <sys/stat.h>
#include <cJSON.h>
#include <inttypes.h>
#include <wifi_station.h>
#include "board.h"
#include "system_info.h"  // 新增：包含系统信息头文件
#include <esp_app_format.h>  // 新增：包含应用描述头文件
#include <esp_ota_ops.h>      // 新增：包含OTA操作头文件
#include "application.h"      // 新增：包含应用程序头文件

#define TAG "ImageResManager"
#define IMAGE_URL_CACHE_FILE "/resources/image_urls.json"  // 修改：图片URL缓存文件
#define LOGO_URL_CACHE_FILE "/resources/logo_url.json"     // 修改：logo URL缓存文件
#define IMAGE_BASE_PATH "/resources/images/"
#define LOGO_FILE_PATH "/resources/images/logo.bin"
#define LOGO_FILE_PATH_H "/resources/images/logo.h"
#define MAX_IMAGE_FILES 9   // 固定值：API必须返回9个动态图片，本地图片数量不足9个时会触发重新下载
#define MAX_DOWNLOAD_RETRIES 3  // 设置合理的重试次数为3次

// 添加调试开关，可以通过配置启用
#define DEBUG_IMAGE_FILES 1

ImageResourceManager::ImageResourceManager() {
    mounted_ = false;
    initialized_ = false;
    has_valid_images_ = false;
    has_valid_logo_ = false;  
    logo_data_ = nullptr;
    cached_static_url_ = "";     // 缓存的静态图片URL
    cached_dynamic_urls_.clear(); // 缓存的动态图片URL列表
    progress_callback_ = nullptr; // 初始化下载进度回调
    preload_progress_callback_ = nullptr; // 初始化预加载进度回调
}

ImageResourceManager::~ImageResourceManager() {
    // 释放所有分配的内存
    for (auto ptr : image_data_pointers_) {
        if (ptr) {
            free(ptr);
        }
    }
    image_data_pointers_.clear();
    image_array_.clear();
    
    // 释放logo数据
    if (logo_data_) {
        free(logo_data_);
        logo_data_ = nullptr;
    }
    
    // 卸载文件系统
    if (mounted_) {
        esp_vfs_spiffs_unregister("/resources");
    }
}

esp_err_t ImageResourceManager::Initialize() {
    ESP_LOGI(TAG, "初始化图片资源管理器...");
    
    // 挂载resources分区为SPIFFS文件系统
    esp_err_t err = MountResourcesPartition();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "无法挂载resources分区");
        return err;
    }
    
    // 确保存在图片目录
    CreateDirectoryIfNotExists(IMAGE_BASE_PATH);
    
    // 网络功能已移除 - 跳过URL缓存读取
    
    // 检查是否有有效图片
    has_valid_images_ = CheckImagesExist();
    has_valid_logo_ = CheckLogoExists();
    
    if (has_valid_images_) {
        ESP_LOGI(TAG, "找到有效的动画图片文件");
    } else {
        ESP_LOGI(TAG, "未找到有效的动画图片文件");
    }

    if (has_valid_logo_) {
        ESP_LOGI(TAG, "找到有效的logo文件");
    } else {
        ESP_LOGI(TAG, "未找到有效的logo文件");
    }
    
    // 加载现有数据
    if (has_valid_images_ || has_valid_logo_) {
        LoadImageData();
    }
    
    initialized_ = true;
    return ESP_OK;
}

esp_err_t ImageResourceManager::MountResourcesPartition() {
    if (mounted_) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "准备挂载resources分区...");
    
    // 检查分区是否存在
    const esp_partition_t* partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "resources");
    if (partition == NULL) {
        ESP_LOGE(TAG, "未找到resources分区");
        return ESP_ERR_NOT_FOUND;
    }
    
    ESP_LOGI(TAG, "找到resources分区，大小: %u字节 (%.1fMB)", (unsigned int)partition->size, partition->size / (1024.0 * 1024.0));
    
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/resources",
        .partition_label = "resources", 
        .max_files = 30,                    // 增加最大文件数
        .format_if_mount_failed = true
    };
    
    ESP_LOGI(TAG, "开始挂载SPIFFS分区，如需格式化可能需要30-60秒...");
    
    // 记录开始时间
    uint32_t start_time = esp_timer_get_time() / 1000;
    
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "挂载resources分区失败 (%s)", esp_err_to_name(ret));
        return ret;
    }
    
    // 记录结束时间
    uint32_t end_time = esp_timer_get_time() / 1000;
    uint32_t duration = end_time - start_time;
    
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "获取SPIFFS信息失败");
        return ret;
    }
    
    ESP_LOGI(TAG, "resources分区挂载成功! 耗时: %u毫秒, 总大小: %u字节, 已使用: %u字节", 
             (unsigned int)duration, (unsigned int)total, (unsigned int)used);
    mounted_ = true;
    return ESP_OK;
}

// ReadLocalDynamicUrls 方法已移除 - 网络功能已禁用

// ReadLocalStaticUrl 方法已移除 - 网络功能已禁用

bool ImageResourceManager::CheckImagesExist() {
    if (!mounted_) {
        return false;
    }
    
    // 扫描本地实际存在的图片文件数量
    int local_file_count = 0;
    for (int i = 1; i <= MAX_IMAGE_FILES; i++) {
        char filename[128];
        snprintf(filename, sizeof(filename), "%soutput_%04d.bin", IMAGE_BASE_PATH, i);
        
        FILE* f = fopen(filename, "r");
        if (f != NULL) {
            fclose(f);
            local_file_count++;
        } else {
            break; // 文件不连续时停止计数
        }
    }
    
    ESP_LOGI(TAG, "本地动画图片文件数量: %d，期望数量: %d", local_file_count, MAX_IMAGE_FILES);
    
    // 严格检查：必须有9个动态图片才认为有效
    if (local_file_count < MAX_IMAGE_FILES) {
        ESP_LOGW(TAG, "本地动画图片数量不足（%d < %d），需要重新下载", local_file_count, MAX_IMAGE_FILES);
        return false;
    }
    
    // 如果有缓存的URL，还要检查URL数量是否匹配
    if (!cached_dynamic_urls_.empty()) {
        if (cached_dynamic_urls_.size() != MAX_IMAGE_FILES) {
            ESP_LOGW(TAG, "缓存的URL数量（%zu）与期望数量（%d）不匹配，需要重新下载", 
                    cached_dynamic_urls_.size(), MAX_IMAGE_FILES);
            return false;
        }
        
        ESP_LOGI(TAG, "本地图片文件和URL缓存数量都正确");
    } else {
        ESP_LOGI(TAG, "本地图片文件数量正确，但没有URL缓存");
    }
    
    return true;
}

bool ImageResourceManager::CheckLogoExists() {
    if (!mounted_) {
        return false;
    }
    
    // 优先检查二进制格式logo文件
    FILE* f = fopen(LOGO_FILE_PATH, "rb");
    if (f != NULL) {
        fclose(f);
        ESP_LOGI(TAG, "找到二进制logo文件: %s", LOGO_FILE_PATH);
        return true;
    }
    
    // 检查.h格式logo文件（向后兼容）
    f = fopen(LOGO_FILE_PATH_H, "r");
    if (f != NULL) {
        fclose(f);
        ESP_LOGI(TAG, "找到.h格式logo文件: %s", LOGO_FILE_PATH_H);
        return true;
    }
    
    ESP_LOGW(TAG, "未找到任何格式的logo文件");
    return false;
}

void ImageResourceManager::CreateDirectoryIfNotExists(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        ESP_LOGI(TAG, "创建目录: %s", path);
        
        // 递归创建目录
        char tmp[256];
        char* p = NULL;
        size_t len;
        
        snprintf(tmp, sizeof(tmp), "%s", path);
        len = strlen(tmp);
        if (tmp[len - 1] == '/') {
            tmp[len - 1] = 0;
        }
        
        for (p = tmp + 1; *p; p++) {
            if (*p == '/') {
                *p = 0;
                mkdir(tmp, 0755);
                *p = '/';
            }
        }
        
        mkdir(tmp, 0755);
    }
}

// CheckServerVersion 方法已移除 - 网络功能已禁用

// CheckServerLogoVersion 方法已移除 - 网络功能已禁用

// DownloadImages 方法已移除 - 网络功能已禁用

// DownloadLogo 方法已移除 - 网络功能已禁用

// DownloadFile 方法已移除 - 网络功能已禁用 (第1部分)
// DownloadFile 方法已移除 - 网络功能已禁用 (第2部分)

bool ImageResourceManager::ConvertHFileToBinary(const char* h_filepath, const char* bin_filepath) {
    ESP_LOGI(TAG, "开始转换.h文件为二进制格式: %s -> %s", h_filepath, bin_filepath);
    
    FILE* f = fopen(h_filepath, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "无法打开.h文件: %s", h_filepath);
        return false;
    }
    
    // 获取文件大小
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    // 分配文本缓冲区
    char* text_buffer = (char*)malloc(file_size + 1);
    if (text_buffer == NULL) {
        ESP_LOGE(TAG, "内存分配失败");
        fclose(f);
        return false;
    }
    
    // 读取整个文件
    size_t read_size = fread(text_buffer, 1, file_size, f);
    text_buffer[read_size] = '\0';
    fclose(f);
    
    // 解析.h文件获取图片数据
    const char* array_pattern = "const unsigned char";
    char* array_start = strstr(text_buffer, array_pattern);
    if (!array_start) {
        ESP_LOGE(TAG, "未找到数组声明");
        free(text_buffer);
        return false;
    }
    
    // 查找数组大小
    char* size_start = strstr(array_start, "[");
    char* size_end = strstr(size_start, "]");
    if (!size_start || !size_end) {
        ESP_LOGE(TAG, "未找到数组大小");
        free(text_buffer);
        return false;
    }
    
    // 提取数组大小
    char size_str[32] = {0};
    strncpy(size_str, size_start + 1, size_end - size_start - 1);
    int array_size = atoi(size_str);
    
    if (array_size <= 0 || array_size > 200000) { // 安全检查
        ESP_LOGE(TAG, "数组大小无效: %d", array_size);
        free(text_buffer);
        return false;
    }
    
    // 查找数据开始的位置
    char* data_start = strstr(size_end, "{");
    if (!data_start) {
        ESP_LOGE(TAG, "未找到数组数据");
        free(text_buffer);
        return false;
    }
    data_start++; // 跳过 '{'
    
    // 分配图像数据缓冲区
    uint8_t* img_buffer = (uint8_t*)malloc(array_size);
    if (!img_buffer) {
        ESP_LOGE(TAG, "图像数据内存分配失败");
        free(text_buffer);
        return false;
    }
    
    // 解析十六进制数据（使用优化的解析逻辑）
    static const int hex_values[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
         0, 1, 2, 3, 4, 5, 6, 7, 8, 9,-1,-1,-1,-1,-1,-1,  // 0-9
        -1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1,  // A-F
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1,  // a-f
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    };
    
    char* p = data_start;
    char* text_end = text_buffer + read_size;
    int index = 0;
    
    while (p < text_end - 5 && index < array_size) {
        char* next_zero = (char*)memchr(p, '0', text_end - p);
        if (!next_zero || next_zero >= text_end - 3) break;
        
        p = next_zero;
        
        if (*(p+1) == 'x' || *(p+1) == 'X') {
            p += 2;
            
            int high = hex_values[(unsigned char)*p];
            int low = hex_values[(unsigned char)*(p+1)];
            
            if ((high | low) >= 0) {
                unsigned char value = (high << 4) | low;
                
                // 字节序交换逻辑
                if (index & 1) {
                    if (index > 0) {
                        unsigned char temp = img_buffer[index-1];
                        img_buffer[index-1] = value;
                        img_buffer[index] = temp;
                    } else {
                        img_buffer[index] = value;
                    }
                } else {
                    img_buffer[index] = value;
                }
                
                index++;
                p += 2;
            } else {
                p++;
            }
        } else {
            p++;
        }
    }
    
    free(text_buffer);
    
    // 验证解析结果
    if (index < array_size) {
        ESP_LOGW(TAG, "解析的数据不完整: %d/%d 字节", index, array_size);
    }
    
    // 创建二进制文件头
    BinaryImageHeader header = {
        .magic = BINARY_IMAGE_MAGIC,
        .version = BINARY_IMAGE_VERSION,
        .width = 240,  // 固定宽度，可以根据实际情况调整
        .height = 240, // 固定高度，可以根据实际情况调整
        .data_size = (uint32_t)index,
        .reserved = {0, 0, 0}
    };
    
    // 写入二进制文件
    FILE* bin_file = fopen(bin_filepath, "wb");
    if (bin_file == NULL) {
        ESP_LOGE(TAG, "无法创建二进制文件: %s", bin_filepath);
        free(img_buffer);
        return false;
    }
    
    // 写入文件头
    if (fwrite(&header, sizeof(BinaryImageHeader), 1, bin_file) != 1) {
        ESP_LOGE(TAG, "写入文件头失败");
        fclose(bin_file);
        free(img_buffer);
        return false;
    }
    
    // 写入图像数据
    if (fwrite(img_buffer, 1, index, bin_file) != index) {
        ESP_LOGE(TAG, "写入图像数据失败");
        fclose(bin_file);
        free(img_buffer);
        return false;
    }
    
    fclose(bin_file);
    free(img_buffer);
    
    ESP_LOGI(TAG, "成功转换为二进制格式: %d 字节数据", index);
    return true;
}

bool ImageResourceManager::LoadBinaryImageFile(int image_index) {
    char filename[128];
    snprintf(filename, sizeof(filename), "%soutput_%04d.bin", IMAGE_BASE_PATH, image_index);
    
    FILE* f = fopen(filename, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "无法打开二进制文件: %s", filename);
        return false;
    }
    
    // 获取文件大小用于调试
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
#if DEBUG_IMAGE_FILES
    // 添加调试信息：显示文件的前16字节
    uint8_t debug_bytes[16];
    size_t debug_read = fread(debug_bytes, 1, 16, f);
    ESP_LOGI(TAG, "调试文件 %s (大小:%ld字节):", filename, file_size);
    ESP_LOGI(TAG, "前16字节: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x", 
             debug_bytes[0], debug_bytes[1], debug_bytes[2], debug_bytes[3],
             debug_bytes[4], debug_bytes[5], debug_bytes[6], debug_bytes[7],
             debug_bytes[8], debug_bytes[9], debug_bytes[10], debug_bytes[11],
             debug_bytes[12], debug_bytes[13], debug_bytes[14], debug_bytes[15]);
    
    // 检查文件格式：优先检查是否为原始RGB565数据
    if (debug_read >= 4) {
        bool has_binary_header = false;
        
        // 检查是否有正确的BIMG魔数（小端序）
        if (debug_bytes[0] == 0x47 && debug_bytes[1] == 0x4D && 
            debug_bytes[2] == 0x49 && debug_bytes[3] == 0x42) {
            has_binary_header = true;
            ESP_LOGI(TAG, "检测到二进制文件头格式");
        }
        
        // 检查文件大小是否符合原始RGB565格式 (115200字节)
        const size_t rgb565_size = 240 * 240 * 2;
        bool matches_rgb565_size = (file_size == rgb565_size);
        
        // 根据服务端文档，优先按原始RGB565处理
        if (matches_rgb565_size && !has_binary_header) {
            ESP_LOGI(TAG, "检测到标准RGB565格式文件 (240x240, 115200字节)");
            fclose(f);
            return LoadRawImageFile(image_index, file_size);
        } else if (!has_binary_header) {
            ESP_LOGW(TAG, "文件大小不匹配RGB565标准，尝试作为原始数据加载");
            fclose(f);
            return LoadRawImageFile(image_index, file_size);
        }
    }
    
    // 重置文件指针到开头
    fseek(f, 0, SEEK_SET);
#endif
    
    // 读取文件头
    BinaryImageHeader header;
    if (fread(&header, sizeof(BinaryImageHeader), 1, f) != 1) {
        ESP_LOGE(TAG, "读取文件头失败: %s", filename);
        fclose(f);
        return false;
    }
    
    // 验证魔数
    if (header.magic != BINARY_IMAGE_MAGIC) {
        ESP_LOGE(TAG, "文件魔数无效: 0x%08" PRIX32 ", 期望: 0x%08" PRIX32, header.magic, BINARY_IMAGE_MAGIC);
        
#if DEBUG_IMAGE_FILES
        ESP_LOGE(TAG, "尝试删除损坏的文件: %s", filename);
        fclose(f);
        if (remove(filename) == 0) {
            ESP_LOGI(TAG, "成功删除损坏的文件，将在下次检查时重新下载");
        }
        return false;
#else
        fclose(f);
        return false;
#endif
    }
    
    // 验证版本
    if (header.version != BINARY_IMAGE_VERSION) {
        ESP_LOGE(TAG, "文件版本不支持: %" PRIu32 ", 期望: %" PRIu32, header.version, BINARY_IMAGE_VERSION);
        fclose(f);
        return false;
    }
    
    // 验证数据大小
    if (header.data_size == 0 || header.data_size > 200000) {
        ESP_LOGE(TAG, "数据大小无效: %" PRIu32, header.data_size);
        fclose(f);
        return false;
    }
    
    // 分配图像数据缓冲区
    uint8_t* img_buffer = (uint8_t*)malloc(header.data_size);
    if (!img_buffer) {
        ESP_LOGE(TAG, "图像数据内存分配失败: %" PRIu32 " 字节", header.data_size);
        fclose(f);
        return false;
    }
    
    // 直接读取图像数据（高速I/O）
    size_t read_size = fread(img_buffer, 1, header.data_size, f);
    fclose(f);
    
    if (read_size != header.data_size) {
        ESP_LOGE(TAG, "读取图像数据不完整: %zu/%" PRIu32 " 字节", read_size, header.data_size);
        free(img_buffer);
        return false;
    }
    
    // 保存到数组
    int array_index = image_index - 1;
    if (array_index >= 0 && array_index < image_array_.size()) {
        // 释放旧数据
        if (image_data_pointers_[array_index]) {
            free(image_data_pointers_[array_index]);
        }
        
        // 设置新数据
        image_data_pointers_[array_index] = img_buffer;
        image_array_[array_index] = img_buffer;
        
            ESP_LOGI(TAG, "成功加载二进制图片 %d: 大小 %" PRIu32 " 字节 (尺寸: %" PRIu32 "x%" PRIu32 ")", 
            image_index, header.data_size, header.width, header.height);
    return true;
} else {
    ESP_LOGE(TAG, "图片索引超出范围: %d", array_index);
    free(img_buffer);
    return false;
}
}

bool ImageResourceManager::LoadRawImageFile(int image_index, size_t file_size) {
    char filename[128];
    snprintf(filename, sizeof(filename), "%soutput_%04d.bin", IMAGE_BASE_PATH, image_index);
    
    ESP_LOGI(TAG, "尝试作为原始RGB数据加载文件: %s (大小: %zu字节)", filename, file_size);
    
    // 检查文件大小是否符合RGB565格式（240x240x2 = 115200字节）
    const size_t expected_size = 240 * 240 * 2;  // 115200字节
    if (file_size != expected_size) {
        ESP_LOGW(TAG, "文件大小 %zu 不符合标准 RGB565 格式大小 %zu", file_size, expected_size);
        // 仍然尝试加载，可能是不同分辨率
    } else {
        ESP_LOGI(TAG, "文件大小匹配标准RGB565格式: %zu字节 (240x240)", file_size);
    }
    
    FILE* f = fopen(filename, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "无法打开原始数据文件: %s", filename);
        return false;
    }
    
    // 分配缓冲区
    uint8_t* img_buffer = (uint8_t*)malloc(file_size);
    if (!img_buffer) {
        ESP_LOGE(TAG, "内存分配失败: %zu 字节", file_size);
        fclose(f);
        return false;
    }
    
    // 读取整个文件
    size_t read_size = fread(img_buffer, 1, file_size, f);
    fclose(f);
    
    if (read_size != file_size) {
        ESP_LOGE(TAG, "读取原始数据失败: 期望 %zu 字节，实际读取 %zu 字节", file_size, read_size);
        free(img_buffer);
        return false;
    }
    
    // 保存到数组
    int array_index = image_index - 1;
    if (array_index >= 0 && array_index < image_array_.size()) {
        // 释放旧数据
        if (image_data_pointers_[array_index]) {
            free(image_data_pointers_[array_index]);
        }
        
        // 设置新数据
        image_data_pointers_[array_index] = img_buffer;
        image_array_[array_index] = img_buffer;
        
        ESP_LOGI(TAG, "成功加载原始RGB数据 %d: 大小 %zu 字节", image_index, file_size);
        return true;
    } else {
        ESP_LOGE(TAG, "图片索引超出范围: %d", array_index);
        free(img_buffer);
        return false;
    }
}

// SaveDynamicUrls 和 SaveStaticUrl 方法已移除 - 网络功能已禁用

void ImageResourceManager::LoadImageData() {
    // 检查可用内存
    size_t free_heap = esp_get_free_heap_size();
    ESP_LOGI(TAG, "加载图片前可用内存: %u字节", (unsigned int)free_heap);
    
    if (free_heap < 150000) { // 从200KB减少到150KB，降低内存门槛
        ESP_LOGW(TAG, "内存不足，跳过图片加载，可用内存: %u字节", (unsigned int)free_heap);
        return;
    }
    
    // 清空原有动画图片数据
    for (auto ptr : image_data_pointers_) {
        if (ptr) {
            free(ptr);
        }
    }
    image_data_pointers_.clear();
    image_array_.clear();
    
    // 释放原有logo数据
    if (logo_data_) {
        free(logo_data_);
        logo_data_ = nullptr;
    }
    
    // 优先加载logo文件（最高优先级，确保快速显示）
    if (has_valid_logo_ && !LoadLogoFile()) {
        ESP_LOGE(TAG, "加载logo文件失败");
    }
    
    // 优化启动加载策略：快速启动，延迟加载
    if (has_valid_images_) {
        // 根据缓存的URL数量确定要加载的图片数量，如果缓存为空则扫描本地文件
        int actual_image_count = std::min((int)cached_dynamic_urls_.size(), MAX_IMAGE_FILES);
        
        // 如果缓存为空，扫描本地实际存在的文件数量
        if (actual_image_count == 0) {
            ESP_LOGI(TAG, "URL缓存为空，扫描本地图片文件数量...");
            for (int i = 1; i <= MAX_IMAGE_FILES; i++) {
                char filename[128];
                snprintf(filename, sizeof(filename), "%soutput_%04d.bin", IMAGE_BASE_PATH, i);
                FILE* f = fopen(filename, "rb");
                if (f != NULL) {
                    fclose(f);
                    actual_image_count++;
                } else {
                    break; // 文件不连续时停止计数
                }
            }
            ESP_LOGI(TAG, "发现本地图片文件数量: %d", actual_image_count);
        }
        
        // 验证图片数量是否符合期望
        if (actual_image_count < MAX_IMAGE_FILES) {
            ESP_LOGW(TAG, "本地图片数量不足（%d < %d），标记为无效图片资源", actual_image_count, MAX_IMAGE_FILES);
            has_valid_images_ = false;
            
            // 清空图片数组，等待重新下载
            image_array_.clear();
            image_data_pointers_.clear();
            ESP_LOGI(TAG, "图片数量不足，已清空图片数组");
            return;
        }
        
        // 预分配空间
        image_array_.resize(actual_image_count);
        image_data_pointers_.resize(actual_image_count, nullptr);
        
        // **优化：启动时立即加载前两张图片**，确保快速显示和基本动画
        if (actual_image_count > 0) {
            ESP_LOGI(TAG, "优化启动策略：立即加载前两张关键图片，其余图片异步预加载");
            
            // 加载第一张图片（静态显示用）
            if (!LoadImageFile(1)) {
                ESP_LOGE(TAG, "加载第一张动画图片失败，索引: 1");
            }
            
            // 如果有第二张图片，也立即加载（基本动画用）
            if (actual_image_count > 1) {
                if (!LoadImageFile(2)) {
                    ESP_LOGW(TAG, "加载第二张动画图片失败，索引: 2");
                }
            }
        }
        
        ESP_LOGI(TAG, "优化预加载策略：已预分配 %d 个图片槽位，立即加载了关键图片", actual_image_count);
    }
    
    if (has_valid_logo_) {
        ESP_LOGI(TAG, "logo文件已快速加载");
    }
    
    // 最终内存检查
    free_heap = esp_get_free_heap_size();
    ESP_LOGI(TAG, "优化加载完成，剩余内存: %u字节", (unsigned int)free_heap);
}

bool ImageResourceManager::LoadLogoFile() {
    // 首先尝试加载二进制格式logo
    FILE* bin_test = fopen(LOGO_FILE_PATH, "rb");
    if (bin_test != NULL) {
        fclose(bin_test);
        ESP_LOGI(TAG, "发现二进制logo文件，使用高速加载: %s", LOGO_FILE_PATH);
        
        // 读取二进制logo文件
        FILE* f = fopen(LOGO_FILE_PATH, "rb");
        if (f == NULL) {
            ESP_LOGE(TAG, "无法打开二进制logo文件: %s", LOGO_FILE_PATH);
            return false;
        }
        
        // 获取文件大小
        fseek(f, 0, SEEK_END);
        long logo_file_size = ftell(f);
        fseek(f, 0, SEEK_SET);
        
#if DEBUG_IMAGE_FILES
        // 添加调试信息
        uint8_t debug_bytes[16];
        size_t debug_read = fread(debug_bytes, 1, 16, f);
        ESP_LOGI(TAG, "调试logo文件 %s (大小:%ld字节):", LOGO_FILE_PATH, logo_file_size);
        ESP_LOGI(TAG, "前16字节: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x", 
                 debug_bytes[0], debug_bytes[1], debug_bytes[2], debug_bytes[3],
                 debug_bytes[4], debug_bytes[5], debug_bytes[6], debug_bytes[7],
                 debug_bytes[8], debug_bytes[9], debug_bytes[10], debug_bytes[11],
                 debug_bytes[12], debug_bytes[13], debug_bytes[14], debug_bytes[15]);
        
        // 检查是否为原始RGB数据
        bool looks_like_raw_logo = true;
        if (debug_read >= 4 && debug_bytes[0] == 0x47 && debug_bytes[1] == 0x4D && 
            debug_bytes[2] == 0x49 && debug_bytes[3] == 0x42) {
            looks_like_raw_logo = false;
        }
        
        if (looks_like_raw_logo) {
            ESP_LOGW(TAG, "logo文件看起来像原始RGB数据，直接加载");
            fclose(f);
            
            // 直接作为原始数据加载
            FILE* raw_f = fopen(LOGO_FILE_PATH, "rb");
            if (raw_f) {
                logo_data_ = (uint8_t*)malloc(logo_file_size);
                if (logo_data_) {
                    size_t read_size = fread(logo_data_, 1, logo_file_size, raw_f);
                    fclose(raw_f);
                    if (read_size == logo_file_size) {
                        ESP_LOGI(TAG, "成功加载原始logo数据: 大小 %ld 字节", logo_file_size);
                        return true;
                    } else {
                        free(logo_data_);
                        logo_data_ = nullptr;
                    }
                } else {
                    fclose(raw_f);
                }
            }
            ESP_LOGE(TAG, "加载原始logo数据失败");
            return false;
        }
        
        // 重置文件指针
        fseek(f, 0, SEEK_SET);
#endif
        
        // 读取文件头
        BinaryImageHeader header;
        if (fread(&header, sizeof(BinaryImageHeader), 1, f) != 1) {
            ESP_LOGE(TAG, "读取logo文件头失败");
            fclose(f);
            return false;
        }
        
        // 验证魔数和版本
        if (header.magic != BINARY_IMAGE_MAGIC || header.version != BINARY_IMAGE_VERSION) {
            ESP_LOGE(TAG, "logo文件格式无效，魔数: 0x%08" PRIX32 "，版本: %" PRIu32, header.magic, header.version);
            
#if DEBUG_IMAGE_FILES
            ESP_LOGE(TAG, "尝试删除损坏的logo文件: %s", LOGO_FILE_PATH);
            fclose(f);
            if (remove(LOGO_FILE_PATH) == 0) {
                ESP_LOGI(TAG, "成功删除损坏的logo文件，将在下次检查时重新下载");
            }
            return false;
#else
            fclose(f);
            return false;
#endif
        }
        
        // 分配logo数据缓冲区
        logo_data_ = (uint8_t*)malloc(header.data_size);
        if (!logo_data_) {
            ESP_LOGE(TAG, "logo数据内存分配失败");
            fclose(f);
            return false;
        }
        
        // 直接读取logo数据
        size_t read_size = fread(logo_data_, 1, header.data_size, f);
        fclose(f);
        
        if (read_size != header.data_size) {
            ESP_LOGE(TAG, "读取logo数据不完整");
            free(logo_data_);
            logo_data_ = nullptr;
            return false;
        }
        
        ESP_LOGI(TAG, "成功加载二进制logo: 大小 %" PRIu32 " 字节", header.data_size);
        return true;
    }
    
    // 回退到.h文件解析（向后兼容）
    FILE* f = fopen(LOGO_FILE_PATH_H, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "无法打开.h格式logo文件: %s", LOGO_FILE_PATH_H);
        return false;
    }
    
    ESP_LOGI(TAG, "使用传统.h文件解析logo: %s", LOGO_FILE_PATH_H);
    
    // 获取文件大小
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    // 优化内存分配：预先分配足够大的缓冲区并对齐
    size_t aligned_size = (file_size + 15) & ~15;  // 16字节对齐，提升内存访问速度
    char* text_buffer = (char*)malloc(aligned_size + 1);
    if (text_buffer == NULL) {
        ESP_LOGE(TAG, "内存分配失败");
        fclose(f);
        return false;
    }
    
    // 使用更大的缓冲区进行文件读取，减少I/O调用次数  
    setvbuf(f, NULL, _IOFBF, 65536);  // 增加到64KB缓冲区，大幅提升I/O性能
    size_t read_size = fread(text_buffer, 1, file_size, f);
    text_buffer[read_size] = '\0';
    fclose(f);
    
    // 查找数组声明
    const char* array_pattern = "const unsigned char";
    char* array_start = strstr(text_buffer, array_pattern);
    if (!array_start) {
        ESP_LOGE(TAG, "未找到数组声明");
        free(text_buffer);
        return false;
    }
    
    // 查找数组大小
    char* size_start = strstr(array_start, "[");
    char* size_end = strstr(size_start, "]");
    if (!size_start || !size_end) {
        ESP_LOGE(TAG, "未找到数组大小");
        free(text_buffer);
        return false;
    }
    
    // 提取数组大小
    char size_str[32] = {0};
    strncpy(size_str, size_start + 1, size_end - size_start - 1);
    int array_size = atoi(size_str);
    
    // 查找数据开始的位置
    char* data_start = strstr(size_end, "{");
    if (!data_start) {
        ESP_LOGE(TAG, "未找到数组数据");
        free(text_buffer);
        return false;
    }
    data_start++;  // 跳过 '{'
    
    // 分配logo缓冲区
    logo_data_ = (uint8_t*)malloc(array_size);
    if (!logo_data_) {
        ESP_LOGE(TAG, "内存分配失败");
        free(text_buffer);
        return false;
    }
    
    // 使用优化的查找表进行十六进制解析，但保持原有的字节序处理逻辑
    char* p = data_start;
    int index = 0;
    
    // 优化的十六进制查找表
    static const int hex_values[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
         0, 1, 2, 3, 4, 5, 6, 7, 8, 9,-1,-1,-1,-1,-1,-1,  // 0-9
        -1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1,  // A-F
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1,  // a-f
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    };
    
    // 优化的高速十六进制解析循环
    char* text_end = text_buffer + read_size;
    
    while (p < text_end - 5 && index < array_size) {
        // 优化的字符串搜索：使用memchr快速查找下一个'0'
        char* next_zero = (char*)memchr(p, '0', text_end - p);
        if (!next_zero || next_zero >= text_end - 3) break;
        
        p = next_zero;
        
        // 快速检查是否为 "0x" 或 "0X"
        if (*(p+1) == 'x' || *(p+1) == 'X') {
            p += 2; // 跳过 "0x"
            
            // 快速提取十六进制数值
            int high = hex_values[(unsigned char)*p];
            int low = hex_values[(unsigned char)*(p+1)];
            
            if ((high | low) >= 0) {  // 位运算检查，比 && 更快
                unsigned char value = (high << 4) | low;
                
                // 优化的字节序交换逻辑
                if (index & 1) {  // 使用位运算代替取模
                    // 奇数索引：交换字节序
                    if (index > 0) {
                        unsigned char temp = logo_data_[index-1];
                        logo_data_[index-1] = value;
                        logo_data_[index] = temp;
                    } else {
                        logo_data_[index] = value;
                    }
                } else {
                    // 偶数索引：直接存储
                    logo_data_[index] = value;
                }
                
                index++;
                p += 2;
            } else {
                p++;
            }
        } else {
            p++;
        }
    }
    
    free(text_buffer);
    
    // 检查是否提取了足够的数据
    if (index < array_size) {
        ESP_LOGW(TAG, "提取的logo数据不完整: %d/%d 字节", index, array_size);
    }
    
    ESP_LOGI(TAG, "成功加载logo图片: 大小 %d 字节", index);
    return true;
}

bool ImageResourceManager::LoadImageFile(int image_index) {
    // 首先尝试加载二进制格式（高速加载）
    char bin_filename[128];
    snprintf(bin_filename, sizeof(bin_filename), "%soutput_%04d.bin", IMAGE_BASE_PATH, image_index);
    
    FILE* bin_test = fopen(bin_filename, "rb");
    if (bin_test != NULL) {
        fclose(bin_test);
        ESP_LOGI(TAG, "发现二进制文件，使用高速加载: %s", bin_filename);
        return LoadBinaryImageFile(image_index);
    }
    
    // 回退到.h文件解析（兼容性支持）
    char filename[128];
    snprintf(filename, sizeof(filename), "%soutput_%04d.h", IMAGE_BASE_PATH, image_index);
    
    FILE* f = fopen(filename, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "无法打开文件: %s", filename);
        return false;
    }
    
    ESP_LOGI(TAG, "使用传统.h文件解析: %s", filename);
    
    // 获取文件大小
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    // 优化内存分配：预先分配足够大的缓冲区并对齐
    size_t aligned_size = (file_size + 15) & ~15;  // 16字节对齐，提升内存访问速度
    char* text_buffer = (char*)malloc(aligned_size + 1);
    if (text_buffer == NULL) {
        ESP_LOGE(TAG, "内存分配失败");
        fclose(f);
        return false;
    }
    
    // 使用更大的缓冲区进行文件读取，减少I/O调用次数  
    setvbuf(f, NULL, _IOFBF, 65536);  // 增加到64KB缓冲区，大幅提升I/O性能
    size_t read_size = fread(text_buffer, 1, file_size, f);
    text_buffer[read_size] = '\0';
    fclose(f);
    
    // 查找数组声明
    const char* array_pattern = "const unsigned char";
    char* array_start = strstr(text_buffer, array_pattern);
    if (!array_start) {
        ESP_LOGE(TAG, "未找到数组声明");
        free(text_buffer);
        return false;
    }
    
    // 查找数组大小
    char* size_start = strstr(array_start, "[");
    char* size_end = strstr(size_start, "]");
    if (!size_start || !size_end) {
        ESP_LOGE(TAG, "未找到数组大小");
        free(text_buffer);
        return false;
    }
    
    // 提取数组大小
    char size_str[32] = {0};
    strncpy(size_str, size_start + 1, size_end - size_start - 1);
    int array_size = atoi(size_str);
    
    // 查找数据开始的位置
    char* data_start = strstr(size_end, "{");
    if (!data_start) {
        ESP_LOGE(TAG, "未找到数组数据");
        free(text_buffer);
        return false;
    }
    data_start++;  // 跳过 '{'
    
    // 分配图像缓冲区
    uint8_t* img_buffer = (uint8_t*)malloc(array_size);
    if (!img_buffer) {
        ESP_LOGE(TAG, "内存分配失败");
        free(text_buffer);
        return false;
    }
    
    // 使用优化的查找表进行十六进制解析，但保持原有的字节序处理逻辑
    char* p = data_start;
    int index = 0;
    
    // 优化的十六进制查找表
    static const int hex_values[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
         0, 1, 2, 3, 4, 5, 6, 7, 8, 9,-1,-1,-1,-1,-1,-1,  // 0-9
        -1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1,  // A-F
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1,  // a-f
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    };
    
    // 优化的高速十六进制解析循环
    char* text_end = text_buffer + read_size;
    
    while (p < text_end - 5 && index < array_size) {
        // 优化的字符串搜索：使用memchr快速查找下一个'0'
        char* next_zero = (char*)memchr(p, '0', text_end - p);
        if (!next_zero || next_zero >= text_end - 3) break;
        
        p = next_zero;
        
        // 快速检查是否为 "0x" 或 "0X"
        if (*(p+1) == 'x' || *(p+1) == 'X') {
            p += 2; // 跳过 "0x"
            
            // 快速提取十六进制数值
            int high = hex_values[(unsigned char)*p];
            int low = hex_values[(unsigned char)*(p+1)];
            
            if ((high | low) >= 0) {  // 位运算检查，比 && 更快
                unsigned char value = (high << 4) | low;
                
                // 优化的字节序交换逻辑
                if (index & 1) {  // 使用位运算代替取模
                    // 奇数索引：交换字节序
                    if (index > 0) {
                        unsigned char temp = img_buffer[index-1];
                        img_buffer[index-1] = value;
                        img_buffer[index] = temp;
                    } else {
                        img_buffer[index] = value;
                    }
                } else {
                    // 偶数索引：直接存储
                    img_buffer[index] = value;
                }
                
                index++;
                p += 2;
            } else {
                p++;
            }
        } else {
            p++;
        }
    }
    
    free(text_buffer);
    
    // 检查是否提取了足够的数据
    if (index < array_size) {
        ESP_LOGW(TAG, "提取的数据不完整: %d/%d 字节", index, array_size);
    }
    
    // 保存到数组
    int array_index = image_index - 1;
    if (array_index >= 0 && array_index < image_array_.size()) {
        if (image_data_pointers_[array_index]) {
            free(image_data_pointers_[array_index]);
        }
        
        image_data_pointers_[array_index] = img_buffer;
        image_array_[array_index] = img_buffer;
        
        ESP_LOGI(TAG, "成功加载动画图片 %d: 大小 %d 字节", image_index, index);
        return true;
    } else {
        ESP_LOGE(TAG, "索引超出范围: %d", array_index);
        free(img_buffer);
        return false;
    }
}

// CheckAndUpdateResources 方法已移除 - 网络功能已禁用

// CheckAndUpdateLogo 方法已移除 - 网络功能已禁用

const std::vector<const uint8_t*>& ImageResourceManager::GetImageArray() const {
    return image_array_;
}

const uint8_t* ImageResourceManager::GetLogoImage() const {
    return logo_data_;
}

// CheckAllServerResources 方法已移除 - 网络功能已禁用 (第1部分)
// CheckAllServerResources 方法已移除 - 网络功能已禁用 (第2部分)

// DownloadImagesWithUrls 方法已移除 - 网络功能已禁用 (第1部分)
// DownloadImagesWithUrls 方法已移除 - 网络功能已禁用 (第2部分)

// DownloadLogoWithUrl 方法已移除 - 网络功能已禁用

// CheckAndUpdateAllResources 方法已移除 - 网络功能已禁用

// EnterDownloadMode 方法已移除 - 网络功能已禁用

// ExitDownloadMode 方法已移除 - 网络功能已禁用

// DeleteExistingAnimationFiles 方法已移除 - 网络功能已禁用

// DeleteExistingLogoFile 方法已移除 - 网络功能已禁用

bool ImageResourceManager::LoadImageOnDemand(int image_index) {
    // 检查索引是否有效
    if (image_index < 1 || image_index > (int)image_array_.size()) {
        ESP_LOGE(TAG, "按需加载：图片索引超出范围: %d", image_index);
        return false;
    }
    
    int array_index = image_index - 1;
    
    // 检查是否已经加载
    if (image_array_[array_index] != nullptr) {
        return true;  // 已经加载，直接返回成功
    }
    
    // 检查内存是否足够
    size_t free_heap = esp_get_free_heap_size();
    if (free_heap < 200000) { // 需要至少200KB可用内存
        ESP_LOGW(TAG, "按需加载：内存不足，无法加载图片 %d，可用内存: %u字节", 
                image_index, (unsigned int)free_heap);
        return false;
    }
    
    ESP_LOGI(TAG, "按需加载图片 %d...", image_index);
    
    // 尝试加载图片
    if (LoadImageFile(image_index)) {
        ESP_LOGI(TAG, "按需加载图片 %d 成功", image_index);
        return true;
    } else {
        ESP_LOGE(TAG, "按需加载图片 %d 失败", image_index);
        return false;
    }
}

bool ImageResourceManager::IsImageLoaded(int image_index) const {
    // 检查索引是否有效
    if (image_index < 1 || image_index > (int)image_array_.size()) {
        return false;
    }
    
    int array_index = image_index - 1;
    return image_array_[array_index] != nullptr;
}

bool ImageResourceManager::ClearAllImageFiles() {
    if (!mounted_) {
        ESP_LOGE(TAG, "分区未挂载，无法清理文件");
        return false;
    }
    
    ESP_LOGI(TAG, "开始清理所有图片文件...");
    int deleted_count = 0;
    
    // 清理动画图片文件
    for (int i = 1; i <= MAX_IMAGE_FILES; i++) {
        char bin_filepath[128];
        char h_filepath[128];
        snprintf(bin_filepath, sizeof(bin_filepath), "%soutput_%04d.bin", IMAGE_BASE_PATH, i);
        snprintf(h_filepath, sizeof(h_filepath), "%soutput_%04d.h", IMAGE_BASE_PATH, i);
        
        if (remove(bin_filepath) == 0) {
            ESP_LOGI(TAG, "删除文件: %s", bin_filepath);
            deleted_count++;
        }
        
        if (remove(h_filepath) == 0) {
            ESP_LOGI(TAG, "删除文件: %s", h_filepath);
            deleted_count++;
        }
    }
    
    // 清理logo文件
    if (remove(LOGO_FILE_PATH) == 0) {
        ESP_LOGI(TAG, "删除文件: %s", LOGO_FILE_PATH);
        deleted_count++;
    }
    
    if (remove(LOGO_FILE_PATH_H) == 0) {
        ESP_LOGI(TAG, "删除文件: %s", LOGO_FILE_PATH_H);
        deleted_count++;
    }
    
    // 清理缓存文件
    if (remove(IMAGE_URL_CACHE_FILE) == 0) {
        ESP_LOGI(TAG, "删除文件: %s", IMAGE_URL_CACHE_FILE);
        deleted_count++;
    }
    
    if (remove(LOGO_URL_CACHE_FILE) == 0) {
        ESP_LOGI(TAG, "删除文件: %s", LOGO_URL_CACHE_FILE);
        deleted_count++;
    }
    
    ESP_LOGI(TAG, "文件清理完成，共删除 %d 个文件", deleted_count);
    
    // 重置状态
    has_valid_images_ = false;
    has_valid_logo_ = false;
    cached_dynamic_urls_.clear();
    cached_static_url_.clear();
    server_dynamic_urls_.clear();
    server_static_url_.clear();
    
    // 清理内存中的图片数据
    for (auto ptr : image_data_pointers_) {
        if (ptr) {
            free(ptr);
        }
    }
    image_data_pointers_.clear();
    image_array_.clear();
    
    if (logo_data_) {
        free(logo_data_);
        logo_data_ = nullptr;
    }
    
    ESP_LOGI(TAG, "图片资源状态已重置，下次将重新下载所有文件");
    return true;
}

esp_err_t ImageResourceManager::PreloadRemainingImages() {
    if (!has_valid_images_ || image_array_.empty()) {
        ESP_LOGW(TAG, "没有有效的图片资源，跳过预加载");
        return ESP_FAIL;
    }
    
    // 检查可用内存
    size_t free_heap = esp_get_free_heap_size();
    ESP_LOGI(TAG, "开始预加载剩余图片，当前可用内存: %u字节", (unsigned int)free_heap);
    
    if (free_heap < 500000) { // 从1MB减少到500KB，降低内存门槛
        ESP_LOGW(TAG, "内存不足，跳过预加载，可用内存: %u字节", (unsigned int)free_heap);
        return ESP_ERR_NO_MEM;
    }
    
    // 获取Application实例以检查音频状态
    auto& app = Application::GetInstance();
    
    int loaded_count = 0;
    int total_images = image_array_.size();
    
    // 通知开始预加载
    if (preload_progress_callback_) {
        preload_progress_callback_(0, total_images, "准备预加载图片资源...");
    }
    
    ESP_LOGI(TAG, "优化预加载策略：智能加载剩余图片 (总数: %d)", total_images);
    
    for (int i = 1; i <= total_images; i++) {
        // 检查图片是否已经加载
        if (IsImageLoaded(i)) {
            loaded_count++;
            // 更新进度（已加载的图片）
            if (preload_progress_callback_) {
                char message[64];
                snprintf(message, sizeof(message), "图片 %d 已加载，跳过...", i);
                preload_progress_callback_(loaded_count, total_images, message);
            }
            continue; // 已加载，跳过
        }
        
        // 优化：减少音频状态检查频率，只检查关键状态
        if (i % 3 == 0) { // 每3张图片检查一次，减少检查频率
            if (!app.IsAudioQueueEmpty() || app.GetDeviceState() != kDeviceStateIdle) {
                ESP_LOGW(TAG, "检测到音频活动，暂停预加载以避免冲突，已加载: %d/%d", loaded_count, total_images);
                
                // 通知预加载被中断
                if (preload_progress_callback_) {
                    char message[64];
                    snprintf(message, sizeof(message), "预加载中断：检测到音频活动");
                    preload_progress_callback_(loaded_count, total_images, message);
                    vTaskDelay(pdMS_TO_TICKS(1500)); // 从2秒减少到1.5秒显示时间
                    preload_progress_callback_(loaded_count, total_images, nullptr); // 隐藏UI
                }
                break;
            }
        }
        
        // 检查内存状况
        free_heap = esp_get_free_heap_size();
        if (free_heap < 200000) { // 从300KB减少到200KB，更激进的内存使用
            ESP_LOGW(TAG, "预加载过程中内存不足，停止加载，已加载: %d/%d", loaded_count, total_images);
            
            // 通知内存不足
            if (preload_progress_callback_) {
                char message[64];
                snprintf(message, sizeof(message), "预加载停止：内存不足");
                preload_progress_callback_(loaded_count, total_images, message);
                vTaskDelay(pdMS_TO_TICKS(1500)); // 从2秒减少到1.5秒显示时间
                preload_progress_callback_(loaded_count, total_images, nullptr); // 隐藏UI
            }
            break;
        }
        
        // 更新进度 - 开始加载当前图片
        if (preload_progress_callback_) {
            char message[64];
            snprintf(message, sizeof(message), "正在预加载图片 %d/%d", i, total_images);
            preload_progress_callback_(loaded_count, total_images, message);
        }
        
        ESP_LOGI(TAG, "预加载图片 %d/%d...", i, total_images);
        
        if (LoadImageFile(i)) {
            loaded_count++;
            ESP_LOGI(TAG, "预加载图片 %d 成功", i);
            
            // 更新进度 - 当前图片加载完成
            if (preload_progress_callback_) {
                char message[64];
                snprintf(message, sizeof(message), "图片 %d 预加载完成", i);
                preload_progress_callback_(loaded_count, total_images, message);
            }
        } else {
            ESP_LOGE(TAG, "预加载图片 %d 失败", i);
            
            // 通知加载失败但继续
            if (preload_progress_callback_) {
                char message[64];
                snprintf(message, sizeof(message), "图片 %d 预加载失败，继续下一张", i);
                preload_progress_callback_(loaded_count, total_images, message);
            }
        }
        
        // 优化：进一步减少延迟时间，加快预加载速度
        vTaskDelay(pdMS_TO_TICKS(10)); // 从20ms减少到10ms
    }
    
    free_heap = esp_get_free_heap_size();
    ESP_LOGI(TAG, "预加载完成，成功加载: %d/%d 张图片，剩余内存: %u字节", 
             loaded_count, total_images, (unsigned int)free_heap);
    ESP_LOGI(TAG, "已启用服务器直传二进制格式优化，下载+预加载速度大幅提升");
    
    // 通知预加载完成
    if (preload_progress_callback_) {
        char message[64];
        if (loaded_count == total_images) {
            snprintf(message, sizeof(message), "所有图片预加载完成！");
        } else {
            snprintf(message, sizeof(message), "预加载完成：%d/%d 张图片", loaded_count, total_images);
        }
        preload_progress_callback_(loaded_count, total_images, message);
        
        // 优化：减少UI显示时间
        vTaskDelay(pdMS_TO_TICKS(1500)); // 从2秒减少到1.5秒
        preload_progress_callback_(loaded_count, total_images, nullptr);
    }
    
    return loaded_count > 0 ? ESP_OK : ESP_FAIL;
}