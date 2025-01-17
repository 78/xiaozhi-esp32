#ifndef SDCARD_H
#define SDCARD_H

#include "string.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include "esp_log.h"

class Sdcard
{
private:
    gpio_num_t cmd;
    gpio_num_t clk;
    gpio_num_t d0;
    gpio_num_t d1;
    gpio_num_t d2;
    gpio_num_t d3;
    gpio_num_t cdz;
    sdmmc_card_t *card;

    bool isSdCardInserted() {
        // 配置 CDZ 引脚为输入模式
        gpio_config_t io_conf;
        io_conf.intr_type = GPIO_INTR_DISABLE;  // 禁用中断
        io_conf.mode = GPIO_MODE_INPUT;  // 输入模式
        io_conf.pin_bit_mask = (1ULL << cdz);  // 配置的引脚
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;  // 不启用下拉电阻
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;  // 启用上拉电阻，确保引脚状态稳定
        gpio_config(&io_conf);

        // 读取 CDZ 引脚的电平
        int level = gpio_get_level(cdz);
        // 假设低电平表示 SD 卡已插入，高电平表示未插入
        if (level == 0) {
            ESP_LOGI("Sdcard", "SD card is inserted.");
            return true;
        } else {
            ESP_LOGI("Sdcard", "SD card is not inserted.");
            return false;
        }
    }

public:
    Sdcard(gpio_num_t cmd, gpio_num_t clk, gpio_num_t d0, gpio_num_t d1, gpio_num_t d2, gpio_num_t d3, gpio_num_t cdz);
    ~Sdcard();

    void Init();
    void Unmount();
    void Write(const char *filename, const char *data);
    void Read(const char *filename, char *buffer, size_t buffer_size);
};

#endif // SDCARD_H