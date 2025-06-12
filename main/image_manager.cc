#include "image_manager.h"
#include <esp_log.h>
#include <esp_spiffs.h>
#include <esp_http_client.h>
#include <string.h>
#include <sys/stat.h>
#include <cJSON.h>
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
#define LOGO_FILE_PATH "/resources/images/logo.h"
#define MAX_IMAGE_FILES 9   // 修改：根据服务器返回有9个动态图片
#define MAX_DOWNLOAD_RETRIES 3  // 设置合理的重试次数为3次

ImageResourceManager::ImageResourceManager() {
    mounted_ = false;
    initialized_ = false;
    has_valid_images_ = false;
    has_valid_logo_ = false;  
    logo_data_ = nullptr;
    cached_static_url_ = "";     // 缓存的静态图片URL
    cached_dynamic_urls_.clear(); // 缓存的动态图片URL列表
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
        // 检查是否有至少一个图片文件
        char filename[64];
        snprintf(filename, sizeof(filename), "%soutput_0001.h", IMAGE_BASE_PATH);
        
        FILE* f = fopen(filename, "r");
        if (f == NULL) {
            ESP_LOGW(TAG, "未找到任何动画图片文件");
            return false;
        }
        fclose(f);
        return true;
    }
    
    // 根据缓存的URL数量检查对应的文件
    for (size_t i = 0; i < cached_dynamic_urls_.size(); i++) {
        char filename[64];
        snprintf(filename, sizeof(filename), "%soutput_%04d.h", IMAGE_BASE_PATH, (int)(i + 1));
        
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
    
    ESP_LOGI(TAG, "开始下载动画图片文件，进入专用下载模式...");
    
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
        progress_callback_(0, 100, "准备下载动画图片资源...");
    }
    
    // 检查是否有服务器URL列表
    if (server_dynamic_urls_.empty()) {
        ESP_LOGE(TAG, "没有服务器返回的动态图片URL列表");
        if (progress_callback_) {
            progress_callback_(0, 100, "没有可下载的图片URL");
        }
        return ESP_FAIL;
    }
    
    // 清空现有的图片文件
    ESP_LOGI(TAG, "清空现有的动画图片文件...");
    for (int i = 1; i <= MAX_IMAGE_FILES; i++) {
        char filepath[128];
        snprintf(filepath, sizeof(filepath), "%soutput_%04d.h", IMAGE_BASE_PATH, i);
        
        struct stat file_stat;
        if (stat(filepath, &file_stat) == 0) {
            if (remove(filepath) == 0) {
                ESP_LOGI(TAG, "删除现有文件: %s", filepath);
            } else {
                ESP_LOGW(TAG, "删除文件失败: %s", filepath);
            }
        }
    }
    
    bool success = true;
    
    // 使用服务器返回的URL列表进行下载
    std::vector<std::string> file_paths;
    
    // 准备文件路径
    for (size_t i = 0; i < server_dynamic_urls_.size() && i < MAX_IMAGE_FILES; i++) {
        char filepath[128];
        snprintf(filepath, sizeof(filepath), "%soutput_%04d.h", IMAGE_BASE_PATH, (int)(i + 1));
        file_paths.push_back(filepath);
        
        ESP_LOGI(TAG, "准备下载动画图片文件 [%zu/%zu]: %s", i + 1, server_dynamic_urls_.size(), server_dynamic_urls_[i].c_str());
    }
    
    // 逐个下载文件
    int failed_count = 0;
    for (size_t i = 0; i < server_dynamic_urls_.size() && i < MAX_IMAGE_FILES; i++) {
        if (progress_callback_) {
            int overall_percent = static_cast<int>(i * 100 / server_dynamic_urls_.size());
            char message[128];
            const char* filename = strrchr(file_paths[i].c_str(), '/') + 1;
            snprintf(message, sizeof(message), "准备下载动画图片: %s (%zu/%zu)", 
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
        vTaskDelay(pdMS_TO_TICKS(3000));  // 增加到3秒，给网络连接恢复时间
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
        
        ESP_LOGI(TAG, "所有动画图片文件下载完成");
        ExitDownloadMode();  // 退出下载模式
        return ESP_OK;
    }
    
    // 通知失败
    if (progress_callback_) {
        progress_callback_(0, 100, "下载动画图片资源失败");
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
    
    // 清空现有的logo文件
    struct stat file_stat;
    if (stat(LOGO_FILE_PATH, &file_stat) == 0) {
        if (remove(LOGO_FILE_PATH) == 0) {
            ESP_LOGI(TAG, "删除现有logo文件: %s", LOGO_FILE_PATH);
        } else {
            ESP_LOGW(TAG, "删除logo文件失败: %s", LOGO_FILE_PATH);
        }
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
            vTaskDelay(pdMS_TO_TICKS(5000));
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
            vTaskDelay(pdMS_TO_TICKS(2000 * retry_count));
            continue;
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
        http->SetHeader("Connection", "close");  // 强制关闭连接，避免连接复用问题
        http->SetHeader("Accept-Encoding", "identity");  // 禁用压缩，减少处理开销
        
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
            
            // 重试前强制垃圾回收和等待网络恢复
            vTaskDelay(pdMS_TO_TICKS(5000));  // 基础等待5秒
            
            // 如果是网络超时问题，增加额外等待时间
            if (retry_count >= 2) {
                ESP_LOGW(TAG, "多次连接失败，延长等待时间以让网络恢复");
                vTaskDelay(pdMS_TO_TICKS(10000 * retry_count));  // 渐进式增加等待时间
            }
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
        
        ESP_LOGI(TAG, "下载文件大小: %u字节", (unsigned int)content_length);
        
        // 根据网络状况动态调整缓冲区大小
        size_t buffer_size = 2048;  // 降低默认缓冲区，提高稳定性
        
        // 根据可用内存动态调整缓冲区大小，更保守的设置
        size_t current_free_heap = esp_get_free_heap_size();
        if (current_free_heap > 1000000) {
            buffer_size = 4096;  // 1MB以上可用内存使用4KB缓冲区
        } else if (current_free_heap > 500000) {
            buffer_size = 2048;  // 500KB以上使用2KB缓冲区
        } else {
            buffer_size = 1024;  // 否则使用1KB缓冲区，确保稳定性
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
        
        ESP_LOGI(TAG, "使用%zu字节缓冲区进行下载", buffer_size);
        
        size_t total_read = 0;
        bool download_success = true;
        
        while (true) {
            // 定期检查内存和网络连接（降低检查频率减少系统开销）
            if (total_read % (1024 * 100) == 0) { // 每100KB检查一次
                free_heap = esp_get_free_heap_size();
                if (free_heap < 150000) { // 提高内存要求到150KB
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
            
            int ret = http->Read(buffer, buffer_size);  // 使用动态缓冲区
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
            
            // 更频繁地更新进度显示
            if (content_length > 0) {
                int percent = (float)total_read * 100 / content_length;
                
                // 每当进度变化时都更新显示
                if (percent != last_logged_percent) {
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
                    
                    // 记录日志（但减少日志频率）
                    if (percent % 20 == 0 || percent == 100) {  // 改为每20%记录一次
                        ESP_LOGI(TAG, "下载进度: %d%%, 已下载: %zu/%zu字节, 可用内存: %u字节", 
                                percent, total_read, content_length, (unsigned int)esp_get_free_heap_size());
                    }
                    
                    last_logged_percent = percent;
                }
            }
            
            // 在下载模式下适度延迟，平衡速度和稳定性
            vTaskDelay(pdMS_TO_TICKS(10));  // 10ms延迟确保系统稳定
        }
        
        // 清理资源
        free(buffer);
        buffer = nullptr;
        fclose(f);
        f = nullptr;
        http->Close();
        delete http;
        http = nullptr;
        
        // 强制垃圾回收，释放内存
        vTaskDelay(pdMS_TO_TICKS(200));  // 增加等待时间确保资源完全释放
        
        if (download_success) {
            if (progress_callback_) {
                const char* filename = strrchr(filepath, '/') + 1;
                char message[128];
                snprintf(message, sizeof(message), "文件 %s 下载完成", filename);
                progress_callback_(100, 100, message);
            }
            last_logged_percent = -1;
            ESP_LOGI(TAG, "下载完成后可用内存: %u字节", (unsigned int)esp_get_free_heap_size());
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
            
            ESP_LOGW(TAG, "下载失败，将在%d秒后重试 (第%d次重试)", 8 * retry_count, retry_count);
            // 增加重试延时，每次递增，给网络更多恢复时间
            vTaskDelay(pdMS_TO_TICKS(8000 * retry_count));
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
    
    // **优化的十六进制数据解析 - 大幅提升logo解析速度**
    char* p = data_start;
    int index = 0;
    
    // 预编译十六进制字符查找表，避免重复的isxdigit和strtol调用
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
    
    while (*p && index < array_size) {
        // 查找0X或0x格式的十六进制数
        if ((*p == '0' && (*(p+1) == 'X' || *(p+1) == 'x'))) {
            p += 2;  // 跳过"0X"
            
            // 使用查找表快速解析十六进制字符，避免isxdigit和strtol调用
            int high_nibble = hex_values[(unsigned char)*p];
            int low_nibble = hex_values[(unsigned char)*(p+1)];
            
            if (high_nibble >= 0 && low_nibble >= 0) {
                // 直接计算值，避免strtol调用（性能提升显著）
                unsigned char value = (high_nibble << 4) | low_nibble;
                
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
    
    // **优化的十六进制数据解析 - 大幅提升解析速度**
    char* p = data_start;
    int index = 0;
    
    // 预编译十六进制字符查找表，避免重复的isxdigit和strtol调用
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
    
    while (*p && index < array_size) {
        // 查找0X或0x格式的十六进制数
        if ((*p == '0' && (*(p+1) == 'X' || *(p+1) == 'x'))) {
            p += 2;  // 跳过"0X"
            
            // 使用查找表快速解析十六进制字符，避免isxdigit和strtol调用
            int high_nibble = hex_values[(unsigned char)*p];
            int low_nibble = hex_values[(unsigned char)*(p+1)];
            
            if (high_nibble >= 0 && low_nibble >= 0) {
                // 直接计算值，避免strtol调用（性能提升显著）
                unsigned char value = (high_nibble << 4) | low_nibble;
                
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
    
    ESP_LOGI(TAG, "使用预获取的URL列表下载动画图片，进入专用下载模式...");
    
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
        progress_callback_(0, 100, "准备下载动画图片资源...");
    }
    
    // 清空现有的图片文件
    ESP_LOGI(TAG, "清空现有的动画图片文件...");
    for (int i = 1; i <= MAX_IMAGE_FILES; i++) {
        char filepath[128];
        snprintf(filepath, sizeof(filepath), "%soutput_%04d.h", IMAGE_BASE_PATH, i);
        
        struct stat file_stat;
        if (stat(filepath, &file_stat) == 0) {
            if (remove(filepath) == 0) {
                ESP_LOGI(TAG, "删除现有文件: %s", filepath);
            } else {
                ESP_LOGW(TAG, "删除文件失败: %s", filepath);
            }
        }
    }
    
    bool success = true;
    
    // 准备文件路径
    std::vector<std::string> file_paths;
    for (size_t i = 0; i < urls.size() && i < MAX_IMAGE_FILES; i++) {
        char filepath[128];
        snprintf(filepath, sizeof(filepath), "%soutput_%04d.h", IMAGE_BASE_PATH, (int)(i + 1));
        file_paths.push_back(filepath);
        
        ESP_LOGI(TAG, "准备下载动画图片文件 [%zu/%zu]: %s", i + 1, urls.size(), urls[i].c_str());
    }
    
    // 逐个下载文件
    int failed_count = 0;
    for (size_t i = 0; i < urls.size() && i < MAX_IMAGE_FILES; i++) {
        if (progress_callback_) {
            int overall_percent = static_cast<int>(i * 100 / urls.size());
            char message[128];
            const char* filename = strrchr(file_paths[i].c_str(), '/') + 1;
            snprintf(message, sizeof(message), "下载动画图片: %s (%zu/%zu)", 
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
        vTaskDelay(pdMS_TO_TICKS(3000));  // 增加到3秒，给网络连接恢复时间
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
        
        ESP_LOGI(TAG, "所有动画图片文件下载完成");
        ExitDownloadMode();  // 退出下载模式
        return ESP_OK;
    }
    
    // 通知失败
    if (progress_callback_) {
        progress_callback_(0, 100, "下载动画图片资源失败");
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
    
    // 清空现有的logo文件
    struct stat file_stat;
    if (stat(LOGO_FILE_PATH, &file_stat) == 0) {
        if (remove(LOGO_FILE_PATH) == 0) {
            ESP_LOGI(TAG, "删除现有logo文件: %s", LOGO_FILE_PATH);
        } else {
            ESP_LOGW(TAG, "删除logo文件失败: %s", LOGO_FILE_PATH);
        }
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
    
    ESP_LOGI(TAG, "预加载策略：加载所有剩余图片 (总数: %d)", total_images);
    
    for (int i = 1; i <= total_images; i++) {
        // 检查图片是否已经加载
        if (IsImageLoaded(i)) {
            continue; // 已加载，跳过
        }
        
        // 检查音频状态，如果有音频播放则暂停预加载
        if (!app.IsAudioQueueEmpty() || app.GetDeviceState() != kDeviceStateIdle) {
            ESP_LOGW(TAG, "检测到音频活动，暂停预加载以避免冲突，已加载: %d/%d", loaded_count, total_images);
            break;
        }
        
        // 检查内存状况
        free_heap = esp_get_free_heap_size();
        if (free_heap < 300000) { // 如果内存不足300KB，停止加载
            ESP_LOGW(TAG, "预加载过程中内存不足，停止加载，已加载: %d/%d", loaded_count, total_images);
            break;
        }
        
        ESP_LOGI(TAG, "预加载图片 %d/%d...", i, total_images);
        
        if (LoadImageFile(i)) {
            loaded_count++;
            ESP_LOGI(TAG, "预加载图片 %d 成功", i);
        } else {
            ESP_LOGE(TAG, "预加载图片 %d 失败", i);
        }
        
        // 给系统更多时间进行其他操作，特别是音频处理
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    free_heap = esp_get_free_heap_size();
    ESP_LOGI(TAG, "预加载完成，成功加载: %d/%d 张图片，剩余内存: %u字节", 
             loaded_count, total_images, (unsigned int)free_heap);
    
    return loaded_count > 0 ? ESP_OK : ESP_FAIL;
}