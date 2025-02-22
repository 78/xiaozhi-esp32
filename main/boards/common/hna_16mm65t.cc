#include "hna_16mm65t.h"
#include "driver/usb_serial_jtag.h"
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <string.h>
#include <esp_log.h>
#include "application.h"
#include "settings.h"

// 定义日志标签
#define TAG "HNA_16MM65T"

void HNA_16MM65T::waveanimate()
{
    int left_sum = 0, right_sum = 0;
    int64_t current_time = esp_timer_get_time() / 1000;

    int64_t elapsed_time = current_time - wave_start_time;

    if (elapsed_time >= 220) // 2 cycle plus 10%
    {
        wave_start_time = current_time;

        for (size_t i = 0; i < FFT_SIZE; i++)
        {
            waveData[i].last_value = waveData[i].target_value;
            waveData[i].target_value = 0;
            waveData[i].animation_step = 0;
        }
    }
    for (int i = 0; i < FFT_SIZE; i++)
    {
        if (waveData[i].animation_step < wave_total_steps)
        {
            float progress = static_cast<float>(waveData[i].animation_step) / wave_total_steps;
            float factor = 1 - std::exp(-3 * progress); // 指数衰减因子
            waveData[i].current_value = waveData[i].last_value + static_cast<int>((waveData[i].target_value - waveData[i].last_value) * factor);
            wavehelper(i, waveData[i].current_value * 8 / 90);
            waveData[i].animation_step++;
        }
        else
        {
            waveData[i].last_value = waveData[i].target_value;
            wavehelper(i, waveData[i].target_value * 8 / 90);
        }
        if (i < 6)
            left_sum += waveData[i].current_value;
        else
            right_sum += waveData[i].current_value;
    }

    corewavehelper(left_sum * 8 / 90 / 4, right_sum * 8 / 90 / 4);
}

uint32_t HNA_16MM65T::contentgetpart(uint32_t raw, uint32_t before_raw, uint32_t mask)
{
    return (raw & mask) | (before_raw & (~mask));
}

void HNA_16MM65T::contentanimate()
{
    static int64_t start_time = esp_timer_get_time() / 1000;
    int64_t current_time = esp_timer_get_time() / 1000;

    int64_t elapsed_time = current_time - start_time;

    if (elapsed_time >= 30)
        start_time = current_time;
    else
        return;

    if (content_inhibit_time != 0)
    {
        elapsed_time = current_time - content_inhibit_time;

        if (elapsed_time > 0)
        {
            for (size_t i = 0; i < NUM_SIZE; i++)
            {
                currentData[i].last_content = currentData[i].current_content;
                currentData[i].animation_type = tempData[i].animation_type;
                currentData[i].current_content = tempData[i].current_content;
            }

            content_inhibit_time = 0;
        }
    }

    for (int i = 0; i < NUM_SIZE; i++)
    {
        if (currentData[i].current_content != currentData[i].last_content)
        {
            uint32_t before_raw_code = find_hex_code(currentData[i].last_content);
            uint32_t raw_code = find_hex_code(currentData[i].current_content);
            uint32_t code = raw_code;
            if (currentData[i].animation_type == ANI_CLOCKWISE)
            {
                switch (currentData[i].animation_step)
                {
                case 0:
                    code = contentgetpart(raw_code, before_raw_code, 0x080000 | 0x800000);
                    if (code != 0)
                        break;
                case 1:
                    code = contentgetpart(raw_code, before_raw_code, 0x4C0000 | 0x800000);
                    if (code != 0)
                        break;
                case 2:
                    code = contentgetpart(raw_code, before_raw_code, 0x6e0000 | 0x800000);
                    if (code != 0)
                        break;
                case 3:
                    code = contentgetpart(raw_code, before_raw_code, 0x6f6000 | 0x800000);
                    if (code != 0)
                        break;
                case 4:
                    code = contentgetpart(raw_code, before_raw_code, 0x6f6300 | 0x800000);
                    if (code != 0)
                        break;
                case 5:
                    code = contentgetpart(raw_code, before_raw_code, 0x6f6770 | 0x800000);
                    if (code != 0)
                        break;
                case 6:
                    code = contentgetpart(raw_code, before_raw_code, 0x6f6ff0 | 0x800000);
                    if (code != 0)
                        break;
                case 7:
                    code = contentgetpart(raw_code, before_raw_code, 0x6ffff0 | 0x800000);
                    if (code != 0)
                        break;
                default:
                    currentData[i].animation_step = -1;
                    break;
                }
            }
            else if (currentData[i].animation_type == ANI_ANTICLOCKWISE)
            {
                switch (currentData[i].animation_step)
                {
                case 0:
                    code = contentgetpart(raw_code, before_raw_code, 0x004880);
                    if (code != 0)
                        break;
                case 1:
                    code = contentgetpart(raw_code, before_raw_code, 0x004ca0);
                    if (code != 0)
                        break;
                case 2:
                    code = contentgetpart(raw_code, before_raw_code, 0x004ef0);
                    if (code != 0)
                        break;
                case 3:
                    code = contentgetpart(raw_code, before_raw_code, 0x006ff0);
                    if (code != 0)
                        break;
                case 4:
                    code = contentgetpart(raw_code, before_raw_code, 0x036ff0);
                    if (code != 0)
                        break;
                case 5:
                    code = contentgetpart(raw_code, before_raw_code, 0x676ff0);
                    if (code != 0)
                        break;
                case 6:
                    code = contentgetpart(raw_code, before_raw_code, 0xef6ff0);
                    if (code != 0)
                        break;
                case 7:
                    code = contentgetpart(raw_code, before_raw_code, 0xffeff0);
                    if (code != 0)
                        break;
                default:
                    currentData[i].animation_step = -1;
                    break;
                }
            }
            else if (currentData[i].animation_type == ANI_UP2DOWN)
            {
                switch (currentData[i].animation_step)
                {
                case 0:
                    code = contentgetpart(raw_code, before_raw_code, 0xe00000);
                    if (code != 0)
                        break;
                case 1:
                    code = contentgetpart(raw_code, before_raw_code, 0xff0000);
                    if (code != 0)
                        break;
                case 2:
                    code = contentgetpart(raw_code, before_raw_code, 0xffe000);
                    if (code != 0)
                        break;
                case 3:
                    code = contentgetpart(raw_code, before_raw_code, 0xffff00);
                    if (code != 0)
                        break;
                default:
                    currentData[i].animation_step = -1;
                    break;
                }
            }
            else if (currentData[i].animation_type == ANI_DOWN2UP)
            {
                switch (currentData[i].animation_step)
                {
                case 0:
                    code = contentgetpart(raw_code, before_raw_code, 0x0000f0);
                    if (code != 0)
                        break;
                case 1:
                    code = contentgetpart(raw_code, before_raw_code, 0x001ff0);
                    if (code != 0)
                        break;
                case 2:
                    code = contentgetpart(raw_code, before_raw_code, 0x00fff0);
                    if (code != 0)
                        break;
                case 3:
                    code = contentgetpart(raw_code, before_raw_code, 0x1ffff0);
                    if (code != 0)
                        break;
                default:
                    currentData[i].animation_step = -1;
                    break;
                }
            }
            else if (currentData[i].animation_type == ANI_LEFT2RT)
            {
                switch (currentData[i].animation_step)
                {
                case 0:
                    code = contentgetpart(raw_code, before_raw_code, 0x901080);
                    if (code != 0)
                        break;
                case 1:
                    code = contentgetpart(raw_code, before_raw_code, 0xd89880);
                    if (code != 0)
                        break;
                case 2:
                    code = contentgetpart(raw_code, before_raw_code, 0xdcdce0);
                    if (code != 0)
                        break;
                case 3:
                    code = contentgetpart(raw_code, before_raw_code, 0xdefee0);
                    if (code != 0)
                        break;
                default:
                    currentData[i].animation_step = -1;
                    break;
                }
            }
            else if (currentData[i].animation_type == ANI_RT2LEFT)
            {
                switch (currentData[i].animation_step)
                {
                case 0:
                    code = contentgetpart(raw_code, before_raw_code, 0x210110);
                    if (code != 0)
                        break;
                case 1:
                    code = contentgetpart(raw_code, before_raw_code, 0x632310);
                    if (code != 0)
                        break;
                case 2:
                    code = contentgetpart(raw_code, before_raw_code, 0x676770);
                    if (code != 0)
                        break;
                case 3:
                    code = contentgetpart(raw_code, before_raw_code, 0x6fef70);
                    if (code != 0)
                        break;
                default:
                    currentData[i].animation_step = -1;
                    break;
                }
            }
            else
                currentData[i].animation_step = -1;

            if (currentData[i].animation_step == -1)
                currentData[i].last_content = currentData[i].current_content;

            charhelper(i, code);
            currentData[i].animation_step++;
        }
    }
}

/**
 * @brief HNA_16MM65T 类的构造函数。
 *
 * 初始化 PT6324 设备，并创建一个任务用于刷新显示和执行动画。
 *
 * @param spi_device SPI 设备句柄，用于与 PT6324 通信。
 */
HNA_16MM65T::HNA_16MM65T(spi_device_handle_t spi_device) : PT6324Writer(spi_device)
{
    // 初始化 PT6324 设备
    pt6324_init();
    InitializeBacklight();
    // 创建一个任务用于刷新显示和执行动画
    xTaskCreate(
        [](void *arg)
        {
            // 将参数转换为 HNA_16MM65T 指针
            HNA_16MM65T *vfd = static_cast<HNA_16MM65T *>(arg);
            vfd->symbolhelper(LBAR_RBAR, true);
            while (true)
            {
                // 刷新显示
                vfd->pt6324_refrash(vfd->gram);
                // 执行动画
                vfd->contentanimate();
                vfd->waveanimate();
                // 任务延时 10 毫秒
                vTaskDelay(pdMS_TO_TICKS(10));
                // 可取消注释以打印任务空闲栈大小
                // UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
                // ESP_LOGI(TAG, "Task refrash free stack size: %u bytes", uxHighWaterMark * sizeof(StackType_t));
            }
            // 删除当前任务
            vTaskDelete(NULL);
        },
        "vfd",
        4096 - 1024,
        this,
        6,
        nullptr);
}

/**
 * @brief 显示频谱信息。
 *
 * 处理传入的频谱数据，计算每个频段的平均值，并应用增益。
 * 更新目标值和动画步数，用于后续动画效果。
 *
 * @param buf 频谱数据缓冲区，包含每个频段的幅度值。
 * @param size 缓冲区的大小，即频谱数据的数量。
 */
void HNA_16MM65T::spectrum_show(float *buf, int size) // 0-100
{
    wave_start_time = esp_timer_get_time() / 1000;
#if true
    if (size < 512)
        return;
    // 定义每个频段的增益系数
    static float fft_gain[FFT_SIZE] = {1.8f * 2, 2.2f * 2, 2.6f * 2, 2.8f * 2, 3.0f * 2, 3.0f * 2, 3.0f * 2, 3.0f * 2, 3.0f * 2, 3.0f * 2, 3.0f * 2, 3.0f * 2};
    // 定义每个频段的显示位置映射
    static uint8_t fft_postion[FFT_SIZE] = {0, 2, 4, 6, 8, 10, 11, 9, 7, 5, 3, 1};
    // 记录最大幅度值
    static float max = 0;
    // 存储每个频段的平均幅度值
    float fft_buf[FFT_SIZE];
    int sum = 0;
    // 计算每个频段包含的数据元素数量
    int elements_per_part = size / 4 / 12;
    // 计算每个频段的平均幅度值
    for (int i = 0; i < FFT_SIZE; i++)
    {
        int max_val = 0;
        for (int j = 0; j < elements_per_part; j++)
        {
            if (max_val < buf[(i + 3) * elements_per_part + j])
                max_val = buf[(i + 3) * elements_per_part + j];
        }
        sum += max_val;
        fft_buf[i] = max_val;
        // 更新最大幅度值
        if (max < fft_buf[i])
        {
            max = fft_buf[i];
        }

        // 确保幅度值非负，并应用增益
        if (fft_buf[i] < 0)
            fft_buf[i] = -fft_buf[i];
    }
    // if (sum < 30)
    // {
    //     for (size_t i = 0; i < FFT_SIZE; i++)
    //     {
    //         fft_buf[i] = 0;
    //     }
    //     ESP_LOGI(TAG, "Sound: %d", (int)max);
    // }

#else
#endif
    wavebusy = false;
    // 更新上一次的值、目标值和动画步数
    for (size_t i = 0; i < FFT_SIZE; i++)
    {
        waveData[i].last_value = waveData[i].target_value;
        waveData[i].target_value = fft_buf[fft_postion[i]] * fft_gain[fft_postion[i]];
        waveData[i].animation_step = 0;
    }
    // 打印最大幅度值和每个频段的目标值
    // ESP_LOGI(TAG, "%d-FFT: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d", (int)max, wave_target_values[0], wave_target_values[1], wave_target_values[2], wave_target_values[3], wave_target_values[4], wave_target_values[5],
    //          wave_target_values[6], wave_target_values[7], wave_target_values[8], wave_target_values[9], wave_target_values[10], wave_target_values[11]);
}
void HNA_16MM65T::time_blink()
{
    static bool time_mark = true;
    if (content_inhibit_time != 0)
    {
        return;
    }
    time_mark = !time_mark;

    symbolhelper(NUM6_MARK, time_mark);
    symbolhelper(NUM8_MARK, time_mark);
}

void HNA_16MM65T::content_show(int start, char *buf, int size, NumAni ani)
{
    if (content_inhibit_time != 0)
    {
        for (size_t i = 0; i < size && (start + i) < NUM_SIZE; i++)
        {
            tempData[start + i].animation_type = ani;
            tempData[start + i].current_content = buf[i];
        }
        return;
    }
    for (size_t i = 0; i < size && (start + i) < NUM_SIZE; i++)
    {
        currentData[start + i].animation_type = ani;
        currentData[start + i].current_content = buf[i];
    }
}

void HNA_16MM65T::noti_show(int start, char *buf, int size, NumAni ani, int timeout)
{
    content_inhibit_time = esp_timer_get_time() / 1000 + timeout;
    for (size_t i = 0; i < size && (start + i) < NUM_SIZE; i++)
    {
        currentData[start + i].animation_type = ani;
        currentData[start + i].current_content = buf[i];
    }
}
/**
 * @brief 测试函数，创建一个任务用于模拟频谱数据显示。
 *
 * 创建一个任务，在任务中随机生成频谱数据并调用 spectrum_show 函数显示。
 * 同时可以进行数字显示和点矩阵显示的测试。
 */
void HNA_16MM65T::test()
{
    wavebusy = false;
    // 创建一个任务用于测试显示功能
    xTaskCreate(
        [](void *arg)
        {
            HNA_16MM65T *vfd = static_cast<HNA_16MM65T *>(arg);
            float testbuff[FFT_SIZE];
            int rollcounter = 0;
            NumAni num_ani = ANI_ANTICLOCKWISE;
            char tempstr[NUM_SIZE];
            // 获取初始时间戳（单位：毫秒）
            int64_t start_time = esp_timer_get_time() / 1000;
            while (1)
            {
                int64_t current_time = esp_timer_get_time() / 1000;

                int64_t elapsed_time = current_time - start_time;

                if (elapsed_time >= 5000)
                {
                    num_ani = (NumAni)((int)(num_ani + 1) % ANI_MAX);
                    start_time = current_time;
                }

                snprintf(tempstr, NUM_SIZE, "ABC%dDEF", (rollcounter++) % 100);
                vfd->content_show(0, tempstr, NUM_SIZE, num_ani);

                // for (int i = 0; i < FFT_SIZE; i++)
                //     testbuff[i] = rand() % 100;
                // vfd->spectrum_show(testbuff, FFT_SIZE);
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            // 删除当前任务
            vTaskDelete(NULL);
        },
        "vfd_test",
        4096 - 1024,
        this,
        5,
        nullptr);
}

void HNA_16MM65T::cali()
{
    // Configure USB SERIAL JTAG
    usb_serial_jtag_driver_config_t usb_serial_jtag_config = {
        .tx_buffer_size = BUF_SIZE,
        .rx_buffer_size = BUF_SIZE,
    };
    wavebusy = false;
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb_serial_jtag_config));
    uint8_t *recv_data = (uint8_t *)malloc(BUF_SIZE);
    while (1)
    {
        memset(recv_data, 0, BUF_SIZE);
        int len = usb_serial_jtag_read_bytes(recv_data, BUF_SIZE - 1, 0x20 / portTICK_PERIOD_MS);
        if (len > 0)
        {
            // dotshelper((Dots)((recv_data[0] - '0') % 4));
            // for (int i = 0; i < 10; i++)
            //     charhelper(i, recv_data[0]);
            // for (size_t i = 0; i < 12; i++)
            //     wavehelper(i, (recv_data[0] - '0') % 9);
            int index = 0, data = 0;

            sscanf((char *)recv_data, "%d:%X", &index, &data);
            printf("Parsed contents: %d and 0x%02X\n", index, data);
            gram[index] = data;
            // pt6324_refrash(gram);
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void HNA_16MM65T::InitializeBacklight()
{
    Settings settings("display", false);
    brightness_ = settings.GetInt("bright", 80);
    SetBacklight(brightness_);
}

void HNA_16MM65T::SetBacklight(uint8_t brightness)
{
    char tempstr[11] = {0};
    pt6324_setbrightness(brightness);
    sprintf(tempstr, "BRIGHT:%d", brightness);
    noti_show(0, tempstr, 10, ANI_UP2DOWN);
}

bool HNA_16MM65T::Lock(int timeout_ms)
{
    return true;
}

void HNA_16MM65T::Unlock()
{
}

/**
 * @brief 根据字符查找对应的十六进制编码。
 *
 * 在字符数组中查找给定字符，并返回其对应的十六进制编码。
 *
 * @param ch 要查找的字符。
 * @return 字符对应的十六进制编码，如果未找到则返回 0。
 */
unsigned int HNA_16MM65T::find_hex_code(char ch)
{
    if (ch >= ' ' && ch <= 'Z')
        return hex_codes[ch - ' '];
    else if (ch >= 'a' && ch <= 'z')
        return hex_codes[ch - 'a' + 'A' - ' '];
    return 0;
}

/**
 * @brief 显示数字字符。
 *
 * 根据给定的索引和字符，查找对应的十六进制编码，并更新显示缓冲区。
 *
 * @param index 数字显示的索引位置。
 * @param ch 要显示的字符。
 */
void HNA_16MM65T::charhelper(int index, char ch)
{
    // 检查索引是否越界
    if (index >= 10)
        return;
    // 查找字符对应的十六进制编码
    uint32_t val = find_hex_code(ch);
    // 更新显示缓冲区
    gram[NUM_BEGIN + index * 3 + 2] = val >> 16;
    gram[NUM_BEGIN + index * 3 + 1] = val >> 8;
    gram[NUM_BEGIN + index * 3 + 0] = val & 0xff;
}

void HNA_16MM65T::charhelper(int index, uint32_t code)
{
    // 检查索引是否越界
    if (index >= 10)
        return;
    // 更新显示缓冲区
    gram[NUM_BEGIN + index * 3 + 2] = code >> 16;
    gram[NUM_BEGIN + index * 3 + 1] = code >> 8;
    gram[NUM_BEGIN + index * 3 + 0] = code & 0xff;
}

/**
 * @brief 根据符号枚举值查找其在显示缓冲区中的位置。
 *
 * 该函数接收一个符号枚举值，通过查询 `symbolPositions` 数组，
 * 找到该符号对应的字节索引和位索引，并将其存储在传入的指针所指向的变量中。
 *
 * @param flag 符号的枚举值，用于标识要查找的符号。
 * @param byteIndex 指向一个整数的指针，用于存储符号所在的字节索引。
 * @param bitIndex 指向一个整数的指针，用于存储符号所在的位索引。
 */
void HNA_16MM65T::find_enum_code(Symbols flag, int *byteIndex, int *bitIndex)
{
    *byteIndex = symbolPositions[flag].byteIndex;
    *bitIndex = symbolPositions[flag].bitIndex;
}

/**
 * @brief 控制特定符号的显示状态。
 *
 * 根据传入的符号枚举值和显示状态标志，找到该符号在显示缓冲区中的位置，
 * 并相应地设置或清除该位置的位，以控制符号的显示或隐藏。
 *
 * @param symbol 要控制的符号的枚举值。
 * @param is_on 一个布尔值，指示符号是否应该显示（true 表示显示，false 表示隐藏）。
 */
void HNA_16MM65T::symbolhelper(Symbols symbol, bool is_on)
{
    // 检查符号枚举值是否越界，如果越界则直接返回
    if (symbol >= SYMBOL_MAX)
        return;

    int byteIndex, bitIndex;
    // 调用 find_enum_code 函数查找符号在显示缓冲区中的位置
    find_enum_code(symbol, &byteIndex, &bitIndex);

    // 可取消注释以打印符号的位置信息
    // printf("symbol %d 所在字节: %d, 所在位: %d\n", symbol, byteIndex, bitIndex);

    if (is_on)
        // 如果要显示符号，则将该位置的位设置为 1
        gram[byteIndex] |= bitIndex;
    else
        // 如果要隐藏符号，则将该位置的位设置为 0
        gram[byteIndex] &= ~bitIndex;
}

/**
 * @brief 根据不同的点矩阵状态更新显示缓冲区。
 *
 * 该函数根据传入的点矩阵状态枚举值，对显示缓冲区中的特定字节进行操作，
 * 以实现不同的点矩阵显示效果。在操作之前，会先清除相关字节中的特定位。
 *
 * @param dot 点矩阵的状态枚举值，指示要显示的点矩阵样式。
 */
void HNA_16MM65T::dotshelper(Dots dot)
{
    // 清除 gram[1] 字节中从第 3 位到第 7 位的位
    gram[1] &= ~0xF8;
    // 清除 gram[2] 字节中从第 0 位到第 3 位的位
    gram[2] &= ~0xF;

    switch (dot)
    {
    case DOT_MATRIX_UP:
        // 如果是向上的点矩阵样式，设置 gram[1] 字节的相应位
        gram[1] |= 0x78;
        break;
    case DOT_MATRIX_NEXT:
        // 如果是下一个的点矩阵样式，设置 gram[1] 和 gram[2] 字节的相应位
        gram[1] |= 0xD0;
        gram[2] |= 0xA;
        break;
    case DOT_MATRIX_PAUSE:
        // 如果是暂停的点矩阵样式，设置 gram[1] 和 gram[2] 字节的相应位
        gram[1] |= 0xB2;
        gram[2] |= 0x1;
        break;
    case DOT_MATRIX_FILL:
        // 如果是填充的点矩阵样式，设置 gram[1] 和 gram[2] 字节的相应位
        gram[1] |= 0xF8;
        gram[2] |= 0x7;
        break;
    }
}

/**
 * @brief 根据索引和级别更新波形显示。
 *
 * 该函数根据传入的索引和级别，在显示缓冲区中更新相应的波形显示。
 * 会先检查索引和级别是否在有效范围内，然后根据级别设置或清除显示缓冲区中的位。
 *
 * @param index 波形的索引，用于确定在显示缓冲区中的起始位置。
 * @param level 波形的级别，指示波形的高度，范围通常为 0 到 8。
 */
void HNA_16MM65T::wavehelper(int index, int level)
{
    // 定义每个波形在显示缓冲区中的起始位置
    static SymbolPosition wavePositions[] = {
        {33, 0x10},
        {33, 8},
        {33, 4},

        {36, 0x10},
        {36, 8},
        {36, 4},

        {42, 4},
        {42, 8},
        {42, 0x10},

        {45, 4},
        {45, 8},
        {45, 0x10},
    };

    // 检查索引是否越界，如果越界则直接返回
    if (index >= 12)
        return;
    // 确保级别不超过 8
    if (level > 8)
        level = 8;

    int byteIndex = wavePositions[index].byteIndex, bitIndex = wavePositions[index].bitIndex;

    if (!wavebusy)
        gram[byteIndex + 2] |= 0x80;

    for (size_t i = 0; i < 7; i++)
    {
        if ((i) >= (8 - level) && level > 1)
            // 如果当前位置在波形高度范围内，则设置相应位
            gram[byteIndex] |= bitIndex;
        else
            // 否则清除相应位
            gram[byteIndex] &= ~bitIndex;

        // 位索引左移 3 位
        bitIndex <<= 3;
        if (bitIndex > 0xFF)
        {
            // 如果位索引超过一个字节的范围，则右移 8 位并将字节索引加 1
            bitIndex >>= 8;
            byteIndex++;
        }
    }
}

void HNA_16MM65T::corewavehelper(int l_level, int r_level) // 0-100
{
    static int rollcount = 0;
    static int64_t start_time = esp_timer_get_time() / 1000;
    int64_t current_time = esp_timer_get_time() / 1000;

    int64_t elapsed_time = current_time - start_time;

    if (elapsed_time >= 30)
        start_time = current_time;
    else
        return;

    gram[0] &= 0x80;
    gram[COREWAVE_BEGIN] = gram[COREWAVE_BEGIN + 1] = gram[COREWAVE_BEGIN + 2] = 0;
    if (wavebusy)
    {
        uint16_t core_level = (((1 << 3) - 1) << 8) | ((1 << 3) - 1);
        core_level = (core_level << rollcount) | (core_level >> (8 - rollcount));
        rollcount = (rollcount + 1) % 8;
        gram[COREWAVE_BEGIN + 1] = core_level >> 8;
        gram[COREWAVE_BEGIN + 1 + 1] = core_level & 0xFF;
        // gram[COREWAVE_BEGIN] |= 0x3;
        return;
    }
    if (l_level > 8)
        l_level = 8;
    if (r_level > 8)
        r_level = 8;
    uint16_t core_level = (((1 << l_level) - 1) << 6) | ((1 << r_level) - 1);

    core_level = (core_level << rollcount) | (core_level >> (6 - rollcount));
    rollcount = (rollcount + 1) % 8;

    if (l_level > 1)
        gram[0] |= 0x40;
    if (l_level > 3)
        gram[0] |= 0x20;
    if (l_level > 5)
        gram[0] |= 0x10;

    if (r_level > 1)
        gram[0] |= 0x8;
    if (r_level > 3)
        gram[0] |= 0x4;
    if (r_level > 5)
        gram[0] |= 0x2;

    if (l_level > 3 || r_level > 3)
    {
        gram[COREWAVE_BEGIN + 1] = core_level >> 8;
        gram[COREWAVE_BEGIN + 1 + 1] = core_level & 0x3F;
    }

    gram[COREWAVE_BEGIN + 1 + 1] |= 0x80;

    if (l_level > 2 || r_level > 2)
    {
        gram[COREWAVE_BEGIN + 1 + 1] |= 0x40;
    }

    if (l_level > 4)
    {
        gram[COREWAVE_BEGIN] |= 0x40;
    }
    if (r_level > 4)
    {
        gram[COREWAVE_BEGIN] |= 0x10;
    }
    if (l_level > 5)
    {
        gram[COREWAVE_BEGIN] |= 0x20;
    }
    if (r_level > 5)
    {
        gram[COREWAVE_BEGIN] |= 0x80;
    }
    if (l_level > 6)
    {
        gram[COREWAVE_BEGIN] |= 0x4;
    }
    if (r_level > 6)
    {
        gram[COREWAVE_BEGIN] |= 0x8;
    }
    if (l_level > 7)
    {
        gram[COREWAVE_BEGIN] |= 0x1;
    }
    if (r_level > 7)
    {
        gram[COREWAVE_BEGIN] |= 0x2;
    }
}

void HNA_16MM65T::OnStateChanged()
{
    auto &app = Application::GetInstance();
    auto device_state = app.GetDeviceState();
    symbolhelper(GIGA, false);
    symbolhelper(MONO, false);
    symbolhelper(STEREO, false);
    symbolhelper(REC_1, false);
    symbolhelper(REC_2, false);
    symbolhelper(USB1, false);
    dotshelper(DOT_MATRIX_FILL);
    switch (device_state)
    {
    case kDeviceStateStarting:
        symbolhelper(GIGA, true);
        break;
    case kDeviceStateWifiConfiguring:
        symbolhelper(MONO, true);
        break;
    case kDeviceStateIdle:
        break;
    case kDeviceStateConnecting:
        symbolhelper(STEREO, true);
        break;
    case kDeviceStateListening:
        if (app.IsVoiceDetected())
        {
            symbolhelper(REC_1, true);
            symbolhelper(REC_2, true);
        }
        else
        {
            symbolhelper(REC_2, true);
        }
        break;
    case kDeviceStateSpeaking:
        dotshelper(DOT_MATRIX_NEXT);
        break;
    case kDeviceStateUpgrading:
        symbolhelper(USB1, true);
        break;
    default:
        ESP_LOGE(TAG, "Invalid led strip event: %d", device_state);
        return;
    }
}