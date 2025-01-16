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
    sdmmc_card_t *card;

public:
    Sdcard(gpio_num_t cmd, gpio_num_t clk, gpio_num_t d0, gpio_num_t d1, gpio_num_t d2, gpio_num_t d3);
    ~Sdcard();

    void Init();
    void Unmount();
    void Write(const char *filename, const char *data);
    void Read(const char *filename, char *buffer, size_t buffer_size);
};

#endif // SDCARD_H