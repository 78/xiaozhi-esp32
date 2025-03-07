/**
 * @file Sdcard.h
 * @brief 该头文件定义了 Sdcard 类，用于管理 SD 卡的操作，
 * 包括初始化、挂载、卸载、写入和读取文件等功能，同时支持检测 SD 卡是否插入。
 *
 * @author 施华锋
 * @date 2025-2-18
 */

#ifndef SDCARD_H
#define SDCARD_H

#include "string.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include "esp_log.h"

/**
 * @class Sdcard
 * @brief 该类封装了 SD 卡的操作，提供了初始化、卸载、写入和读取文件等功能，
 * 还可以检测 SD 卡是否插入。
 */
class Sdcard
{
private:
    gpio_num_t _cmd;
    gpio_num_t _cs;
    gpio_num_t _mosi;
    gpio_num_t _clk;
    gpio_num_t _miso;
    gpio_num_t _d0;
    gpio_num_t _d1;
    gpio_num_t _d2;
    gpio_num_t _d3;
    gpio_num_t _cdz;
    spi_host_device_t _spi_num;
    // SD 卡设备结构体指针，用于表示已挂载的 SD 卡
    sdmmc_card_t *_card;
    /**
     * @brief 检测 SD 卡是否插入。
     *
     * 该方法通过配置 CDZ 引脚为输入模式，并启用上拉电阻，
     * 读取该引脚的电平来判断 SD 卡是否插入。
     *
     * @return 如果 SD 卡已插入返回 true，否则返回 false。
     */
    bool isSdCardInserted()
    {
        if (_cdz == GPIO_NUM_NC)
            return true;
        // 定义 GPIO 配置结构体
        gpio_config_t io_conf;
        // 禁用 GPIO 中断
        io_conf.intr_type = GPIO_INTR_DISABLE;
        // 将 GPIO 配置为输入模式
        io_conf.mode = GPIO_MODE_INPUT;
        // 设置要配置的 GPIO 引脚，使用位掩码
        io_conf.pin_bit_mask = (1ULL << _cdz);
        // 禁用下拉电阻
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        // 启用上拉电阻，确保引脚状态稳定
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        // 应用 GPIO 配置
        gpio_config(&io_conf);

        // 读取 CDZ 引脚的电平
        int level = gpio_get_level(_cdz);
        // 假设低电平表示 SD 卡已插入，高电平表示未插入
        if (level == 0)
        {
            ESP_LOGI("Sdcard", "SD card is inserted.");
            return true;
        }
        else
        {
            ESP_LOGI("Sdcard", "SD card is not inserted.");
            return false;
        }
    }

public:
    /**
     * @brief 构造函数，初始化 SD 卡对象。
     *
     * @param cmd SD 卡的命令引脚。
     * @param clk SD 卡的时钟引脚。
     * @param d0 SD 卡的数据 0 引脚。
     * @param d1 SD 卡的数据 1 引脚。
     * @param d2 SD 卡的数据 2 引脚。
     * @param d3 SD 卡的数据 3 引脚。
     * @param cdz SD 卡的插入检测引脚。
     */
    Sdcard(gpio_num_t cmd, gpio_num_t clk, gpio_num_t d0, gpio_num_t d1, gpio_num_t d2, gpio_num_t d3, gpio_num_t cdz);

    Sdcard(gpio_num_t cs, gpio_num_t mosi, gpio_num_t clk, gpio_num_t miso, spi_host_device_t spi_num);

    /**
     * @brief 析构函数，负责释放资源。
     */
    ~Sdcard();

    /**
     * @brief 卸载 SD 卡文件系统。
     *
     * 该方法用于卸载已挂载的 SD 卡文件系统，释放相关资源。
     */
    void Unmount();

    /**
     * @brief 向 SD 卡写入数据。
     *
     * 该方法将指定的数据写入到 SD 卡的指定文件中。
     *
     * @param filename 要写入的文件名。
     * @param data 要写入的数据。
     */
    void Write(const char *filename, const char *data);

    /**
     * @brief 从 SD 卡读取数据。
     *
     * 该方法从 SD 卡的指定文件中读取数据到缓冲区。
     *
     * @param filename 要读取的文件名。
     * @param buffer 用于存储读取数据的缓冲区。
     * @param buffer_size 缓冲区的大小。
     */
    void Read(const char *filename, char *buffer, size_t buffer_size);
};

#endif // SDCARD_H