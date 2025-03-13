#include <esp_log.h>       // ESP32 日志库
#include <esp_err.h>       // ESP32 错误处理库
#include <nvs.h>           // 非易失性存储 (NVS) 库
#include <nvs_flash.h>     // NVS Flash 初始化库
#include <driver/gpio.h>   // GPIO 驱动库
#include <esp_event.h>     // ESP32 事件循环库

#include "application.h"   // 应用程序头文件
#include "system_info.h"   // 系统信息头文件

#define TAG "main"         // 日志标签

// 主函数，ESP32 程序的入口点
extern "C" void app_main(void)
{
    // 初始化默认事件循环
    // 事件循环用于处理系统事件（如 WiFi 连接、断开等）
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 初始化 NVS Flash，用于存储 WiFi 配置和其他持久化数据
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // 如果 NVS Flash 分区没有空闲页或发现新版本，擦除并重新初始化
        ESP_LOGW(TAG, "Erasing NVS flash to fix corruption");
        ESP_ERROR_CHECK(nvs_flash_erase());  // 擦除 NVS Flash
        ret = nvs_flash_init();             // 重新初始化 NVS Flash
    }
    ESP_ERROR_CHECK(ret);  // 检查 NVS Flash 初始化是否成功

    // 启动应用程序
    Application::GetInstance().Start();  // 获取应用程序单例并启动

    // 主线程将退出并释放栈内存
    // 在 ESP32 中，app_main 函数退出后，主线程会结束，但 FreeRTOS 任务会继续运行
}