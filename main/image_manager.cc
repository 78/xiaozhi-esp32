#include "image_manager.h"
#include <esp_log.h>
#include <esp_spiffs.h>
#include <esp_http_client.h>
#include <string.h>
#include <sys/stat.h>
#include <cJSON.h>
#include <wifi_station.h>
#include "board.h"

#define TAG "ImageResManager"
#define IMAGE_VERSION_FILE "/resources/version.json"
#define LOGO_VERSION_FILE "/resources/logo_version.json"  // 新增：logo版本文件
#define IMAGE_BASE_PATH "/resources/images/"
#define LOGO_FILE_PATH "/resources/images/logo.h"
#define MAX_IMAGE_FILES 2
#define MAX_DOWNLOAD_RETRIES 3

ImageResourceManager::ImageResourceManager() {
    mounted_ = false;
    initialized_ = false;
    has_valid_images_ = false;
    has_valid_logo_ = false;  // 新增
    logo_data_ = nullptr;
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
    
    // 检查本地版本
    local_version_ = ReadLocalVersion();
    local_logo_version_ = ReadLocalLogoVersion();  // 新增：读取logo版本
    ESP_LOGI(TAG, "当前本地动画图片版本: %s", local_version_.c_str());
    ESP_LOGI(TAG, "当前本地logo版本: %s", local_logo_version_.c_str());
    
    // 检查是否有有效图片
    has_valid_images_ = CheckImagesExist();
    has_valid_logo_ = CheckLogoExists();  // 新增：检查logo
    
    if (has_valid_images_) {
        ESP_LOGI(TAG, "找到有效的动画图片文件");
    } else {
        ESP_LOGI(TAG, "未找到有效的动画图片文件，将在网络连接后下载");
    }
    
    if (has_valid_logo_) {
        ESP_LOGI(TAG, "找到有效的logo文件");
    } else {
        ESP_LOGI(TAG, "未找到有效的logo文件，将在网络连接后下载");
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
    
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/resources",
        .partition_label = "resources",
        .max_files = 20,
        .format_if_mount_failed = true
    };
    
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "挂载resources分区失败 (%s)", esp_err_to_name(ret));
        return ret;
    }
    
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "获取SPIFFS信息失败");
        return ret;
    }
    
    ESP_LOGI(TAG, "resources分区挂载成功, 总大小: %d字节, 已使用: %d字节", total, used);
    mounted_ = true;
    return ESP_OK;
}

std::string ImageResourceManager::ReadLocalVersion() {
    if (!mounted_) {
        return "0";
    }
    
    FILE* f = fopen(IMAGE_VERSION_FILE, "r");
    if (f == NULL) {
        ESP_LOGW(TAG, "无法打开动画图片版本文件，假定初始版本");
        return "0";
    }
    
    char buffer[128];
    size_t len = fread(buffer, 1, sizeof(buffer) - 1, f);
    fclose(f);
    
    if (len <= 0) {
        return "0";
    }
    
    buffer[len] = '\0';
    
    cJSON* root = cJSON_Parse(buffer);
    if (root == NULL) {
        ESP_LOGE(TAG, "解析动画图片版本文件失败");
        return "0";
    }
    
    cJSON* version = cJSON_GetObjectItem(root, "version");
    if (version == NULL || !cJSON_IsString(version) || strlen(version->valuestring) == 0) {
        cJSON_Delete(root);
        return "0";
    }
    
    std::string ver = version->valuestring;
    cJSON_Delete(root);
    
    // 确保版本不为空
    if (ver.empty()) {
        return "0";
    }
    
    return ver;
}

std::string ImageResourceManager::ReadLocalLogoVersion() {
    if (!mounted_) {
        return "0";
    }
    
    FILE* f = fopen(LOGO_VERSION_FILE, "r");
    if (f == NULL) {
        ESP_LOGW(TAG, "无法打开logo版本文件，假定初始版本");
        return "0";
    }
    
    char buffer[128];
    size_t len = fread(buffer, 1, sizeof(buffer) - 1, f);
    fclose(f);
    
    if (len <= 0) {
        return "0";
    }
    
    buffer[len] = '\0';
    
    cJSON* root = cJSON_Parse(buffer);
    if (root == NULL) {
        ESP_LOGE(TAG, "解析logo版本文件失败");
        return "0";
    }
    
    cJSON* version = cJSON_GetObjectItem(root, "version");
    if (version == NULL || !cJSON_IsString(version) || strlen(version->valuestring) == 0) {
        cJSON_Delete(root);
        return "0";
    }
    
    std::string ver = version->valuestring;
    cJSON_Delete(root);
    
    // 确保版本不为空
    if (ver.empty()) {
        return "0";
    }
    
    return ver;
}

bool ImageResourceManager::CheckImagesExist() {
    if (!mounted_) {
        return false;
    }
    
    // 检查动画图片文件
    for (int i = 1; i <= MAX_IMAGE_FILES; i++) {
        char filename[64];
        snprintf(filename, sizeof(filename), "%soutput_%04d.h", IMAGE_BASE_PATH, i);
        
        FILE* f = fopen(filename, "r");
        if (f == NULL) {
            ESP_LOGW(TAG, "未找到动画图片文件: %s", filename);
            return false;
        }
        fclose(f);
    }
    
    return true;
}

bool ImageResourceManager::CheckLogoExists() {
    if (!mounted_) {
        return false;
    }
    
    FILE* f = fopen(LOGO_FILE_PATH, "r");
    if (f == NULL) {
        ESP_LOGW(TAG, "未找到logo文件: %s", LOGO_FILE_PATH);
        return false;
    }
    fclose(f);
    
    return true;
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

esp_err_t ImageResourceManager::CheckServerVersion(const char* version_url) {
    // 确保已连接WiFi
    if (!WifiStation::GetInstance().IsConnected()) {
        ESP_LOGW(TAG, "未连接WiFi，无法检查服务器动画图片版本");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "检查服务器动画图片版本...");
    
    auto http = Board::GetInstance().CreateHttp();
    if (!http->Open("GET", version_url)) {
        ESP_LOGE(TAG, "无法连接到服务器");
        delete http;
        return ESP_FAIL;
    }
    
    std::string response = http->GetBody();
    http->Close();
    delete http;
    
    if (response.empty()) {
        ESP_LOGE(TAG, "服务器返回空响应");
        return ESP_FAIL;
    }
    
    cJSON* root = cJSON_Parse(response.c_str());
    if (root == NULL) {
        ESP_LOGE(TAG, "解析服务器响应失败");
        return ESP_FAIL;
    }
    
    cJSON* version = cJSON_GetObjectItem(root, "version");
    if (version == NULL || !cJSON_IsString(version)) {
        ESP_LOGE(TAG, "服务器响应中无版本信息");
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    
    server_version_ = version->valuestring;
    if (server_version_.empty()) {
        ESP_LOGE(TAG, "服务器版本为空");
        server_version_ = "1.0.0"; // 设置默认版本
    }
    
    cJSON_Delete(root);
    
    ESP_LOGI(TAG, "服务器动画图片版本: %s, 本地版本: %s", 
             server_version_.c_str(), local_version_.c_str());
    
    return server_version_ != local_version_ ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t ImageResourceManager::CheckServerLogoVersion(const char* logo_version_url) {
    // 确保已连接WiFi
    if (!WifiStation::GetInstance().IsConnected()) {
        ESP_LOGW(TAG, "未连接WiFi，无法检查服务器logo版本");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "检查服务器logo版本...");
    
    auto http = Board::GetInstance().CreateHttp();
    if (!http->Open("GET", logo_version_url)) {
        ESP_LOGE(TAG, "无法连接到logo版本服务器");
        delete http;
        return ESP_FAIL;
    }
    
    std::string response = http->GetBody();
    http->Close();
    delete http;
    
    if (response.empty()) {
        ESP_LOGE(TAG, "logo版本服务器返回空响应");
        return ESP_FAIL;
    }
    
    cJSON* root = cJSON_Parse(response.c_str());
    if (root == NULL) {
        ESP_LOGE(TAG, "解析logo版本响应失败");
        return ESP_FAIL;
    }
    
    cJSON* version = cJSON_GetObjectItem(root, "version");
    if (version == NULL || !cJSON_IsString(version)) {
        ESP_LOGE(TAG, "logo版本响应中无版本信息");
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    
    server_logo_version_ = version->valuestring;
    if (server_logo_version_.empty()) {
        ESP_LOGE(TAG, "服务器logo版本为空");
        server_logo_version_ = "1.0.0"; // 设置默认版本
    }
    
    cJSON_Delete(root);
    
    ESP_LOGI(TAG, "服务器logo版本: %s, 本地logo版本: %s", 
             server_logo_version_.c_str(), local_logo_version_.c_str());
    
    return server_logo_version_ != local_logo_version_ ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t ImageResourceManager::DownloadImages(const char* api_url) {
    // 确保已连接WiFi
    if (!WifiStation::GetInstance().IsConnected()) {
        ESP_LOGW(TAG, "未连接WiFi，无法下载动画图片");
        
        // 通知错误
        if (progress_callback_) {
            progress_callback_(0, 100, "未连接WiFi，无法下载动画图片");
        }
        
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "开始下载动画图片文件...");
    
    // 通知开始下载
    if (progress_callback_) {
        progress_callback_(0, 100, "准备下载动画图片资源...");
    }
    
    bool success = true;
    
    // 下载动画图片文件 - 使用多任务并行下载
    // 创建一个临时数组存储文件路径
    std::vector<std::string> file_paths;
    std::vector<std::string> urls;
    
    // 先准备好所有文件路径和URL
    for (int i = 1; i <= MAX_IMAGE_FILES; i++) {
        char filename[64];
        snprintf(filename, sizeof(filename), "output_%04d.h", i);
        
        char filepath[128];
        snprintf(filepath, sizeof(filepath), "%s%s", IMAGE_BASE_PATH, filename);
        
        std::string url = std::string(api_url) + "/" + std::string(filename);
        
        file_paths.push_back(filepath);
        urls.push_back(url);
        
        ESP_LOGI(TAG, "准备下载动画图片文件 [%d/%d]: %s", i, MAX_IMAGE_FILES, filename);
    }
    
    // 逐个下载文件
    for (size_t i = 0; i < urls.size(); i++) {
        if (progress_callback_) {
            int overall_percent = static_cast<int>(i * 100 / MAX_IMAGE_FILES);
            char message[128];
            const char* filename = strrchr(file_paths[i].c_str(), '/') + 1;
            snprintf(message, sizeof(message), "准备下载动画图片: %s (%d/%d)", 
                    filename, static_cast<int>(i+1), MAX_IMAGE_FILES);
            progress_callback_(overall_percent, 100, message);
        }
        
        if (DownloadFile(urls[i].c_str(), file_paths[i].c_str()) != ESP_OK) {
            ESP_LOGE(TAG, "下载动画图片文件失败: %s", file_paths[i].c_str());
            success = false;
            break;
        }
    }
    
    if (success) {
        // 保存新版本
        if (server_version_.empty()) {
            ESP_LOGE(TAG, "服务器版本为空，使用默认版本");
            server_version_ = "1.0.0"; // 设置一个默认版本
        }
        
        if (!SaveVersion(server_version_)) {
            ESP_LOGE(TAG, "保存动画图片版本信息失败");
            
            // 通知错误
            if (progress_callback_) {
                progress_callback_(100, 100, "保存动画图片版本信息失败");
            }
            
            return ESP_FAIL;
        }
        
        local_version_ = server_version_;
        has_valid_images_ = true;
        
        // 通知完成
        if (progress_callback_) {
            progress_callback_(100, 100, "动画图片下载完成，正在加载图片...");
        }
        
        // 加载新下载的图片数据
        LoadImageData();
        
        // 通知加载完成
        if (progress_callback_) {
            progress_callback_(100, 100, "动画图片资源已就绪");
            
            // 延迟一段时间后隐藏进度条
            vTaskDelay(pdMS_TO_TICKS(1000));
            progress_callback_(100, 100, nullptr);
        }
        
        ESP_LOGI(TAG, "所有动画图片文件下载完成，更新版本为: %s", local_version_.c_str());
        return ESP_OK;
    }
    
    // 通知失败
    if (progress_callback_) {
        progress_callback_(0, 100, "下载动画图片资源失败");
    }
    
    return ESP_FAIL;
}

esp_err_t ImageResourceManager::DownloadLogo(const char* api_url) {
    // 确保已连接WiFi
    if (!WifiStation::GetInstance().IsConnected()) {
        ESP_LOGW(TAG, "未连接WiFi，无法下载logo");
        
        // 通知错误
        if (progress_callback_) {
            progress_callback_(0, 100, "未连接WiFi，无法下载logo");
        }
        
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "开始下载logo文件...");
    
    // 通知开始下载
    if (progress_callback_) {
        progress_callback_(0, 100, "准备下载logo资源...");
    }
    
    // 下载logo文件
    std::string logo_url = std::string(api_url) + "/logo.h";
    ESP_LOGI(TAG, "下载logo文件: logo.h");
    
    if (progress_callback_) {
        progress_callback_(0, 100, "正在下载logo.h");
    }
    
    if (DownloadFile(logo_url.c_str(), LOGO_FILE_PATH) != ESP_OK) {
        ESP_LOGE(TAG, "下载logo文件失败");
        
        if (progress_callback_) {
            progress_callback_(0, 100, "下载logo失败");
        }
        
        return ESP_FAIL;
    }
    
    // 保存新版本
    if (server_logo_version_.empty()) {
        ESP_LOGE(TAG, "服务器logo版本为空，使用默认版本");
        server_logo_version_ = "1.0.0"; // 设置一个默认版本
    }
    
    if (!SaveLogoVersion(server_logo_version_)) {
        ESP_LOGE(TAG, "保存logo版本信息失败");
        
        // 通知错误
        if (progress_callback_) {
            progress_callback_(100, 100, "保存logo版本信息失败");
        }
        
        return ESP_FAIL;
    }
    
    local_logo_version_ = server_logo_version_;
    has_valid_logo_ = true;
    
    // 通知完成
    if (progress_callback_) {
        progress_callback_(100, 100, "logo下载完成，正在加载logo...");
    }
    
    // 加载新下载的logo数据
    LoadImageData();
    
    // 通知加载完成
    if (progress_callback_) {
        progress_callback_(100, 100, "logo资源已就绪");
        
        // 延迟一段时间后隐藏进度条
        vTaskDelay(pdMS_TO_TICKS(1000));
        progress_callback_(100, 100, nullptr);
    }
    
    ESP_LOGI(TAG, "logo文件下载完成，更新版本为: %s", local_logo_version_.c_str());
    return ESP_OK;
}

esp_err_t ImageResourceManager::DownloadFile(const char* url, const char* filepath) {
    int retry_count = 0;
    static int last_logged_percent = -1;
    
    // 检查可用内存
    size_t free_heap = esp_get_free_heap_size();
    if (free_heap < 100000) { // 至少需要100KB可用内存
        ESP_LOGE(TAG, "内存不足，无法下载文件，可用内存: %d字节", free_heap);
        if (progress_callback_) {
            progress_callback_(0, 100, "内存不足，下载失败");
        }
        return ESP_ERR_NO_MEM;
    }
    
    while (retry_count < MAX_DOWNLOAD_RETRIES) {
        last_logged_percent = -1;
        
        // 通知开始下载
        if (progress_callback_) {
            char message[128];
            const char* filename = strrchr(filepath, '/') + 1;
            snprintf(message, sizeof(message), "正在下载: %s", filename);
            progress_callback_(0, 100, message);
        }
        
        // 检查内存情况
        free_heap = esp_get_free_heap_size();
        ESP_LOGI(TAG, "下载前可用内存: %d字节", free_heap);
        
        auto http = Board::GetInstance().CreateHttp();
        if (!http) {
            ESP_LOGE(TAG, "无法创建HTTP客户端");
            retry_count++;
            if (progress_callback_) {
                progress_callback_(0, 100, "HTTP客户端创建失败");
            }
            vTaskDelay(pdMS_TO_TICKS(1000 * retry_count));
            continue;
        }
        
        if (!http->Open("GET", url)) {
            ESP_LOGE(TAG, "无法连接到服务器");
            delete http;
            retry_count++;
            
            if (progress_callback_) {
                char message[128];
                snprintf(message, sizeof(message), "连接失败，正在重试 (%d/%d)", 
                        retry_count, MAX_DOWNLOAD_RETRIES);
                progress_callback_(0, 100, message);
            }
            
            vTaskDelay(pdMS_TO_TICKS(1000 * retry_count));
            continue;
        }
        
        FILE* f = fopen(filepath, "w");
        if (f == NULL) {
            ESP_LOGE(TAG, "无法创建文件: %s", filepath);
            http->Close();
            delete http;
            if (progress_callback_) {
                progress_callback_(0, 100, "创建文件失败");
            }
            return ESP_ERR_NO_MEM;
        }
        
        size_t content_length = http->GetBodyLength();
        if (content_length == 0) {
            ESP_LOGE(TAG, "无法获取文件大小");
            fclose(f);
            http->Close();
            delete http;
            if (progress_callback_) {
                progress_callback_(0, 100, "无法获取文件大小");
            }
            return ESP_FAIL;
        }
        
        ESP_LOGI(TAG, "下载文件大小: %d字节", content_length);
        
        // 增大缓冲区大小，从512字节增加到4KB，平衡内存使用和性能
        char* buffer = (char*)malloc(4096);
        if (!buffer) {
            ESP_LOGE(TAG, "无法分配下载缓冲区");
            fclose(f);
            http->Close();
            delete http;
            if (progress_callback_) {
                progress_callback_(0, 100, "内存分配失败");
            }
            return ESP_ERR_NO_MEM;
        }
        
        size_t total_read = 0;
        bool download_success = true;
        
        while (true) {
            // 定期检查内存
            if (total_read % (4096 * 10) == 0) { // 每40KB检查一次
                free_heap = esp_get_free_heap_size();
                if (free_heap < 50000) { // 如果可用内存低于50KB
                    ESP_LOGW(TAG, "内存不足，中止下载，可用内存: %d字节", free_heap);
                    download_success = false;
                    break;
                }
            }
            
            int ret = http->Read(buffer, 4096);
            if (ret < 0) {
                ESP_LOGE(TAG, "读取HTTP数据失败");
                download_success = false;
                break;
            }
            
            if (ret == 0) {
                // 下载完成
                break;
            }
            
            size_t written = fwrite(buffer, 1, ret, f);
            if (written != ret) {
                ESP_LOGE(TAG, "写入文件失败");
                download_success = false;
                break;
            }
            
            total_read += ret;
            
            // 更频繁地更新进度显示
            if (content_length > 0) {
                int percent = (float)total_read * 100 / content_length;
                
                // 每当进度变化时都更新显示
                if (percent != last_logged_percent) {
                    const char* filename = strrchr(filepath, '/') + 1;
                    char message[128];
                    snprintf(message, sizeof(message), "正在下载 %s", filename);
                    
                    if (progress_callback_) {
                        progress_callback_(percent, 100, message);
                    }
                    
                    // 记录日志（但减少日志频率）
                    if (percent % 10 == 0 || percent == 100) {
                        ESP_LOGI(TAG, "下载进度: %d%%", percent);
                    }
                    
                    last_logged_percent = percent;
                }
            }
            
            // 短暂延迟，避免占用过多CPU
            vTaskDelay(1);
        }
        
        // 清理资源
        free(buffer);
        fclose(f);
        http->Close();
        delete http;
        
        if (download_success) {
            if (progress_callback_) {
                const char* filename = strrchr(filepath, '/') + 1;
                char message[128];
                snprintf(message, sizeof(message), "文件 %s 下载完成", filename);
                progress_callback_(100, 100, message);
            }
            last_logged_percent = -1;
            return ESP_OK;
        } else {
            retry_count++;
            if (progress_callback_) {
                char message[128];
                snprintf(message, sizeof(message), "下载失败，正在重试 (%d/%d)", 
                        retry_count, MAX_DOWNLOAD_RETRIES);
                progress_callback_(0, 100, message);
            }
            vTaskDelay(pdMS_TO_TICKS(1000 * retry_count));
        }
    }
    
    ESP_LOGE(TAG, "下载重试次数已达上限");
    if (progress_callback_) {
        progress_callback_(0, 100, "下载失败，重试次数已达上限");
    }
    
    return ESP_FAIL;
}

bool ImageResourceManager::SaveVersion(const std::string& version) {
    if (!mounted_) {
        return false;
    }
    
    FILE* f = fopen(IMAGE_VERSION_FILE, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "无法创建动画图片版本文件");
        return false;
    }
    
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "version", version.c_str());
    
    char* json_str = cJSON_Print(root);
    fprintf(f, "%s", json_str);
    
    cJSON_Delete(root);
    free(json_str);
    fclose(f);
    
    return true;
}

bool ImageResourceManager::SaveLogoVersion(const std::string& version) {
    if (!mounted_) {
        return false;
    }
    
    FILE* f = fopen(LOGO_VERSION_FILE, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "无法创建logo版本文件");
        return false;
    }
    
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "version", version.c_str());
    
    char* json_str = cJSON_Print(root);
    fprintf(f, "%s", json_str);
    
    cJSON_Delete(root);
    free(json_str);
    fclose(f);
    
    return true;
}

void ImageResourceManager::LoadImageData() {
    // 检查可用内存
    size_t free_heap = esp_get_free_heap_size();
    ESP_LOGI(TAG, "加载图片前可用内存: %d字节", free_heap);
    
    if (free_heap < 200000) { // 至少需要200KB可用内存来加载图片
        ESP_LOGW(TAG, "内存不足，跳过图片加载，可用内存: %d字节", free_heap);
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
    
    // 首先加载logo文件
    if (has_valid_logo_ && !LoadLogoFile()) {
        ESP_LOGE(TAG, "加载logo文件失败");
    }
    
    // 然后加载动画图片
    if (has_valid_images_) {
        // 预分配空间
        image_array_.resize(MAX_IMAGE_FILES);
        image_data_pointers_.resize(MAX_IMAGE_FILES, nullptr);
        
        // 加载每个图片文件
        for (int i = 1; i <= MAX_IMAGE_FILES; i++) {
            // 每次加载前检查内存
            free_heap = esp_get_free_heap_size();
            if (free_heap < 100000) { // 如果内存不足100KB，停止加载
                ESP_LOGW(TAG, "内存不足，停止加载更多图片，当前可用内存: %d字节", free_heap);
                break;
            }
            
            if (!LoadImageFile(i)) {
                ESP_LOGE(TAG, "加载动画图片文件失败，索引: %d", i);
            }
        }
        
        ESP_LOGI(TAG, "共加载 %d 个动画图片文件", image_array_.size());
    }
    
    if (has_valid_logo_) {
        ESP_LOGI(TAG, "logo文件已加载");
    }
    
    // 最终内存检查
    free_heap = esp_get_free_heap_size();
    ESP_LOGI(TAG, "加载图片后可用内存: %d字节", free_heap);
}

bool ImageResourceManager::LoadLogoFile() {
    FILE* f = fopen(LOGO_FILE_PATH, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "无法打开logo文件: %s", LOGO_FILE_PATH);
        return false;
    }
    
    // 获取文件大小
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* text_buffer = (char*)malloc(file_size + 1);
    if (text_buffer == NULL) {
        ESP_LOGE(TAG, "内存分配失败");
        fclose(f);
        return false;
    }
    
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
    
    // 解析十六进制数据
    char* p = data_start;
    int index = 0;
    
    while (*p && index < array_size) {
        // 查找0X或0x格式的十六进制数
        if ((*p == '0' && (*(p+1) == 'X' || *(p+1) == 'x'))) {
            p += 2;  // 跳过"0X"
            
            // 读取两位十六进制数
            char hex[3] = {0};
            if (isxdigit(*p) && isxdigit(*(p+1))) {
                hex[0] = *p;
                hex[1] = *(p+1);
                
                // 转换为整数
                unsigned char value = (unsigned char)strtol(hex, NULL, 16);
                
                // 交换字节顺序以修正颜色问题
                if (index % 2 == 0 && index + 1 < array_size) {
                    // 存储第一个字节(低字节)
                    logo_data_[index] = value;
                } else {
                    // 存储第二个字节(高字节)，并交换位置
                    if (index > 0) {
                        // 交换当前字节和前一个字节的位置
                        unsigned char temp = logo_data_[index-1];
                        logo_data_[index-1] = value;
                        logo_data_[index] = temp;
                    } else {
                        logo_data_[index] = value;
                    }
                }
                
                index++;
                p += 2;  // 跳过这两位数字
            } else {
                p++;  // 格式不匹配，跳过
            }
        } else {
            p++;  // 跳过非十六进制前缀
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
    char filename[128];
    snprintf(filename, sizeof(filename), "%soutput_%04d.h", IMAGE_BASE_PATH, image_index);
    
    FILE* f = fopen(filename, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "无法打开文件: %s", filename);
        return false;
    }
    
    // 获取文件大小
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* text_buffer = (char*)malloc(file_size + 1);
    if (text_buffer == NULL) {
        ESP_LOGE(TAG, "内存分配失败");
        fclose(f);
        return false;
    }
    
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
    
    // 解析十六进制数据
    char* p = data_start;
    int index = 0;
    
    while (*p && index < array_size) {
        // 查找0X或0x格式的十六进制数
        if ((*p == '0' && (*(p+1) == 'X' || *(p+1) == 'x'))) {
            p += 2;  // 跳过"0X"
            
            // 读取两位十六进制数
            char hex[3] = {0};
            if (isxdigit(*p) && isxdigit(*(p+1))) {
                hex[0] = *p;
                hex[1] = *(p+1);
                
                // 转换为整数
                unsigned char value = (unsigned char)strtol(hex, NULL, 16);
                
                // 交换字节顺序以修正颜色问题
                // 我们每次从文件中读取两个字节：高字节和低字节
                // 需要将高低字节交换顺序，并且每两个字节为一组
                if (index % 2 == 0 && index + 1 < array_size) {
                    // 存储第一个字节(低字节)
                    img_buffer[index] = value;
                } else {
                    // 存储第二个字节(高字节)，并交换位置
                    if (index > 0) {
                        // 交换当前字节和前一个字节的位置
                        unsigned char temp = img_buffer[index-1];
                        img_buffer[index-1] = value;
                        img_buffer[index] = temp;
                    } else {
                        img_buffer[index] = value;
                    }
                }
                
                index++;
                p += 2;  // 跳过这两位数字
            } else {
                p++;  // 格式不匹配，跳过
            }
        } else {
            p++;  // 跳过非十六进制前缀
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

esp_err_t ImageResourceManager::CheckAndUpdateResources(const char* api_url, const char* version_url) {
    // 如果没有有效图片或者需要检查服务器版本
    if (!has_valid_images_) {
        ESP_LOGI(TAG, "未找到有效动画图片，需要下载");
        
        // 通知需要下载
        if (progress_callback_) {
            progress_callback_(0, 100, "首次启动，需要下载动画图片资源");
        }
        
        return DownloadImages(api_url);
    }
    
    // 检查服务器版本
    esp_err_t status = CheckServerVersion(version_url);
    if (status == ESP_OK) {
        ESP_LOGI(TAG, "发现新版本，需要更新动画图片资源");
        
        // 通知更新
        if (progress_callback_) {
            char message[128];
            snprintf(message, sizeof(message), "发现动画图片新版本，更新中... %s -> %s", 
                    local_version_.c_str(), server_version_.c_str());
            progress_callback_(0, 100, message);
        }
        
        return DownloadImages(api_url);
    } else if (status == ESP_ERR_NOT_FOUND) {
        ESP_LOGI(TAG, "动画图片资源已是最新版本");
        return ESP_ERR_NOT_FOUND;
    }
    
    return status;
}

esp_err_t ImageResourceManager::CheckAndUpdateLogo(const char* api_url, const char* logo_version_url) {
    // 如果没有有效logo或者需要检查服务器版本
    if (!has_valid_logo_) {
        ESP_LOGI(TAG, "未找到有效logo，需要下载");
        
        // 通知需要下载
        if (progress_callback_) {
            progress_callback_(0, 100, "首次启动，需要下载logo资源");
        }
        
        return DownloadLogo(api_url);
    }
    
    // 检查服务器logo版本
    esp_err_t status = CheckServerLogoVersion(logo_version_url);
    if (status == ESP_OK) {
        ESP_LOGI(TAG, "发现新logo版本，需要更新logo资源");
        
        // 通知更新
        if (progress_callback_) {
            char message[128];
            snprintf(message, sizeof(message), "发现logo新版本，更新中... %s -> %s", 
                    local_logo_version_.c_str(), server_logo_version_.c_str());
            progress_callback_(0, 100, message);
        }
        
        return DownloadLogo(api_url);
    } else if (status == ESP_ERR_NOT_FOUND) {
        ESP_LOGI(TAG, "logo资源已是最新版本");
        return ESP_ERR_NOT_FOUND;
    }
    
    return status;
}

const std::vector<const uint8_t*>& ImageResourceManager::GetImageArray() const {
    return image_array_;
}

const uint8_t* ImageResourceManager::GetLogoImage() const {
    return logo_data_;
}