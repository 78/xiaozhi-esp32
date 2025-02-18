#include "sdcard.h"

// 定义日志标签，方便打印与该类相关的日志信息
static const char *TAG = "Sdcard";

/**
 * @brief Sdcard 类的构造函数。
 *
 * 该构造函数接收 SD 卡的各个引脚信息，对成员变量进行初始化，并调用 Init 方法来初始化 SD 卡。
 *
 * @param cmd SD 卡的命令引脚。
 * @param clk SD 卡的时钟引脚。
 * @param d0 SD 卡的数据 0 引脚。
 * @param d1 SD 卡的数据 1 引脚。
 * @param d2 SD 卡的数据 2 引脚。
 * @param d3 SD 卡的数据 3 引脚。
 * @param cdz SD 卡的插入检测引脚。
 */
Sdcard::Sdcard(gpio_num_t cmd, gpio_num_t clk, gpio_num_t d0, gpio_num_t d1, gpio_num_t d2, gpio_num_t d3, gpio_num_t cdz)
    : cmd(cmd), clk(clk), d0(d0), d1(d1), d2(d2), d3(d3), cdz(cdz), card(nullptr)
{
    // 调用 Init 方法初始化 SD 卡
    Init();
}

/**
 * @brief Sdcard 类的析构函数。
 *
 * 该析构函数在对象销毁时调用 Unmount 方法来卸载 SD 卡，确保资源被正确释放。
 */
Sdcard::~Sdcard()
{
    // 如果 card 指针为空，说明 SD 卡未挂载，直接返回
    if (card == nullptr)
        return;
    // 调用 Unmount 方法卸载 SD 卡
    Unmount();
}

/**
 * @brief 初始化 SD 卡并挂载文件系统。
 *
 * 该方法首先调用 isSdCardInserted 方法检查 SD 卡是否插入，若插入则进行后续的初始化和挂载操作。
 * 包括配置 SDMMC 主机、SD 卡插槽和挂载参数，最后挂载文件系统并打印 SD 卡信息。
 */
void Sdcard::Init()
{
    // 检查 SD 卡是否插入，如果未插入则直接返回
    if (!isSdCardInserted())
        return;

    // 初始化 SDMMC 主机配置为默认值
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();

    // 提高 SD 卡频率，可根据实际情况调整
    host.max_freq_khz = SDMMC_FREQ_PROBING;

    // 配置 SD 卡插槽，使用默认配置
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    // 设置数据总线宽度为 4 位
    slot_config.width = 4;
    // 设置命令引脚
    slot_config.cmd = cmd;
    // 设置时钟引脚
    slot_config.clk = clk;
    // 设置数据 0 引脚
    slot_config.d0 = d0;
    // 设置数据 1 引脚
    slot_config.d1 = d1;
    // 设置数据 2 引脚
    slot_config.d2 = d2;
    // 设置数据 3 引脚
    slot_config.d3 = d3;
    // 未使用卡检测引脚
    slot_config.cd = GPIO_NUM_NC;
    // 未使用写保护引脚
    slot_config.wp = GPIO_NUM_NC;

    // 初始化文件系统挂载配置
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,  // 挂载失败时不格式化
        .max_files = 5,                   // 最大打开文件数为 5
        .allocation_unit_size = 16 * 1024 // 分配单元大小为 16KB
    };

    // 挂载 SD 卡文件系统，并检查操作是否成功
    ESP_ERROR_CHECK(esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card));

    // 打印 SD 卡的详细信息
    sdmmc_card_print_info(stdout, card);
}

/**
 * @brief 卸载 SD 卡文件系统。
 *
 * 该方法用于卸载已挂载的 SD 卡文件系统，释放相关资源，并将 card 指针置为 nullptr。
 */
void Sdcard::Unmount()
{
    // 卸载 SD 卡文件系统，并检查操作是否成功
    ESP_ERROR_CHECK(esp_vfs_fat_sdcard_unmount("/sdcard", card));
    // 将 card 指针置为 nullptr，表示 SD 卡已卸载
    card = nullptr;
    // 打印 SD 卡卸载成功的日志信息
    ESP_LOGI(TAG, "SD card unmounted");
}

/**
 * @brief 向 SD 卡写入数据。
 *
 * 该方法将指定的数据以追加模式写入到 SD 卡的指定文件中。
 *
 * @param filename 要写入的文件名。
 * @param data 要写入的数据。
 */
void Sdcard::Write(const char *filename, const char *data)
{
    // 如果 card 指针为空，说明 SD 卡未挂载，直接返回
    if (card == nullptr)
        return;
    // 以追加模式打开文件
    FILE *file = fopen(filename, "a");
    if (file == NULL)
    {
        // 打开文件失败，打印错误日志并返回
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }

    // 获取要写入数据的长度
    size_t data_length = strlen(data);
    // 写入数据，并获取实际写入的字节数
    size_t written = fwrite(data, 1, data_length, file);
    if (written < data_length)
    {
        // 写入数据失败，打印错误日志
        ESP_LOGE(TAG, "Failed to write all data to file");
    }
    else
    {
        // 写入数据成功，打印成功日志
        ESP_LOGI(TAG, "Data written to file successfully");
    }

    // 关闭文件
    fclose(file);
}

/**
 * @brief 从 SD 卡读取数据。
 *
 * 该方法从 SD 卡的指定文件中读取数据到缓冲区。
 *
 * @param filename 要读取的文件名。
 * @param buffer 用于存储读取数据的缓冲区。
 * @param buffer_size 缓冲区的大小。
 */
void Sdcard::Read(const char *filename, char *buffer, size_t buffer_size)
{
    // 如果 card 指针为空，说明 SD 卡未挂载，直接返回
    if (card == nullptr)
        return;
    // 以只读模式打开文件
    FILE *file = fopen(filename, "r");
    if (file == NULL)
    {
        // 打开文件失败，打印错误日志并返回
        ESP_LOGE(TAG, "Failed to open file for reading");
        return;
    }

    // 读取数据，并获取实际读取的字节数
    size_t read_size = fread(buffer, 1, buffer_size - 1, file);
    if (read_size == 0)
    {
        // 读取数据失败，打印错误日志
        ESP_LOGE(TAG, "Failed to read from file");
    }
    else
    {
        // 在读取的数据末尾添加字符串结束符
        buffer[read_size] = '\0';
        // 读取数据成功，打印成功日志
        ESP_LOGI(TAG, "Data read from file successfully");
    }

    // 关闭文件
    fclose(file);
}