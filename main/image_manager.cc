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
#define IMAGE_BASE_PATH "/resources/images/"
#define MAX_IMAGE_FILES 2
#define MAX_DOWNLOAD_RETRIES 3

ImageResourceManager::ImageResourceManager() {
    mounted_ = false;
    initialized_ = false;
    has_valid_images_ = false;
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
    ESP_LOGI(TAG, "当前本地图片版本: %s", local_version_.c_str());
    
    // 检查是否有有效图片
    has_valid_images_ = CheckImagesExist();
    if (has_valid_images_) {
        ESP_LOGI(TAG, "找到有效的图片文件");
        LoadImageData();
    } else {
        ESP_LOGI(TAG, "未找到有效的图片文件，将在网络连接后下载");
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
        ESP_LOGW(TAG, "无法打开版本文件，假定初始版本");
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
        ESP_LOGE(TAG, "解析版本文件失败");
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
    
    for (int i = 1; i <= MAX_IMAGE_FILES; i++) {
        char filename[64];
        snprintf(filename, sizeof(filename), "%soutput_%04d.h", IMAGE_BASE_PATH, i);
        
        FILE* f = fopen(filename, "r");
        if (f == NULL) {
            ESP_LOGW(TAG, "未找到图片文件: %s", filename);
            return false;
        }
        fclose(f);
    }
    
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
        ESP_LOGW(TAG, "未连接WiFi，无法检查服务器版本");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "检查服务器图片版本...");
    
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
    
    ESP_LOGI(TAG, "服务器图片版本: %s, 本地版本: %s", 
             server_version_.c_str(), local_version_.c_str());
    
    return server_version_ != local_version_ ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t ImageResourceManager::DownloadImages(const char* api_url) {
    // 确保已连接WiFi
    if (!WifiStation::GetInstance().IsConnected()) {
        ESP_LOGW(TAG, "未连接WiFi，无法下载图片");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "开始下载图片文件...");
    
    bool success = true;
    
    // 下载所有图片文件
    for (int i = 1; i <= MAX_IMAGE_FILES; i++) {
        char filename[64];
        snprintf(filename, sizeof(filename), "output_%04d.h", i);
        
        char filepath[128];
        snprintf(filepath, sizeof(filepath), "%s%s", IMAGE_BASE_PATH, filename);
        
        std::string url = std::string(api_url) + "/" + std::string(filename);
        
        ESP_LOGI(TAG, "下载图片文件 [%d/%d]: %s", i, MAX_IMAGE_FILES, filename);
        
        if (DownloadFile(url.c_str(), filepath) != ESP_OK) {
            ESP_LOGE(TAG, "下载图片文件失败: %s", filename);
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
            ESP_LOGE(TAG, "保存版本信息失败");
            return ESP_FAIL;
        }
        
        local_version_ = server_version_;
        has_valid_images_ = true;
        
        // 加载新下载的图片数据
        LoadImageData();
        
        ESP_LOGI(TAG, "所有图片文件下载完成，更新版本为: %s", local_version_.c_str());
        return ESP_OK;
    }
    
    return ESP_FAIL;
}

esp_err_t ImageResourceManager::DownloadFile(const char* url, const char* filepath) {
    int retry_count = 0;
    
    while (retry_count < MAX_DOWNLOAD_RETRIES) {
        auto http = Board::GetInstance().CreateHttp();
        
        if (!http->Open("GET", url)) {
            ESP_LOGE(TAG, "无法连接到服务器");
            delete http;
            retry_count++;
            vTaskDelay(pdMS_TO_TICKS(1000 * retry_count)); // 延迟增加重试时间
            continue;
        }
        
        FILE* f = fopen(filepath, "w");
        if (f == NULL) {
            ESP_LOGE(TAG, "无法创建文件: %s (errno: %d, strerror: %s)", filepath, errno, strerror(errno));
            delete http;
            return ESP_ERR_NO_MEM;
        }
        
        size_t content_length = http->GetBodyLength();
        if (content_length == 0) {
            ESP_LOGE(TAG, "无法获取文件大小");
            fclose(f);
            delete http;
            return ESP_FAIL;
        }
        
        ESP_LOGI(TAG, "下载文件大小: %d字节", content_length);
        
        char buffer[512];
        size_t total_read = 0;
        
        while (true) {
            int ret = http->Read(buffer, sizeof(buffer));
            if (ret < 0) {
                ESP_LOGE(TAG, "读取HTTP数据失败");
                fclose(f);
                delete http;
                retry_count++;
                vTaskDelay(pdMS_TO_TICKS(1000 * retry_count));
                break;
            }
            
            if (ret == 0) { // 下载完成
                fclose(f);
                delete http;
                return ESP_OK;
            }
            
            size_t written = fwrite(buffer, 1, ret, f);
            if (written != ret) {
                ESP_LOGE(TAG, "写入文件失败");
                fclose(f);
                delete http;
                return ESP_FAIL;
            }
            
            total_read += ret;
            
            // 显示下载进度
            if (content_length > 0) {
                ESP_LOGI(TAG, "下载进度: %.1f%%", (float)total_read * 100 / content_length);
            }
        }
    }
    
    ESP_LOGE(TAG, "下载重试次数已达上限");
    return ESP_FAIL;
}

bool ImageResourceManager::SaveVersion(const std::string& version) {
    if (!mounted_) {
        return false;
    }
    
    FILE* f = fopen(IMAGE_VERSION_FILE, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "无法创建版本文件");
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
    if (!has_valid_images_ || !mounted_) {
        ESP_LOGE(TAG, "无有效图片数据可加载");
        return;
    }
    
    // 清空原有数据
    for (auto ptr : image_data_pointers_) {
        if (ptr) {
            free(ptr);
        }
    }
    image_data_pointers_.clear();
    image_array_.clear();
    
    // 预分配空间
    image_array_.resize(MAX_IMAGE_FILES);
    image_data_pointers_.resize(MAX_IMAGE_FILES, nullptr);
    
    // 加载每个图片文件
    for (int i = 1; i <= MAX_IMAGE_FILES; i++) {
        if (!LoadImageFile(i)) {
            ESP_LOGE(TAG, "加载图片文件失败，索引: %d", i);
        }
    }
    
    ESP_LOGI(TAG, "共加载 %d 个图片文件", image_array_.size());
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
        
        ESP_LOGI(TAG, "成功加载图片 %d: 大小 %d 字节", image_index, index);
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
        ESP_LOGI(TAG, "未找到有效图片，需要下载");
        return DownloadImages(api_url);
    }
    
    // 检查服务器版本
    esp_err_t status = CheckServerVersion(version_url);
    if (status == ESP_OK) {
        ESP_LOGI(TAG, "发现新版本，需要更新图片资源");
        return DownloadImages(api_url);
    } else if (status == ESP_ERR_NOT_FOUND) {
        ESP_LOGI(TAG, "图片资源已是最新版本");
        return ESP_OK;
    }
    
    return status;
}

const std::vector<const uint8_t*>& ImageResourceManager::GetImageArray() const {
    return image_array_;
}