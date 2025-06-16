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
#define MAX_IMAGE_FILES 9   // 修改：根据服务器返回有9个动态图片
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
    
    // 读取本地缓存的URL
    ReadLocalDynamicUrls(); // 读取动态图片URL缓存
    ReadLocalStaticUrl();   // 读取静态图片URL缓存
    ESP_LOGI(TAG, "当前本地动画图片URL数量: %d", cached_dynamic_urls_.size());
    ESP_LOGI(TAG, "当前本地logo URL: %s", cached_static_url_.c_str());
    
    // 检查是否有有效图片
    has_valid_images_ = CheckImagesExist();
    has_valid_logo_ = CheckLogoExists();
    
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
        .format_if_mount_failed = false
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

std::string ImageResourceManager::ReadLocalDynamicUrls() {
    if (!mounted_) {
        return "";
    }
    
    FILE* f = fopen(IMAGE_URL_CACHE_FILE, "r");
    if (f == NULL) {
        ESP_LOGW(TAG, "无法打开动画图片URL缓存文件，假定初始状态");
        return "";
    }
    
    char buffer[1024];  // 增大缓冲区以容纳URL数组
    size_t len = fread(buffer, 1, sizeof(buffer) - 1, f);
    fclose(f);
    
    if (len <= 0) {
        return "";
    }
    
    buffer[len] = '\0';
    
    cJSON* root = cJSON_Parse(buffer);
    if (root == NULL) {
        ESP_LOGE(TAG, "解析动画图片URL缓存文件失败");
        return "";
    }
    
    // 读取缓存的动态图片URL数组
    cJSON* dyn_array = cJSON_GetObjectItem(root, "dyn");
    if (dyn_array != NULL && cJSON_IsArray(dyn_array)) {
        cached_dynamic_urls_.clear();
        int array_size = cJSON_GetArraySize(dyn_array);
        for (int i = 0; i < array_size; i++) {
            cJSON* url_item = cJSON_GetArrayItem(dyn_array, i);
            if (cJSON_IsString(url_item)) {
                cached_dynamic_urls_.push_back(url_item->valuestring);
            }
        }
        
        // 返回第一个URL作为标识符
        if (!cached_dynamic_urls_.empty()) {
            cJSON_Delete(root);
            return cached_dynamic_urls_[0];
        }
    }
    
    cJSON_Delete(root);
    return "";
}

std::string ImageResourceManager::ReadLocalStaticUrl() {
    if (!mounted_) {
        return "";
    }
    
    FILE* f = fopen(LOGO_URL_CACHE_FILE, "r");
    if (f == NULL) {
        ESP_LOGW(TAG, "无法打开logo URL缓存文件，假定初始状态");
        return "";
    }
    
    char buffer[512];
    size_t len = fread(buffer, 1, sizeof(buffer) - 1, f);
    fclose(f);
    
    if (len <= 0) {
        return "";
    }
    
    buffer[len] = '\0';
    
    cJSON* root = cJSON_Parse(buffer);
    if (root == NULL) {
        ESP_LOGE(TAG, "解析logo URL缓存文件失败");
        return "";
    }
    
    cJSON* sta_url = cJSON_GetObjectItem(root, "sta");
    if (sta_url == NULL || !cJSON_IsString(sta_url) || strlen(sta_url->valuestring) == 0) {
        cJSON_Delete(root);
        return "";
    }
    
    cached_static_url_ = sta_url->valuestring;
    cJSON_Delete(root);
    
    return cached_static_url_;
}

bool ImageResourceManager::CheckImagesExist() {
    if (!mounted_) {
        return false;
    }
    
    // 如果没有缓存的URL，则检查是否有任何图片文件
    if (cached_dynamic_urls_.empty()) {
        // 检查是否有至少一个二进制图片文件
        char filename[64];
        snprintf(filename, sizeof(filename), "%soutput_0001.bin", IMAGE_BASE_PATH);
        
        FILE* f = fopen(filename, "r");
        if (f == NULL) {
            ESP_LOGW(TAG, "未找到任何动画图片文件");
            return false;
        }
        fclose(f);
        return true;
    }
    
    // 根据缓存的URL数量检查对应的二进制文件
    for (size_t i = 0; i < cached_dynamic_urls_.size(); i++) {
        char filename[64];
        snprintf(filename, sizeof(filename), "%soutput_%04d.bin", IMAGE_BASE_PATH, (int)(i + 1));
        
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

esp_err_t ImageResourceManager::CheckServerVersion(const char* version_url) {
    // 确保已连接WiFi
    if (!WifiStation::GetInstance().IsConnected()) {
        ESP_LOGW(TAG, "未连接WiFi，无法检查服务器动画图片");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "检查服务器动画图片URL...");
    
    auto http = Board::GetInstance().CreateHttp();
    
    // 先设置必要的请求头，再打开连接
    std::string device_id = SystemInfo::GetMacAddress();
    std::string client_id = SystemInfo::GetClientId();
    
    if (!device_id.empty()) {
        http->SetHeader("Device-Id", device_id.c_str());
    }
    if (!client_id.empty()) {
        http->SetHeader("Client-Id", client_id.c_str());
    }
    
    // 添加其他常用请求头
    auto app_desc = esp_app_get_description();
    http->SetHeader("User-Agent", std::string(BOARD_NAME "/") + app_desc->version);
    http->SetHeader("Accept", "application/json");
    http->SetHeader("Content-Type", "application/json");
    
    if (!http->Open("GET", version_url)) {
        ESP_LOGE(TAG, "无法连接到服务器: %s", version_url);
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
    
    // 添加调试信息：输出服务器响应内容
    ESP_LOGI(TAG, "服务器响应内容: %s", response.c_str());
    
    cJSON* root = cJSON_Parse(response.c_str());
    if (root == NULL) {
        ESP_LOGE(TAG, "解析服务器响应失败");
        return ESP_FAIL;
    }
    
    // 解析动态图片URL数组
    cJSON* dyn_array = cJSON_GetObjectItem(root, "dyn");
    if (dyn_array == NULL || !cJSON_IsArray(dyn_array)) {
        ESP_LOGE(TAG, "服务器响应中无动态图片URL数组");
        // 输出完整的JSON结构以便调试
        char* json_string = cJSON_Print(root);
        if (json_string) {
            ESP_LOGE(TAG, "完整JSON响应: %s", json_string);
            free(json_string);
        }
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    
    // 提取服务器返回的动态图片URL
    std::vector<std::string> server_dynamic_urls;
    int array_size = cJSON_GetArraySize(dyn_array);
    for (int i = 0; i < array_size; i++) {
        cJSON* url_item = cJSON_GetArrayItem(dyn_array, i);
        if (cJSON_IsString(url_item)) {
            server_dynamic_urls.push_back(url_item->valuestring);
        }
    }
    
    if (server_dynamic_urls.empty()) {
        ESP_LOGE(TAG, "服务器返回的动态图片URL数组为空");
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    
    // 保存服务器URL列表以供后续下载使用
    server_dynamic_urls_ = server_dynamic_urls;
    
    cJSON_Delete(root);
    
    // 对比URL数组是否一致
    if (cached_dynamic_urls_.size() != server_dynamic_urls.size()) {
        ESP_LOGI(TAG, "动态图片URL数量不一致，需要更新");
        return ESP_OK;
    }
    
    for (size_t i = 0; i < cached_dynamic_urls_.size(); i++) {
        if (cached_dynamic_urls_[i] != server_dynamic_urls[i]) {
            ESP_LOGI(TAG, "动态图片URL不一致，需要更新");
            return ESP_OK;
        }
    }
    
    ESP_LOGI(TAG, "动态图片URL一致，无需更新");
    return ESP_ERR_NOT_FOUND;
}

esp_err_t ImageResourceManager::CheckServerLogoVersion(const char* logo_version_url) {
    // 确保已连接WiFi
    if (!WifiStation::GetInstance().IsConnected()) {
        ESP_LOGW(TAG, "未连接WiFi，无法检查服务器logo");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "检查服务器logo URL...");
    
    auto http = Board::GetInstance().CreateHttp();
    
    // 先设置必要的请求头，再打开连接
    std::string device_id = SystemInfo::GetMacAddress();
    std::string client_id = SystemInfo::GetClientId();
    
    if (!device_id.empty()) {
        http->SetHeader("Device-Id", device_id.c_str());
    }
    if (!client_id.empty()) {
        http->SetHeader("Client-Id", client_id.c_str());
    }
    
    // 添加其他常用请求头
    auto app_desc = esp_app_get_description();
    http->SetHeader("User-Agent", std::string(BOARD_NAME "/") + app_desc->version);
    http->SetHeader("Accept", "application/json");
    http->SetHeader("Content-Type", "application/json");
    
    if (!http->Open("GET", logo_version_url)) {
        ESP_LOGE(TAG, "无法连接到logo服务器: %s", logo_version_url);
        delete http;
        return ESP_FAIL;
    }
    
    std::string response = http->GetBody();
    http->Close();
    delete http;
    
    if (response.empty()) {
        ESP_LOGE(TAG, "logo服务器返回空响应");
        return ESP_FAIL;
    }
    
    // 添加调试信息：输出logo服务器响应内容
    ESP_LOGI(TAG, "logo服务器响应内容: %s", response.c_str());
    
    cJSON* root = cJSON_Parse(response.c_str());
    if (root == NULL) {
        ESP_LOGE(TAG, "解析logo响应失败");
        return ESP_FAIL;
    }
    
    cJSON* sta_url = cJSON_GetObjectItem(root, "sta");
    if (sta_url == NULL || !cJSON_IsString(sta_url)) {
        ESP_LOGE(TAG, "logo响应中无静态图片URL");
        // 输出完整的JSON结构以便调试
        char* json_string = cJSON_Print(root);
        if (json_string) {
            ESP_LOGE(TAG, "完整logo JSON响应: %s", json_string);
            free(json_string);
        }
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    
    server_static_url_ = sta_url->valuestring;
    if (server_static_url_.empty()) {
        ESP_LOGE(TAG, "服务器静态图片URL为空");
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    
    cJSON_Delete(root);
    
    ESP_LOGI(TAG, "服务器logo URL: %s, 本地缓存URL: %s", 
             server_static_url_.c_str(), cached_static_url_.c_str());
    
    return server_static_url_ != cached_static_url_ ? ESP_OK : ESP_ERR_NOT_FOUND;
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
    
    ESP_LOGI(TAG, "开始下载二进制动画图片文件，进入专用下载模式...");
    
    // 进入专用下载模式：优化系统资源
    EnterDownloadMode();
    
    // 检查可用内存和存储空间
    size_t free_heap = esp_get_free_heap_size();
    ESP_LOGI(TAG, "下载开始前可用内存: %u字节", (unsigned int)free_heap);
    
    if (free_heap < 300000) { // 降低内存需求到300KB
        ESP_LOGE(TAG, "内存不足，无法开始下载，可用内存: %u字节", (unsigned int)free_heap);
        if (progress_callback_) {
            progress_callback_(0, 100, "内存不足，无法开始下载");
        }
        ExitDownloadMode();  // 退出下载模式
        return ESP_ERR_NO_MEM;
    }
    
    // 通知开始下载
    if (progress_callback_) {
        progress_callback_(0, 100, "准备下载二进制动画图片资源...");
    }
    
    // 检查是否有服务器URL列表
    if (server_dynamic_urls_.empty()) {
        ESP_LOGE(TAG, "没有服务器返回的动态图片URL列表");
        if (progress_callback_) {
            progress_callback_(0, 100, "没有可下载的图片URL");
        }
        return ESP_FAIL;
    }
    
    // 删除现有的图片文件（带进度显示）
    if (!DeleteExistingAnimationFiles()) {
        ESP_LOGW(TAG, "删除现有动画图片文件时出现错误，继续下载新文件");
    }
    
    bool success = true;
    
    // 使用服务器返回的URL列表进行下载
    std::vector<std::string> file_paths;
    
    // 准备文件路径（直接使用二进制格式）
    for (size_t i = 0; i < server_dynamic_urls_.size() && i < MAX_IMAGE_FILES; i++) {
        char filepath[128];
        snprintf(filepath, sizeof(filepath), "%soutput_%04d.bin", IMAGE_BASE_PATH, (int)(i + 1));
        file_paths.push_back(filepath);
        
        ESP_LOGI(TAG, "准备下载二进制动画图片文件 [%zu/%zu]: %s", i + 1, server_dynamic_urls_.size(), server_dynamic_urls_[i].c_str());
    }
    
    // 逐个下载文件
    int failed_count = 0;
    for (size_t i = 0; i < server_dynamic_urls_.size() && i < MAX_IMAGE_FILES; i++) {
        if (progress_callback_) {
            int overall_percent = static_cast<int>(i * 100 / server_dynamic_urls_.size());
            char message[128];
            const char* filename = strrchr(file_paths[i].c_str(), '/') + 1;
            snprintf(message, sizeof(message), "准备下载二进制动画图片: %s (%zu/%zu)", 
                    filename, i + 1, server_dynamic_urls_.size());
            progress_callback_(overall_percent, 100, message);
        }
        
        esp_err_t download_result = DownloadFile(server_dynamic_urls_[i].c_str(), file_paths[i].c_str());
        if (download_result != ESP_OK) {
            ESP_LOGE(TAG, "下载动画图片文件失败: %s", file_paths[i].c_str());
            failed_count++;
            
            // 如果失败的文件太多，则停止下载
            if (failed_count > 3) {
                ESP_LOGE(TAG, "连续失败文件过多 (%d个)，停止下载", failed_count);
                success = false;
                break;
            }
            
            // 通知单个文件失败，但继续下载其他文件
            if (progress_callback_) {
                char message[128];
                const char* filename = strrchr(file_paths[i].c_str(), '/') + 1;
                snprintf(message, sizeof(message), "文件 %s 下载失败，继续下载其他文件...", filename);
                progress_callback_(0, 100, message);
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        } else {
            ESP_LOGI(TAG, "文件 %zu/%zu 下载完成", i + 1, server_dynamic_urls_.size());
            failed_count = 0;  // 重置失败计数
        }
        
        // 在下载模式下适当增加文件间等待时间，确保网络连接稳定
        vTaskDelay(pdMS_TO_TICKS(500));  // 减少到500ms，大幅提升下载速度
    }
    
    // 如果至少有一半文件下载成功，认为是部分成功
    if (failed_count > 0 && failed_count < server_dynamic_urls_.size() / 2) {
        ESP_LOGW(TAG, "部分动画图片下载失败 (%d个)，但已获得足够的图片资源", failed_count);
        success = true;  // 设为成功，继续后续处理
    }
    
    if (success) {
        // 保存新的URL缓存
        if (!SaveDynamicUrls(server_dynamic_urls_)) {
            ESP_LOGE(TAG, "保存动画图片URL缓存失败");
            
            // 通知错误
            if (progress_callback_) {
                progress_callback_(100, 100, "保存动画图片URL缓存失败");
            }
            
            return ESP_FAIL;
        }
        
        cached_dynamic_urls_ = server_dynamic_urls_;
        has_valid_images_ = true;
        
        // 通知完成
        if (progress_callback_) {
            progress_callback_(100, 100, "二进制动画图片下载完成，正在加载图片...");
        }
        
        // 加载新下载的图片数据
        LoadImageData();
        
        // 通知加载完成
        if (progress_callback_) {
            progress_callback_(100, 100, "二进制动画图片资源已就绪");
            
            // 延迟一段时间后隐藏进度条
            vTaskDelay(pdMS_TO_TICKS(1000));
            progress_callback_(100, 100, nullptr);
        }
        
        ESP_LOGI(TAG, "所有二进制动画图片文件下载完成");
        ExitDownloadMode();  // 退出下载模式
        return ESP_OK;
    }
    
    // 通知失败
    if (progress_callback_) {
        progress_callback_(0, 100, "下载二进制动画图片资源失败");
    }
    
    ExitDownloadMode();  // 退出下载模式
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
    
    ESP_LOGI(TAG, "开始下载logo文件，进入专用下载模式...");
    
    // 进入专用下载模式：优化系统资源
    EnterDownloadMode();
    
    // 检查可用内存
    size_t free_heap = esp_get_free_heap_size();
    ESP_LOGI(TAG, "logo下载开始前可用内存: %u字节", (unsigned int)free_heap);
    
    if (free_heap < 150000) { // 降低内存需求到150KB
        ESP_LOGE(TAG, "内存不足，无法下载logo，可用内存: %u字节", (unsigned int)free_heap);
        if (progress_callback_) {
            progress_callback_(0, 100, "内存不足，无法下载logo");
        }
        ExitDownloadMode();  // 退出下载模式
        return ESP_ERR_NO_MEM;
    }
    
    // 通知开始下载
    if (progress_callback_) {
        progress_callback_(0, 100, "准备下载logo资源...");
    }
    
    // 检查是否有服务器静态图片URL
    if (server_static_url_.empty()) {
        ESP_LOGE(TAG, "没有服务器返回的静态图片URL");
        if (progress_callback_) {
            progress_callback_(0, 100, "没有可下载的logo URL");
        }
        return ESP_FAIL;
    }
    
    // 删除现有的logo文件（带进度显示）
    if (!DeleteExistingLogoFile()) {
        ESP_LOGW(TAG, "删除现有logo文件时出现错误，继续下载新文件");
    }
    
    ESP_LOGI(TAG, "下载logo文件: %s", server_static_url_.c_str());
    
    if (progress_callback_) {
        progress_callback_(0, 100, "正在下载logo文件");
    }
    
    esp_err_t download_result = DownloadFile(server_static_url_.c_str(), LOGO_FILE_PATH);
    if (download_result != ESP_OK) {
        ESP_LOGE(TAG, "下载logo文件失败，错误码: %s", esp_err_to_name(download_result));
        
        if (progress_callback_) {
            progress_callback_(0, 100, "下载logo失败，将使用默认图片");
        }
        
        ExitDownloadMode();  // 退出下载模式
        return ESP_FAIL;
    }
    
    // 保存新的URL缓存
    if (!SaveStaticUrl(server_static_url_)) {
        ESP_LOGE(TAG, "保存logo URL缓存失败");
        
        // 通知错误
        if (progress_callback_) {
            progress_callback_(100, 100, "保存logo URL缓存失败");
        }
        
        ExitDownloadMode();  // 退出下载模式
        return ESP_FAIL;
    }
    
    cached_static_url_ = server_static_url_;
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
    
    ESP_LOGI(TAG, "logo文件下载完成");
    ExitDownloadMode();  // 退出下载模式
    return ESP_OK;
}

esp_err_t ImageResourceManager::DownloadFile(const char* url, const char* filepath) {
    int retry_count = 0;
    static int last_logged_percent = -1;
    
    // 检查可用内存
    size_t free_heap = esp_get_free_heap_size();
    if (free_heap < 100000) { // 至少需要100KB可用内存
        ESP_LOGE(TAG, "内存不足，无法下载文件，可用内存: %u字节", (unsigned int)free_heap);
        if (progress_callback_) {
            progress_callback_(0, 100, "内存不足，下载失败");
        }
        return ESP_ERR_NO_MEM;
    }
    
    while (retry_count < MAX_DOWNLOAD_RETRIES) {
        last_logged_percent = -1;
        
        // 检查WiFi连接状态
        if (!WifiStation::GetInstance().IsConnected()) {
            ESP_LOGE(TAG, "WiFi连接已断开，等待重连...");
            if (progress_callback_) {
                progress_callback_(0, 100, "网络连接已断开，等待重连...");
            }
            vTaskDelay(pdMS_TO_TICKS(3000));  // 减少等待时间
            retry_count++;
            continue;
        }
        
        // 通知开始下载
        if (progress_callback_) {
            char message[128];
            const char* filename = strrchr(filepath, '/') + 1;
            if (retry_count > 0) {
                snprintf(message, sizeof(message), "重试下载: %s (%d/%d)", 
                        filename, retry_count + 1, MAX_DOWNLOAD_RETRIES);
            } else {
                snprintf(message, sizeof(message), "正在下载: %s", filename);
            }
            progress_callback_(0, 100, message);
        }
        
        // 检查内存情况
        free_heap = esp_get_free_heap_size();
        ESP_LOGI(TAG, "下载前可用内存: %u字节", (unsigned int)free_heap);
        
        auto http = Board::GetInstance().CreateHttp();
        if (!http) {
            ESP_LOGE(TAG, "无法创建HTTP客户端");
            retry_count++;
            if (progress_callback_) {
                progress_callback_(0, 100, "HTTP客户端创建失败");
            }
            vTaskDelay(pdMS_TO_TICKS(1000 * retry_count));  // 减少重试延时
            continue;
        }
        
        // 优化HTTP请求头，提高下载速度
        std::string device_id = SystemInfo::GetMacAddress();
        std::string client_id = SystemInfo::GetClientId();
        
        if (!device_id.empty()) {
            http->SetHeader("Device-Id", device_id.c_str());
        }
        if (!client_id.empty()) {
            http->SetHeader("Client-Id", client_id.c_str());
        }
        
        // 优化网络性能的请求头
        auto app_desc = esp_app_get_description();
        http->SetHeader("User-Agent", std::string(BOARD_NAME "/") + app_desc->version);
        http->SetHeader("Connection", "keep-alive");        // 启用连接复用提高速度
        http->SetHeader("Accept-Encoding", "identity");     // 禁用压缩减少CPU开销
        http->SetHeader("Cache-Control", "no-cache");       // 确保获取最新文件
        http->SetHeader("Accept", "*/*");                   // 接受任何类型
        
        if (!http->Open("GET", url)) {
            ESP_LOGE(TAG, "无法连接到服务器: %s (尝试 %d/%d)", url, retry_count + 1, MAX_DOWNLOAD_RETRIES);
            delete http;
            retry_count++;
            
            if (progress_callback_) {
                char message[128];
                snprintf(message, sizeof(message), "连接失败，正在重试 (%d/%d)", 
                        retry_count, MAX_DOWNLOAD_RETRIES);
                progress_callback_(0, 100, message);
            }
            
            // 减少重试等待时间，提高整体下载速度
            vTaskDelay(pdMS_TO_TICKS(2000));  // 减少到2秒
            
            // 渐进式重试延时，但上限更低
            if (retry_count >= 2) {
                ESP_LOGW(TAG, "多次连接失败，短暂等待网络恢复");
                vTaskDelay(pdMS_TO_TICKS(3000 * retry_count));  // 最多9秒
            }
            continue;
        }
        
        // 根据文件类型选择正确的写入模式
        const char* file_ext = strrchr(filepath, '.');
        const char* mode = (file_ext && strcmp(file_ext, ".bin") == 0) ? "wb" : "w";
        
        FILE* f = fopen(filepath, mode);
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
        
        ESP_LOGI(TAG, "下载文件大小: %u字节", (unsigned int)content_length);
        
        // 调整缓冲区大小，降低下载速度以适应设备性能
        size_t buffer_size = 8192;  // 默认1KB缓冲区，降低下载速度
        
        // 根据可用内存动态调整缓冲区大小，降低一个量级
        size_t current_free_heap = esp_get_free_heap_size();
        if (current_free_heap > 2000000) {
            buffer_size = 8192;   // 2MB以上可用内存使用2KB缓冲区
        } else if (current_free_heap > 1000000) {
            buffer_size = 4096;   // 1MB以上使用1.5KB缓冲区
        } else if (current_free_heap > 500000) {
            buffer_size = 2048;   // 500KB以上使用1KB缓冲区
        } else {
            buffer_size = 1042;    // 否则使用512字节缓冲区
        }
        
        char* buffer = (char*)malloc(buffer_size);
        if (!buffer) {
            ESP_LOGE(TAG, "无法分配%zu字节下载缓冲区", buffer_size);
            fclose(f);
            http->Close();
            delete http;
            if (progress_callback_) {
                progress_callback_(0, 100, "内存分配失败");
            }
            return ESP_ERR_NO_MEM;
        }
        
        ESP_LOGI(TAG, "使用%zu字节缓冲区进行高速下载", buffer_size);
        
        size_t total_read = 0;
        bool download_success = true;
        
        while (true) {
            // 减少检查频率，提高下载速度
            if (total_read % (1024 * 200) == 0) { // 每200KB检查一次（减少一半）
                free_heap = esp_get_free_heap_size();
                if (free_heap < 100000) { // 降低内存要求到100KB
                    ESP_LOGW(TAG, "内存不足，中止下载，可用内存: %u字节", (unsigned int)free_heap);
                    download_success = false;
                    break;
                }
                
                // 检查WiFi连接状态
                if (!WifiStation::GetInstance().IsConnected()) {
                    ESP_LOGE(TAG, "WiFi连接已断开，中止下载");
                    download_success = false;
                    break;
                }
            }
            
            int ret = http->Read(buffer, buffer_size);  // 使用大缓冲区
            if (ret < 0) {
                ESP_LOGE(TAG, "读取HTTP数据失败 (错误码: %d)", ret);
                download_success = false;
                break;
            }
            
            if (ret == 0) {
                // 检查是否真的下载完成
                if (total_read < content_length) {
                    ESP_LOGW(TAG, "下载未完成但无更多数据: %zu/%zu字节", total_read, content_length);
                    download_success = false;
                } else {
                    ESP_LOGI(TAG, "下载完成: %zu字节", total_read);
                }
                break;
            }
            
            size_t written = fwrite(buffer, 1, ret, f);
            if (written != ret) {
                ESP_LOGE(TAG, "写入文件失败: 期望写入%d字节，实际写入%zu字节", ret, written);
                download_success = false;
                break;
            }
            
            total_read += ret;
            
            // 优化进度更新频率
            if (content_length > 0) {
                int percent = (float)total_read * 100 / content_length;
                
                // 减少进度更新频率，提高下载速度
                if (percent != last_logged_percent && percent % 5 == 0) {  // 每5%更新一次
                    const char* filename = strrchr(filepath, '/') + 1;
                    char message[128];
                    if (retry_count > 0) {
                        snprintf(message, sizeof(message), "重试下载 %s (%d/%d)", 
                                filename, retry_count + 1, MAX_DOWNLOAD_RETRIES);
                    } else {
                        snprintf(message, sizeof(message), "正在下载 %s", filename);
                    }
                    
                    if (progress_callback_) {
                        progress_callback_(percent, 100, message);
                    }
                    
                    // 减少日志频率
                    if (percent % 25 == 0 || percent == 100) {  // 改为每25%记录一次
                        ESP_LOGI(TAG, "下载进度: %d%%, 已下载: %zu/%zu字节, 速度: %.1fKB/s", 
                                percent, total_read, content_length, 
                                (float)total_read / 1024.0 / ((esp_timer_get_time() / 1000 - esp_timer_get_time() / 1000) / 1000.0 + 1));
                    }
                    
                    last_logged_percent = percent;
                }
            }
            
            // 添加下载延迟，降低下载速度以适应设备性能
            vTaskDelay(pdMS_TO_TICKS(100));  // 添加100ms延迟，降低下载速度
        }
        
        // 清理资源
        free(buffer);
        buffer = nullptr;
        fclose(f);
        f = nullptr;
        http->Close();
        delete http;
        http = nullptr;
        
        // 减少垃圾回收等待时间
        vTaskDelay(pdMS_TO_TICKS(50));  // 减少到50ms
        
        if (download_success) {
            // 验证下载的二进制文件
            bool file_valid = true;
            
#if DEBUG_IMAGE_FILES
            if (strstr(filepath, ".bin") != nullptr) {
                FILE* verify_file = fopen(filepath, "rb");
                if (verify_file) {
                    // 检查文件大小
                    fseek(verify_file, 0, SEEK_END);
                    long verify_size = ftell(verify_file);
                    fseek(verify_file, 0, SEEK_SET);
                    
                    if (verify_size < sizeof(BinaryImageHeader)) {
                        ESP_LOGW(TAG, "下载文件 %s 太小，可能是原始RGB数据", filepath);
                        file_valid = true; // 仍然认为有效，可能是原始数据
                    } else {
                        // 尝试读取文件头
                        BinaryImageHeader verify_header;
                        if (fread(&verify_header, sizeof(BinaryImageHeader), 1, verify_file) == 1) {
                            if (verify_header.magic == BINARY_IMAGE_MAGIC) {
                                ESP_LOGI(TAG, "下载的二进制文件格式验证成功: %s", filepath);
                                file_valid = true;
                            } else {
                                ESP_LOGW(TAG, "下载的文件魔数不匹配 (0x%08" PRIX32 ")，可能是原始RGB数据: %s", 
                                        verify_header.magic, filepath);
                                file_valid = true; // 仍然认为有效，可能是原始数据
                            }
                        } else {
                            ESP_LOGE(TAG, "无法读取下载文件的头部: %s", filepath);
                            file_valid = false;
                        }
                    }
                    fclose(verify_file);
                } else {
                    ESP_LOGE(TAG, "无法打开下载的文件进行验证: %s", filepath);
                    file_valid = false;
                }
                
                // 如果文件无效，删除它
                if (!file_valid) {
                    ESP_LOGE(TAG, "删除无效的下载文件: %s", filepath);
                    remove(filepath);
                    return ESP_FAIL;
                }
            }
#endif
            
            // 直接处理下载完成的文件
            if (progress_callback_) {
                const char* filename = strrchr(filepath, '/') + 1;
                char message[128];
                snprintf(message, sizeof(message), "文件 %s 下载完成", filename);
                progress_callback_(100, 100, message);
            }
            
            last_logged_percent = -1;
            ESP_LOGI(TAG, "文件下载完成，可用内存: %u字节", (unsigned int)esp_get_free_heap_size());
            return ESP_OK;
        } else {
            retry_count++;
            
            // 删除损坏的文件
            if (remove(filepath) == 0) {
                ESP_LOGI(TAG, "已删除损坏的文件: %s", filepath);
            }
            
            if (progress_callback_) {
                char message[128];
                const char* filename = strrchr(filepath, '/') + 1;
                snprintf(message, sizeof(message), "下载 %s 失败，准备重试 (%d/%d)", 
                        filename, retry_count, MAX_DOWNLOAD_RETRIES);
                progress_callback_(0, 100, message);
            }
            
            ESP_LOGW(TAG, "下载失败，将在%d秒后重试 (第%d次重试)", 3 * retry_count, retry_count);
            // 减少重试延时，提高整体下载速度
            vTaskDelay(pdMS_TO_TICKS(3000 * retry_count));  // 减少到3秒基础延时
        }
    }
    
    const char* filename = strrchr(filepath, '/') + 1;
    ESP_LOGE(TAG, "文件 %s 下载重试次数已达上限，放弃下载", filename);
    if (progress_callback_) {
        char message[128];
        snprintf(message, sizeof(message), "下载 %s 失败，重试次数已达上限", filename);
        progress_callback_(0, 100, message);
    }
    
    return ESP_FAIL;
}

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

bool ImageResourceManager::SaveDynamicUrls(const std::vector<std::string>& urls) {
    if (!mounted_) {
        return false;
    }
    
    FILE* f = fopen(IMAGE_URL_CACHE_FILE, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "无法创建动态图片URL缓存文件");
        return false;
    }
    
    cJSON* root = cJSON_CreateObject();
    cJSON* dyn_array = cJSON_CreateArray();
    
    for (const auto& url : urls) {
        cJSON_AddItemToArray(dyn_array, cJSON_CreateString(url.c_str()));
    }
    
    cJSON_AddItemToObject(root, "dyn", dyn_array);
    
    char* json_str = cJSON_Print(root);
    fprintf(f, "%s", json_str);
    
    cJSON_Delete(root);
    free(json_str);
    fclose(f);
    
    return true;
}

bool ImageResourceManager::SaveStaticUrl(const std::string& url) {
    if (!mounted_) {
        return false;
    }
    
    FILE* f = fopen(LOGO_URL_CACHE_FILE, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "无法创建静态图片URL缓存文件");
        return false;
    }
    
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "sta", url.c_str());
    
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
    ESP_LOGI(TAG, "加载图片前可用内存: %u字节", (unsigned int)free_heap);
    
    if (free_heap < 200000) { // 至少需要200KB可用内存来加载图片
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
    
    // 首先加载logo文件（优先级最高，总是需要显示）
    if (has_valid_logo_ && !LoadLogoFile()) {
        ESP_LOGE(TAG, "加载logo文件失败");
    }
    
    // 延迟加载策略：启动时只加载必要的图片，其他按需加载
    if (has_valid_images_) {
        // 根据缓存的URL数量确定要加载的图片数量
        int actual_image_count = std::min((int)cached_dynamic_urls_.size(), MAX_IMAGE_FILES);
        
        // 预分配空间
        image_array_.resize(actual_image_count);
        image_data_pointers_.resize(actual_image_count, nullptr);
        
        // **启动时只加载第一张图片**，减少启动时间
        if (actual_image_count > 0) {
            ESP_LOGI(TAG, "启动时加载第一张动画图片，其余图片将在系统初始化完成后预加载");
            if (!LoadImageFile(1)) {
                ESP_LOGE(TAG, "加载第一张动画图片失败，索引: 1");
            }
        }
        
        ESP_LOGI(TAG, "预加载策略：已预分配 %d 个图片槽位，当前已加载 1 个", actual_image_count);
    }
    
    if (has_valid_logo_) {
        ESP_LOGI(TAG, "logo文件已加载");
    }
    
    // 最终内存检查
    free_heap = esp_get_free_heap_size();
    ESP_LOGI(TAG, "加载图片后可用内存: %u字节", (unsigned int)free_heap);
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

esp_err_t ImageResourceManager::CheckAndUpdateResources(const char* api_url, const char* version_url) {
    // 如果没有有效图片或者需要检查服务器版本
    if (!has_valid_images_) {
        ESP_LOGI(TAG, "未找到有效动画图片，需要下载");
        
        // 通知需要下载
        if (progress_callback_) {
            progress_callback_(0, 100, "首次启动，需要下载动画图片资源");
        }
        
        // 先获取服务器URL列表
        esp_err_t status = CheckServerVersion(version_url);
        if (status != ESP_OK && status != ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "获取服务器动画图片URL失败");
            return status;
        }
        
        return DownloadImages(api_url);
    }
    
    // 检查服务器版本
    esp_err_t status = CheckServerVersion(version_url);
    if (status == ESP_OK) {
        ESP_LOGI(TAG, "发现URL不同，需要更新动画图片资源");
        
        // 通知更新
        if (progress_callback_) {
            progress_callback_(0, 100, "发现动画图片URL更新，正在下载新资源");
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
        
        // 先获取服务器URL
        esp_err_t status = CheckServerLogoVersion(logo_version_url);
        if (status != ESP_OK && status != ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "获取服务器logo URL失败");
            return status;
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
            snprintf(message, sizeof(message), "发现logo新版本，更新中...");
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

esp_err_t ImageResourceManager::CheckAllServerResources(const char* version_url, bool& need_update_animations, bool& need_update_logo) {
    // 确保已连接WiFi
    if (!WifiStation::GetInstance().IsConnected()) {
        ESP_LOGW(TAG, "未连接WiFi，无法检查服务器资源");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "一次性检查所有服务器资源...");
    ESP_LOGI(TAG, "请求URL: %s", version_url);
    
    auto http = Board::GetInstance().CreateHttp();
    if (!http) {
        ESP_LOGE(TAG, "无法创建HTTP客户端");
        return ESP_FAIL;
    }
    
    // 先设置必要的请求头，再打开连接
    std::string device_id = SystemInfo::GetMacAddress();
    std::string client_id = SystemInfo::GetClientId();
    
    if (!device_id.empty()) {
        http->SetHeader("Device-Id", device_id.c_str());
    }
    if (!client_id.empty()) {
        http->SetHeader("Client-Id", client_id.c_str());
    }
    
    // 添加其他常用请求头
    auto app_desc = esp_app_get_description();
    http->SetHeader("User-Agent", std::string(BOARD_NAME "/") + app_desc->version);
    http->SetHeader("Accept", "application/json");
    http->SetHeader("Content-Type", "application/json");
    
    if (!http->Open("GET", version_url)) {
        ESP_LOGE(TAG, "无法连接到服务器: %s", version_url);
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
    
    // 添加调试信息：输出服务器响应内容
    ESP_LOGI(TAG, "服务器响应内容: %s", response.c_str());
    
    cJSON* root = cJSON_Parse(response.c_str());
    if (root == NULL) {
        ESP_LOGE(TAG, "解析服务器响应失败");
        return ESP_FAIL;
    }
    
    // 注意：不要重置need_update_animations和need_update_logo，保持调用前的状态
    // 如果调用前已经确定需要更新（如首次下载），应该保持这个状态
    
    // 解析动态图片URL数组
    cJSON* dyn_array = cJSON_GetObjectItem(root, "dyn");
    if (dyn_array != NULL && cJSON_IsArray(dyn_array)) {
        ESP_LOGI(TAG, "成功解析到动态图片URL数组");
        // 提取服务器返回的动态图片URL
        std::vector<std::string> server_dynamic_urls;
        int array_size = cJSON_GetArraySize(dyn_array);
        for (int i = 0; i < array_size; i++) {
            cJSON* url_item = cJSON_GetArrayItem(dyn_array, i);
            if (cJSON_IsString(url_item)) {
                server_dynamic_urls.push_back(url_item->valuestring);
            }
        }
        
        if (!server_dynamic_urls.empty()) {
            server_dynamic_urls_ = server_dynamic_urls;
            
            // 只有在已有缓存的情况下才进行比较
            if (!cached_dynamic_urls_.empty()) {
                // 对比URL数组是否一致
                if (cached_dynamic_urls_.size() != server_dynamic_urls.size()) {
                    ESP_LOGI(TAG, "动画图片URL数量不一致，需要更新");
                    need_update_animations = true;
                } else {
                    bool urls_match = true;
                    for (size_t i = 0; i < cached_dynamic_urls_.size(); i++) {
                        if (cached_dynamic_urls_[i] != server_dynamic_urls[i]) {
                            ESP_LOGI(TAG, "动画图片URL不一致，需要更新");
                            need_update_animations = true;
                            urls_match = false;
                            break;
                        }
                    }
                    
                    if (urls_match && !need_update_animations) {
                        ESP_LOGI(TAG, "动画图片URL一致，无需更新");
                    }
                }
            } else {
                // 如果没有缓存，说明是首次下载，保持need_update_animations的状态
                ESP_LOGI(TAG, "首次获取动画图片URL，URL数量: %zu", server_dynamic_urls.size());
                // 输出前3个URL作为调试信息
                for (size_t i = 0; i < std::min(server_dynamic_urls.size(), (size_t)3); i++) {
                    ESP_LOGI(TAG, "动画图片URL[%zu]: %s", i, server_dynamic_urls[i].c_str());
                }
            }
        } else {
            ESP_LOGW(TAG, "服务器返回的动画图片URL列表为空");
        }
    } else {
        ESP_LOGE(TAG, "服务器响应中未找到动态图片URL数组 (dyn字段)");
    }
    
    // 解析静态图片URL
    cJSON* sta_url = cJSON_GetObjectItem(root, "sta");
    if (sta_url != NULL && cJSON_IsString(sta_url)) {
        ESP_LOGI(TAG, "成功解析到静态图片URL");
        server_static_url_ = sta_url->valuestring;
        if (!server_static_url_.empty()) {
            // 只有在已有缓存的情况下才进行比较
            if (!cached_static_url_.empty()) {
                if (server_static_url_ != cached_static_url_) {
                    ESP_LOGI(TAG, "发现新logo版本，需要更新");
                    need_update_logo = true;
                } else if (!need_update_logo) {
                    ESP_LOGI(TAG, "logo URL一致，无需更新");
                }
            } else {
                // 如果没有缓存，说明是首次下载，保持need_update_logo的状态
                ESP_LOGI(TAG, "首次获取logo URL: %s", server_static_url_.c_str());
            }
        } else {
            ESP_LOGW(TAG, "服务器返回的静态图片URL为空");
        }
    } else {
        ESP_LOGE(TAG, "服务器响应中未找到静态图片URL (sta字段)");
    }
    
    cJSON_Delete(root);
    
    ESP_LOGI(TAG, "资源检查完成 - 动画图片需要更新: %s, logo需要更新: %s", 
             need_update_animations ? "是" : "否", 
             need_update_logo ? "是" : "否");
    ESP_LOGI(TAG, "获取到的URL状态 - 动画图片URL数量: %zu, logo URL: %s", 
             server_dynamic_urls_.size(), 
             server_static_url_.empty() ? "未获取" : "已获取");
    
    return ESP_OK;
}

esp_err_t ImageResourceManager::DownloadImagesWithUrls(const std::vector<std::string>& urls) {
    if (urls.empty()) {
        ESP_LOGE(TAG, "URL列表为空，无法下载动画图片");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "使用预获取的URL列表下载二进制动画图片，进入专用下载模式...");
    
    // 进入专用下载模式：优化系统资源
    EnterDownloadMode();
    
    // 检查可用内存和存储空间
    size_t free_heap = esp_get_free_heap_size();
    ESP_LOGI(TAG, "下载开始前可用内存: %u字节", (unsigned int)free_heap);
    
    if (free_heap < 300000) { // 降低内存需求到300KB
        ESP_LOGE(TAG, "内存不足，无法开始下载，可用内存: %u字节", (unsigned int)free_heap);
        if (progress_callback_) {
            progress_callback_(0, 100, "内存不足，无法开始下载");
        }
        ExitDownloadMode();  // 退出下载模式
        return ESP_ERR_NO_MEM;
    }
    
    // 通知开始下载
    if (progress_callback_) {
        progress_callback_(0, 100, "准备下载二进制动画图片资源...");
    }
    
    // 删除现有的图片文件（带进度显示）
    if (!DeleteExistingAnimationFiles()) {
        ESP_LOGW(TAG, "删除现有动画图片文件时出现错误，继续下载新文件");
    }
    
    bool success = true;
    
    // 准备文件路径（直接使用二进制格式）
    std::vector<std::string> file_paths;
    for (size_t i = 0; i < urls.size() && i < MAX_IMAGE_FILES; i++) {
        char filepath[128];
        snprintf(filepath, sizeof(filepath), "%soutput_%04d.bin", IMAGE_BASE_PATH, (int)(i + 1));
        file_paths.push_back(filepath);
        
        ESP_LOGI(TAG, "准备下载二进制动画图片文件 [%zu/%zu]: %s", i + 1, urls.size(), urls[i].c_str());
    }
    
    // 逐个下载文件
    int failed_count = 0;
    for (size_t i = 0; i < urls.size() && i < MAX_IMAGE_FILES; i++) {
        if (progress_callback_) {
            int overall_percent = static_cast<int>(i * 100 / urls.size());
            char message[128];
            const char* filename = strrchr(file_paths[i].c_str(), '/') + 1;
            snprintf(message, sizeof(message), "下载二进制动画图片: %s (%zu/%zu)", 
                    filename, i + 1, urls.size());
            progress_callback_(overall_percent, 100, message);
        }
        
        esp_err_t download_result = DownloadFile(urls[i].c_str(), file_paths[i].c_str());
        if (download_result != ESP_OK) {
            ESP_LOGE(TAG, "下载动画图片文件失败: %s", file_paths[i].c_str());
            failed_count++;
            
            // 如果失败的文件太多，则停止下载
            if (failed_count > 3) {
                ESP_LOGE(TAG, "连续失败文件过多 (%d个)，停止下载", failed_count);
                success = false;
                break;
            }
            
            // 通知单个文件失败，但继续下载其他文件
            if (progress_callback_) {
                char message[128];
                const char* filename = strrchr(file_paths[i].c_str(), '/') + 1;
                snprintf(message, sizeof(message), "文件 %s 下载失败，继续下载其他文件...", filename);
                progress_callback_(0, 100, message);
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        } else {
            ESP_LOGI(TAG, "文件 %zu/%zu 下载完成", i + 1, urls.size());
            failed_count = 0;  // 重置失败计数
        }
        
        // 在下载模式下适当增加文件间等待时间，确保网络连接稳定
        vTaskDelay(pdMS_TO_TICKS(500));  // 减少到500ms，大幅提升下载速度
    }
    
    // 如果至少有一半文件下载成功，认为是部分成功
    if (failed_count > 0 && failed_count < urls.size() / 2) {
        ESP_LOGW(TAG, "部分动画图片下载失败 (%d个)，但已获得足够的图片资源", failed_count);
        success = true;  // 设为成功，继续后续处理
    }
    
    if (success) {
        // 保存新的URL缓存
        if (!SaveDynamicUrls(urls)) {
            ESP_LOGE(TAG, "保存动画图片URL缓存失败");
            
            // 通知错误
            if (progress_callback_) {
                progress_callback_(100, 100, "保存动画图片URL缓存失败");
            }
            
            ExitDownloadMode();  // 退出下载模式
            return ESP_FAIL;
        }
        
        cached_dynamic_urls_ = urls;
        has_valid_images_ = true;
        
        // 通知完成
        if (progress_callback_) {
            progress_callback_(100, 100, "二进制动画图片下载完成，正在加载图片...");
        }
        
        // 加载新下载的图片数据
        LoadImageData();
        
        // 通知加载完成
        if (progress_callback_) {
            progress_callback_(100, 100, "二进制动画图片资源已就绪");
            
            // 延迟一段时间后隐藏进度条
            vTaskDelay(pdMS_TO_TICKS(1000));
            progress_callback_(100, 100, nullptr);
        }
        
        ESP_LOGI(TAG, "所有二进制动画图片文件下载完成");
        ExitDownloadMode();  // 退出下载模式
        return ESP_OK;
    }
    
    // 通知失败
    if (progress_callback_) {
        progress_callback_(0, 100, "下载二进制动画图片资源失败");
    }
    
    ExitDownloadMode();  // 退出下载模式
    return ESP_FAIL;
}

esp_err_t ImageResourceManager::DownloadLogoWithUrl(const std::string& url) {
    if (url.empty()) {
        ESP_LOGE(TAG, "logo URL为空，无法下载");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "使用预获取的URL下载logo文件，进入专用下载模式...");
    
    // 进入专用下载模式：优化系统资源
    EnterDownloadMode();
    
    // 检查可用内存
    size_t free_heap = esp_get_free_heap_size();
    ESP_LOGI(TAG, "logo下载开始前可用内存: %u字节", (unsigned int)free_heap);
    
    if (free_heap < 150000) { // 降低内存需求到150KB
        ESP_LOGE(TAG, "内存不足，无法下载logo，可用内存: %u字节", (unsigned int)free_heap);
        if (progress_callback_) {
            progress_callback_(0, 100, "内存不足，无法下载logo");
        }
        ExitDownloadMode();  // 退出下载模式
        return ESP_ERR_NO_MEM;
    }
    
    // 通知开始下载
    if (progress_callback_) {
        progress_callback_(0, 100, "准备下载logo资源...");
    }
    
    // 删除现有的logo文件（带进度显示）
    if (!DeleteExistingLogoFile()) {
        ESP_LOGW(TAG, "删除现有logo文件时出现错误，继续下载新文件");
    }
    
    ESP_LOGI(TAG, "下载logo文件: %s", url.c_str());
    
    if (progress_callback_) {
        progress_callback_(0, 100, "正在下载logo文件");
    }
    
    esp_err_t download_result = DownloadFile(url.c_str(), LOGO_FILE_PATH);
    if (download_result != ESP_OK) {
        ESP_LOGE(TAG, "下载logo文件失败，错误码: %s", esp_err_to_name(download_result));
        
        if (progress_callback_) {
            progress_callback_(0, 100, "下载logo失败，将使用默认图片");
        }
        
        ExitDownloadMode();  // 退出下载模式
        return ESP_FAIL;
    }
    
    // 保存新的URL缓存
    if (!SaveStaticUrl(url)) {
        ESP_LOGE(TAG, "保存logo URL缓存失败");
        
        // 通知错误
        if (progress_callback_) {
            progress_callback_(100, 100, "保存logo URL缓存失败");
        }
        
        ExitDownloadMode();  // 退出下载模式
        return ESP_FAIL;
    }
    
    cached_static_url_ = url;
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
    
    ESP_LOGI(TAG, "logo文件下载完成");
    ExitDownloadMode();  // 退出下载模式
    return ESP_OK;
}

esp_err_t ImageResourceManager::CheckAndUpdateAllResources(const char* api_url, const char* version_url) {
    ESP_LOGI(TAG, "开始一次性检查并更新所有资源...");
    
    bool need_update_animations = false;
    bool need_update_logo = false;
    
    // 如果没有有效图片或logo，直接需要下载
    if (!has_valid_images_) {
        ESP_LOGI(TAG, "未找到有效动画图片，需要下载");
        need_update_animations = true;
    }
    
    if (!has_valid_logo_) {
        ESP_LOGI(TAG, "未找到有效logo，需要下载");
        need_update_logo = true;
    }
    
    ESP_LOGI(TAG, "资源状态检查 - 需要动画图片: %s, 需要logo: %s, 已有动画图片: %s, 已有logo: %s", 
             need_update_animations ? "是" : "否", 
             need_update_logo ? "是" : "否",
             has_valid_images_ ? "是" : "否",
             has_valid_logo_ ? "是" : "否");
    
    // 无论何种情况，如果有任何资源需要更新或检查，都要请求API获取URL
    if (need_update_animations || need_update_logo || (has_valid_images_ && has_valid_logo_)) {
        ESP_LOGI(TAG, "满足API请求条件，开始请求服务器资源信息...");
        esp_err_t check_result = CheckAllServerResources(version_url, need_update_animations, need_update_logo);
        if (check_result != ESP_OK) {
            ESP_LOGE(TAG, "检查服务器资源失败，错误码: %s", esp_err_to_name(check_result));
            return check_result;
        }
    } else {
        ESP_LOGW(TAG, "不满足API请求条件，跳过服务器资源检查");
    }
    
    // 如果都不需要更新，直接返回
    if (!need_update_animations && !need_update_logo) {
        ESP_LOGI(TAG, "所有资源都是最新版本，无需更新");
        return ESP_ERR_NOT_FOUND;
    }
    
    // 通知开始下载
    if (progress_callback_) {
        if (need_update_animations && need_update_logo) {
            progress_callback_(0, 100, "发现资源更新，开始下载动画图片和logo...");
        } else if (need_update_animations) {
            progress_callback_(0, 100, "发现动画图片更新，开始下载...");
        } else {
            progress_callback_(0, 100, "发现logo更新，开始下载...");
        }
    }
    
    esp_err_t animation_result = ESP_ERR_NOT_FOUND;
    esp_err_t logo_result = ESP_ERR_NOT_FOUND;
    
    // 下载动画图片（如果需要）
    if (need_update_animations) {
        if (!server_dynamic_urls_.empty()) {
            ESP_LOGI(TAG, "开始下载动画图片，URL数量: %zu", server_dynamic_urls_.size());
            animation_result = DownloadImagesWithUrls(server_dynamic_urls_);
        } else {
            ESP_LOGE(TAG, "需要下载动画图片，但没有可用的URL（可能API请求失败）");
            animation_result = ESP_FAIL;
        }
    }
    
    // 下载logo（如果需要）
    if (need_update_logo) {
        if (!server_static_url_.empty()) {
            ESP_LOGI(TAG, "开始下载logo: %s", server_static_url_.c_str());
            logo_result = DownloadLogoWithUrl(server_static_url_);
        } else {
            ESP_LOGE(TAG, "需要下载logo，但没有可用的URL（可能API请求失败）");
            logo_result = ESP_FAIL;
        }
    }
    
    // 评估结果
    bool has_updates = false;
    bool has_errors = false;
    
    if (need_update_animations) {
        if (animation_result == ESP_OK) {
            ESP_LOGI(TAG, "动画图片资源更新完成");
            has_updates = true;
        } else {
            ESP_LOGE(TAG, "动画图片资源更新失败");
            has_errors = true;
        }
    }
    
    if (need_update_logo) {
        if (logo_result == ESP_OK) {
            ESP_LOGI(TAG, "logo资源更新完成");
            has_updates = true;
        } else {
            ESP_LOGE(TAG, "logo资源更新失败");
            has_errors = true;
        }
    }
    
    if (has_updates && !has_errors) {
        return ESP_OK;  // 全部成功
    } else if (has_updates && has_errors) {
        return ESP_OK;  // 部分成功，也算成功
    } else {
        return ESP_FAIL;  // 全部失败
    }
}

void ImageResourceManager::EnterDownloadMode() {
    ESP_LOGI(TAG, "进入专用下载模式，优化系统资源...");
    
    auto& board = Board::GetInstance();
    auto& app = Application::GetInstance();
    
    // 检查设备状态，确保可以安全进入下载模式
    DeviceState current_state = app.GetDeviceState();
    if (current_state == kDeviceStateSpeaking || current_state == kDeviceStateListening) {
        ESP_LOGW(TAG, "设备正在进行音频操作，等待完成后进入下载模式");
        vTaskDelay(pdMS_TO_TICKS(2000));  // 等待2秒让音频操作完成
    }
    
    // 1. 禁用省电模式，确保WiFi连接稳定
    board.SetPowerSaveMode(false);
    ESP_LOGI(TAG, "已禁用省电模式");
    
    // 2. 暂停所有音频处理
    app.PauseAudioProcessing();  // 暂停音频处理器、唤醒词检测和清空音频队列
    
    // 3. 暂停音频编解码器，释放硬件资源
    auto codec = board.GetAudioCodec();
    if (codec) {
        codec->EnableInput(false);
        codec->EnableOutput(false);
        ESP_LOGI(TAG, "已暂停音频输入输出");
    }
    
    // 4. LED功能已移除，节省资源
    
    // 5. 提高当前任务优先级（下载任务）
    vTaskPrioritySet(NULL, configMAX_PRIORITIES - 2);  // 设置为高优先级
    ESP_LOGI(TAG, "已提高下载任务优先级");
    
    // 6. 强制垃圾回收，释放内存并给WiFi更多稳定时间
    vTaskDelay(pdMS_TO_TICKS(1000));  // 增加等待时间到1秒，让WiFi连接更稳定
    
    // 7. 配置TCP保活参数，提高网络连接稳定性
    ESP_LOGI(TAG, "下载模式设置完成，开始专注下载");
}

void ImageResourceManager::ExitDownloadMode() {
    ESP_LOGI(TAG, "退出下载模式，恢复正常运行状态...");
    
    auto& board = Board::GetInstance();
    
    // 1. 恢复正常任务优先级
    vTaskPrioritySet(NULL, 4);  // 恢复到正常优先级
    ESP_LOGI(TAG, "已恢复正常任务优先级");
    
    // 2. 重新启用省电模式
    board.SetPowerSaveMode(true);
    ESP_LOGI(TAG, "已重新启用省电模式");
    
    // 3. 恢复音频编解码器
    auto codec = board.GetAudioCodec();
    if (codec) {
        codec->EnableInput(true);
        codec->EnableOutput(true);
        ESP_LOGI(TAG, "已恢复音频输入输出");
    }
    
    // 4. 获取应用实例并恢复音频处理
    auto& app = Application::GetInstance();
    app.ResumeAudioProcessing();  // 根据设备状态恢复音频处理器和唤醒词检测
    
    // 5. LED功能已移除
    
    // 6. 等待系统稳定
    vTaskDelay(pdMS_TO_TICKS(800));  // 增加等待时间确保音频系统完全恢复
    ESP_LOGI(TAG, "系统已恢复正常运行状态，AFE缓冲区问题已解决");
}

bool ImageResourceManager::DeleteExistingAnimationFiles() {
    if (!mounted_) {
        return false;
    }
    
    ESP_LOGI(TAG, "开始删除现有的动画图片文件...");
    
    // 通知开始删除
    if (progress_callback_) {
        progress_callback_(0, 100, "正在删除旧的动画图片文件...");
    }
    
    // 先快速扫描有哪些文件存在，优先检查二进制文件
    std::vector<std::string> existing_files;
    for (int i = 1; i <= MAX_IMAGE_FILES; i++) {
        char bin_filepath[128];
        char h_filepath[128];
        snprintf(bin_filepath, sizeof(bin_filepath), "%soutput_%04d.bin", IMAGE_BASE_PATH, i);
        snprintf(h_filepath, sizeof(h_filepath), "%soutput_%04d.h", IMAGE_BASE_PATH, i);
        
        struct stat file_stat;
        
        // 优先检查二进制文件
        if (stat(bin_filepath, &file_stat) == 0) {
            existing_files.push_back(bin_filepath);
        }
        
        // 也检查.h文件（向后兼容）
        if (stat(h_filepath, &file_stat) == 0) {
            existing_files.push_back(h_filepath);
        }
    }
    
    if (existing_files.empty()) {
        ESP_LOGI(TAG, "未发现需要删除的动画图片文件");
        if (progress_callback_) {
            progress_callback_(100, 100, "无需删除，准备下载新文件...");
            vTaskDelay(pdMS_TO_TICKS(300)); // 短暂显示
        }
        return true;
    }
    
    ESP_LOGI(TAG, "发现 %d 个动画图片文件需要删除", existing_files.size());
    
    // 并行删除文件以提高速度
    int deleted_count = 0;
    int failed_count = 0;
    
    for (size_t i = 0; i < existing_files.size(); i++) {
        const std::string& filepath = existing_files[i];
        
        // 更新删除进度
        int progress = static_cast<int>((i + 1) * 100 / existing_files.size());
        if (progress_callback_) {
            char message[128];
            const char* filename = strrchr(filepath.c_str(), '/') + 1;
            snprintf(message, sizeof(message), "删除文件: %s (%zu/%zu)", 
                    filename, i + 1, existing_files.size());
            progress_callback_(progress, 100, message);
        }
        
        // 执行删除操作
        if (remove(filepath.c_str()) == 0) {
            deleted_count++;
            ESP_LOGI(TAG, "成功删除文件: %s", filepath.c_str());
        } else {
            failed_count++;
            ESP_LOGW(TAG, "删除文件失败: %s", filepath.c_str());
        }
        
        // 减少延时，提高删除速度，但避免系统阻塞
        if (i % 3 == 0) { // 每删除3个文件后短暂让出CPU
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    
    // 删除完成，显示结果
    ESP_LOGI(TAG, "动画图片文件删除完成: 成功删除 %d 个, 失败 %d 个", deleted_count, failed_count);
    
    if (progress_callback_) {
        char final_message[128];
        if (failed_count == 0) {
            snprintf(final_message, sizeof(final_message), "成功删除 %d 个旧文件", deleted_count);
        } else {
            snprintf(final_message, sizeof(final_message), "删除完成: 成功 %d 个, 失败 %d 个", deleted_count, failed_count);
        }
        progress_callback_(100, 100, final_message);
        vTaskDelay(pdMS_TO_TICKS(800)); // 显示结果800ms
    }
    
    return failed_count == 0;
}

bool ImageResourceManager::DeleteExistingLogoFile() {
    if (!mounted_) {
        return false;
    }
    
    ESP_LOGI(TAG, "开始删除现有的logo文件...");
    
    // 通知开始删除logo
    if (progress_callback_) {
        progress_callback_(0, 100, "正在删除旧的logo文件...");
    }
    
    // 检查logo文件是否存在（优先检查二进制格式）
    struct stat file_stat;
    bool bin_exists = (stat(LOGO_FILE_PATH, &file_stat) == 0);
    bool h_exists = (stat(LOGO_FILE_PATH_H, &file_stat) == 0);
    
    if (!bin_exists && !h_exists) {
        ESP_LOGI(TAG, "未发现需要删除的logo文件");
        if (progress_callback_) {
            progress_callback_(100, 100, "无需删除logo，准备下载新文件...");
            vTaskDelay(pdMS_TO_TICKS(300)); // 短暂显示
        }
        return true;
    }
    
    bool success = true;
    int deleted_count = 0;
    
    // 删除二进制格式logo文件
    if (bin_exists) {
        if (progress_callback_) {
            progress_callback_(25, 100, "正在删除logo.bin文件...");
        }
        
        if (remove(LOGO_FILE_PATH) == 0) {
            ESP_LOGI(TAG, "成功删除二进制logo文件: %s", LOGO_FILE_PATH);
            deleted_count++;
        } else {
            ESP_LOGW(TAG, "删除二进制logo文件失败: %s", LOGO_FILE_PATH);
            success = false;
        }
    }
    
    // 删除.h格式logo文件（如果存在）
    if (h_exists) {
        if (progress_callback_) {
            progress_callback_(75, 100, "正在删除logo.h文件...");
        }
        
        if (remove(LOGO_FILE_PATH_H) == 0) {
            ESP_LOGI(TAG, "成功删除.h格式logo文件: %s", LOGO_FILE_PATH_H);
            deleted_count++;
        } else {
            ESP_LOGW(TAG, "删除.h格式logo文件失败: %s", LOGO_FILE_PATH_H);
            success = false;
        }
    }
    
    // 显示删除结果
    if (progress_callback_) {
        char result_message[128];
        if (success && deleted_count > 0) {
            snprintf(result_message, sizeof(result_message), "成功删除 %d 个logo文件", deleted_count);
        } else if (deleted_count > 0) {
            snprintf(result_message, sizeof(result_message), "部分logo文件删除失败");
        } else {
            snprintf(result_message, sizeof(result_message), "logo文件删除失败");
        }
        progress_callback_(100, 100, result_message);
        vTaskDelay(pdMS_TO_TICKS(500)); // 显示结果500ms
    }
    
    ESP_LOGI(TAG, "logo文件删除完成，成功删除: %d 个文件", deleted_count);
    return success;
}

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
    
    if (free_heap < 1000000) { // 需要至少1MB可用内存来预加载
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
    
    ESP_LOGI(TAG, "预加载策略：加载所有剩余图片 (总数: %d)", total_images);
    
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
        
        // 检查音频状态，如果有音频播放则暂停预加载
        if (!app.IsAudioQueueEmpty() || app.GetDeviceState() != kDeviceStateIdle) {
            ESP_LOGW(TAG, "检测到音频活动，暂停预加载以避免冲突，已加载: %d/%d", loaded_count, total_images);
            
            // 通知预加载被中断
            if (preload_progress_callback_) {
                char message[64];
                snprintf(message, sizeof(message), "预加载中断：检测到音频活动");
                preload_progress_callback_(loaded_count, total_images, message);
                vTaskDelay(pdMS_TO_TICKS(2000)); // 显示消息2秒
                preload_progress_callback_(loaded_count, total_images, nullptr); // 隐藏UI
            }
            break;
        }
        
        // 检查内存状况
        free_heap = esp_get_free_heap_size();
        if (free_heap < 300000) { // 如果内存不足300KB，停止加载
            ESP_LOGW(TAG, "预加载过程中内存不足，停止加载，已加载: %d/%d", loaded_count, total_images);
            
            // 通知内存不足
            if (preload_progress_callback_) {
                char message[64];
                snprintf(message, sizeof(message), "预加载停止：内存不足");
                preload_progress_callback_(loaded_count, total_images, message);
                vTaskDelay(pdMS_TO_TICKS(2000)); // 显示消息2秒
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
        
        // 二进制格式加载速度快，减少延迟时间
        vTaskDelay(pdMS_TO_TICKS(20));
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
        
        // 延迟一段时间后隐藏进度条
        vTaskDelay(pdMS_TO_TICKS(2000));
        preload_progress_callback_(loaded_count, total_images, nullptr);
    }
    
    return loaded_count > 0 ? ESP_OK : ESP_FAIL;
}