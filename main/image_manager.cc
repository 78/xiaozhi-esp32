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

#define TAG "ImageResManager"
#define IMAGE_URL_CACHE_FILE "/resources/image_urls.json"  // 修改：图片URL缓存文件
#define LOGO_URL_CACHE_FILE "/resources/logo_url.json"     // 修改：logo URL缓存文件
#define IMAGE_BASE_PATH "/resources/images/"
#define LOGO_FILE_PATH "/resources/images/logo.h"
#define MAX_IMAGE_FILES 10  // 修改：根据示例有10个动态图片
#define MAX_DOWNLOAD_RETRIES 3

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
    if (!http->Open("GET", version_url)) {
        ESP_LOGE(TAG, "无法连接到服务器");
        delete http;
        return ESP_FAIL;
    }
    
    // 添加必要的请求头
    std::string device_id = SystemInfo::GetMacAddress();
    std::string client_id = SystemInfo::GetClientId();
    
    if (!device_id.empty()) {
        http->SetHeader("Device-Id", device_id.c_str());
    }
    if (!client_id.empty()) {
        http->SetHeader("Client-Id", client_id.c_str());
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
    if (!http->Open("GET", logo_version_url)) {
        ESP_LOGE(TAG, "无法连接到logo服务器");
        delete http;
        return ESP_FAIL;
    }
    
    // 添加必要的请求头
    std::string device_id = SystemInfo::GetMacAddress();
    std::string client_id = SystemInfo::GetClientId();
    
    if (!device_id.empty()) {
        http->SetHeader("Device-Id", device_id.c_str());
    }
    if (!client_id.empty()) {
        http->SetHeader("Client-Id", client_id.c_str());
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
    
    ESP_LOGI(TAG, "开始下载动画图片文件...");
    
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
    for (size_t i = 0; i < server_dynamic_urls_.size() && i < MAX_IMAGE_FILES; i++) {
        if (progress_callback_) {
            int overall_percent = static_cast<int>(i * 100 / server_dynamic_urls_.size());
            char message[128];
            const char* filename = strrchr(file_paths[i].c_str(), '/') + 1;
            snprintf(message, sizeof(message), "准备下载动画图片: %s (%zu/%zu)", 
                    filename, i + 1, server_dynamic_urls_.size());
            progress_callback_(overall_percent, 100, message);
        }
        
        if (DownloadFile(server_dynamic_urls_[i].c_str(), file_paths[i].c_str()) != ESP_OK) {
            ESP_LOGE(TAG, "下载动画图片文件失败: %s", file_paths[i].c_str());
            success = false;
            break;
        }
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
    
    if (DownloadFile(server_static_url_.c_str(), LOGO_FILE_PATH) != ESP_OK) {
        ESP_LOGE(TAG, "下载logo文件失败");
        
        if (progress_callback_) {
            progress_callback_(0, 100, "下载logo失败");
        }
        
        return ESP_FAIL;
    }
    
    // 保存新的URL缓存
    if (!SaveStaticUrl(server_static_url_)) {
        ESP_LOGE(TAG, "保存logo URL缓存失败");
        
        // 通知错误
        if (progress_callback_) {
            progress_callback_(100, 100, "保存logo URL缓存失败");
        }
        
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
        
        // 添加必要的请求头
        std::string device_id = SystemInfo::GetMacAddress();
        std::string client_id = SystemInfo::GetClientId();
        
        if (!device_id.empty()) {
            http->SetHeader("Device-Id", device_id.c_str());
        }
        if (!client_id.empty()) {
            http->SetHeader("Client-Id", client_id.c_str());
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
        // 根据缓存的URL数量确定要加载的图片数量
        int actual_image_count = std::min((int)cached_dynamic_urls_.size(), MAX_IMAGE_FILES);
        
        // 预分配空间
        image_array_.resize(actual_image_count);
        image_data_pointers_.resize(actual_image_count, nullptr);
        
        // 加载每个图片文件
        for (int i = 1; i <= actual_image_count; i++) {
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
        
        ESP_LOGI(TAG, "共加载 %d 个动画图片文件", (int)image_array_.size());
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