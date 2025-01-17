#include "sdcard.h"

static const char *TAG = "Sdcard";

Sdcard::Sdcard(gpio_num_t cmd, gpio_num_t clk, gpio_num_t d0, gpio_num_t d1, gpio_num_t d2, gpio_num_t d3, gpio_num_t cdz) : cmd(cmd), clk(clk), d0(d0), d1(d1), d2(d2), d3(d3), cdz(cdz), card(nullptr)
{
    Init();
}

Sdcard::~Sdcard()
{
    if (card == nullptr)
        return;
    Unmount();
}

void Sdcard::Init()
{
    if (!isSdCardInserted())
        return;
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();

    // 提高 SD 卡频率，可根据实际情况调整
    host.max_freq_khz = SDMMC_FREQ_PROBING;

    // 配置 SD 卡引脚
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 4;
    slot_config.cmd = cmd;
    slot_config.clk = clk;
    slot_config.d0 = d0;
    slot_config.d1 = d1;
    slot_config.d2 = d2;
    slot_config.d3 = d3;
    slot_config.cd = GPIO_NUM_NC;
    slot_config.wp = GPIO_NUM_NC;
    // 初始化 SD 卡并挂载文件系统
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024};

    ESP_ERROR_CHECK(esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card));

    // 打印 SD 卡信息
    sdmmc_card_print_info(stdout, card);
}

void Sdcard::Unmount()
{
    ESP_ERROR_CHECK(esp_vfs_fat_sdcard_unmount("/sdcard", card));
    card = nullptr;
    ESP_LOGI(TAG, "SD card unmounted");
}

void Sdcard::Write(const char *filename, const char *data)
{
    if (card == nullptr)
        return;
    FILE *file = fopen(filename, "a"); // 以追加模式打开文件
    if (file == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }

    size_t data_length = strlen(data);
    size_t written = fwrite(data, 1, data_length, file);
    if (written < data_length)
    {
        ESP_LOGE(TAG, "Failed to write all data to file");
    }
    else
    {
        ESP_LOGI(TAG, "Data written to file successfully");
    }

    fclose(file);
}

void Sdcard::Read(const char *filename, char *buffer, size_t buffer_size)
{
    if (card == nullptr)
        return;
    FILE *file = fopen(filename, "r");
    if (file == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return;
    }

    size_t read_size = fread(buffer, 1, buffer_size - 1, file);
    if (read_size == 0)
    {
        ESP_LOGE(TAG, "Failed to read from file");
    }
    else
    {
        buffer[read_size] = '\0';
        ESP_LOGI(TAG, "Data read from file successfully");
    }

    fclose(file);
}
