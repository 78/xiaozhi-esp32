// ===================== [wj] Start =====================
// @Author  : Wang Jian
// @Date    : 2025-09-26
// @Reason  : move Arduino.h to the first palce to avoid macro redefinition
#include <Arduino.h>
// ===================== [wj] End =====================
#include <esp_log.h>
#include <esp_err.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <driver/gpio.h>
#include <esp_event.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "application.h"
#include "system_info.h"
//===================[wj] Start===================
// @Author  : Wang Jian
// @Date    : 2025-11-3
// @Reason  : add includes for SPIFFS and SQLite3
#include <stdio.h>
#include "esp_log.h"
#include "esp_err.h"

extern "C" {
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/spi_common.h"
#include "driver/sdspi_host.h"
}

#include "sqlite3.h"
//===================[wj] End======================

// ===================== [wj] Start =====================
// @Author  : Wang Jian
// @Date    : 2025-09-26
// @Reason  : add includes for EPD and GT30

#include "ui/epd_renderer.h"
#include "cat.h"
//==================== [wj] End =====================

//===================== [wj] Start ===================== 
// @Author  : Wang Jian
// @Date    : 2025-11-3
// @Reason  : add includes for SPIFFS and SQLite3
// 下面这组在很多 ESP32-S3 DevKit 上可用（仅示例）：
#define SD_SPI_HOST        SPI2_HOST     // S3 上一般用 SPI2_HOST
#define PIN_NUM_MOSI       GPIO_NUM_11
#define PIN_NUM_MISO       GPIO_NUM_13
#define PIN_NUM_SCLK       GPIO_NUM_12
#define PIN_NUM_CS         GPIO_NUM_10
static const char* TAG = "SD_SPI_SQLITE";
static sdmmc_card_t* s_card = nullptr;

// 挂载 SD 卡到 /sdcard（SPI 模式）
static esp_err_t mount_sdcard_spi() {
    // 1) 初始化 SPI 总线
    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = PIN_NUM_MOSI;
    bus_cfg.miso_io_num = PIN_NUM_MISO;
    bus_cfg.sclk_io_num = PIN_NUM_SCLK;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    ESP_ERROR_CHECK(spi_bus_initialize(SD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    // 2) 配置 SDSPI 主机与设备（片选）
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SD_SPI_HOST;

    sdspi_device_config_t slot_cfg = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_cfg.gpio_cs = PIN_NUM_CS;
    slot_cfg.host_id = SD_SPI_HOST;

    // 3) 挂载 FAT 文件系统
    esp_vfs_fat_sdmmc_mount_config_t mcfg = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };

    ESP_LOGI(TAG, "Mounting SD card (SPI) at /sdcard ...");
    esp_err_t ret = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot_cfg, &mcfg, &s_card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Mount failed: %s", esp_err_to_name(ret));
        spi_bus_free(SD_SPI_HOST);
        return ret;
    }

    sdmmc_card_print_info(stdout, s_card);
    return ESP_OK;
}

// 卸载 SD 卡并释放 SPI 总线
static void unmount_sdcard_spi() {
    if (s_card) {
        esp_vfs_fat_sdcard_unmount("/sdcard", s_card);
        s_card = nullptr;
        ESP_LOGI(TAG, "SD card unmounted");
    }
    spi_bus_free(SD_SPI_HOST);
}

// 查询回调：把每行打印出来
static int select_cb(void*, int ncol, char** vals, char** names) {
    for (int i = 0; i < ncol; ++i) {
        printf("%s=%s ", names[i], vals[i] ? vals[i] : "NULL");
    }
    printf("\n");
    return 0;
}

//===================== [wj] End =====================
#define TAG "main"

extern "C" void app_main(void)
{
//===================== [wj] Start =====================
// @Author  : Wang Jian
// @Date    : 2025-09-26
// @Reason  : add EPD and GT30 initialization code
        // Initialize EPD and GT30 via EpdRenderer helper. If EPD not available this is a no-op.
        EpdRenderer::Init();
        if (EpdRenderer::Available()) {
            EpdRenderer::FillAndDraw("北国风光，千里冰封,万里雪飘。", 0, 20);
            EpdRenderer::Draw("望长城内外，惟余莽莽;大河上下,顿失滔滔。", 0, 40);
            EpdRenderer::Draw("山舞银蛇，原驰蜡象,欲与天公试比高。", 0, 60);
            EpdRenderer::Draw("my name is mao ze dong！", 0, 80);
            EpdRenderer::Draw("This is my 沁园春雪？", 0, 100);
            // TODO: drawBitmap of embedded image via EpdRenderer when needed
            EpdRenderer::Display(true);
        }

//===================== [wj] End =====================

    
    // Initialize the default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize NVS flash for WiFi configuration
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "Erasing NVS flash to fix corruption");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Launch the application
    auto& app = Application::GetInstance();
    app.Start();


//===================== [wj] Start =====================
// @Author  : Wang Jian
// @Date    : 2025-11-3
// @Reason  : initialize SQLite3 with SPIFFS
    // 1) 挂载 SD 卡
    if (mount_sdcard_spi() != ESP_OK) return;

    // 2) 打开/创建数据库（保存在 /sdcard）
    sqlite3* db = nullptr;
    int rc = sqlite3_open("/sdcard/my.db", &db);
    if (rc != SQLITE_OK) {
        ESP_LOGE(TAG, "sqlite3_open failed: %s", sqlite3_errmsg(db));
        if (db) sqlite3_close(db);
        unmount_sdcard_spi();
        return;
    }

    // 3) 推荐的 PRAGMA（对 SD/闪存更友好）
    sqlite3_exec(db, "PRAGMA page_size=4096;",            nullptr, nullptr, nullptr);
    sqlite3_exec(db, "PRAGMA journal_mode=PERSIST;",      nullptr, nullptr, nullptr);
    sqlite3_exec(db, "PRAGMA synchronous=NORMAL;",        nullptr, nullptr, nullptr);
    sqlite3_exec(db, "PRAGMA temp_store=MEMORY;",         nullptr, nullptr, nullptr);

    // 4) 建表 + 事务写入 + 查询
    char* errmsg = nullptr;

    rc = sqlite3_exec(db,
        "CREATE TABLE IF NOT EXISTS t ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " v  TEXT"
        ");",
        nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) { ESP_LOGE(TAG, "CREATE error: %s", errmsg); sqlite3_free(errmsg); }

    rc = sqlite3_exec(db, "BEGIN;", nullptr, nullptr, &errmsg);
    if (rc == SQLITE_OK) {
        sqlite3_exec(db, "INSERT INTO t(v) VALUES('hello');",  nullptr, nullptr, nullptr);
        sqlite3_exec(db, "INSERT INTO t(v) VALUES('from spi');", nullptr, nullptr, nullptr);
        rc = sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &errmsg);
    }
    if (rc != SQLITE_OK) {
        ESP_LOGE(TAG, "Tx error: %s", errmsg);
        sqlite3_free(errmsg);
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
    }

    rc = sqlite3_exec(db, "SELECT id, v FROM t;", select_cb, nullptr, &errmsg);
    if (rc != SQLITE_OK) { ESP_LOGE(TAG, "SELECT error: %s", errmsg); sqlite3_free(errmsg); }

    // 5) 关闭数据库 & 卸载 SD 卡
    sqlite3_close(db);
    unmount_sdcard_spi();

    ESP_LOGI(TAG, "All done.");
    //===================== [wj] End =====================
}
