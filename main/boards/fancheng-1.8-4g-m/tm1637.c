#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"  // 需要用us延迟函数
#include "tm1637.h"

// 定义TM1637的GPIO引脚
#define TM1637_CLK_PIN      42
#define TM1637_DIO_PIN      41

// 定义GPIO控制宏
#define TM1637_CLK(x)       gpio_set_level(TM1637_CLK_PIN, (x))
#define TM1637_DIO(x)       gpio_set_level(TM1637_DIO_PIN, (x))
#define TM1637_DIO_READ     gpio_get_level(TM1637_DIO_PIN)

// 数字0-9和A-F的段码表（共阳七段数码管）
const uint8_t TM1637_DIGIT_TABLE[16] = {
    0x3f,    /* 0  -> 0b00111111 */
    0x06,    /* 1  -> 0b00000110 */
    0x5b,    /* 2  -> 0b01011011 */
    0x4f,    /* 3  -> 0b01001111 */
    0x66,    /* 4  -> 0b01100110 */
    0x6d,    /* 5  -> 0b01101101 */
    0x7d,    /* 6  -> 0b01111101 */
    0x07,    /* 7  -> 0b00000111 */
    0x7f,    /* 8  -> 0b01111111 */
    0x6f,    /* 9  -> 0b01101111 */
    0x77,    /* A  -> 0b01110111 */
    0x7c,    /* b  -> 0b01111100 */
    0x39,    /* C  -> 0b00111001 */
    0x5e,    /* d  -> 0b01011110 */
    0x79,    /* E  -> 0b01111001 */
    0x71     /* F  -> 0b01110001 */
};

// 带小数点的段码表
const uint8_t TM1637_DIGIT_DP_TABLE[10] = {0xbf, 0x86, 0xdb, 0xcf, 0xe6, 0xed, 0xfd, 0x87, 0xff, 0xef};

// 函数声明
static void TM1637_delay(void);
static void TM1637_start(void);
static void TM1637_stop(void);
static void TM1637_write_byte(uint8_t data);
static uint8_t TM1637_read_ack(void);

/**
 * @brief       TM1637延迟函数，用于控制通信速度
 * @param       无
 * @retval      无
 */
static void TM1637_delay(void)
{
    esp_rom_delay_us(5);  // 5us延迟
}

/**
 * @brief       生成TM1637起始信号
 * @param       无
 * @retval      无
 */
static void TM1637_start(void)
{
    TM1637_CLK(1);
    TM1637_DIO(1);
    TM1637_delay();
    TM1637_DIO(0);  // DIO从高到低变化，时钟线为高电平，产生起始信号
    TM1637_delay();
    TM1637_CLK(0);  // 拉低时钟线，准备发送数据
    TM1637_delay();
}

/**
 * @brief       生成TM1637停止信号
 * @param       无
 * @retval      无
 */
static void TM1637_stop(void)
{
    TM1637_CLK(0);
    TM1637_delay();
    TM1637_DIO(0);
    TM1637_delay();
    TM1637_CLK(1);  // 时钟线拉高
    TM1637_delay();
    TM1637_DIO(1);  // DIO从低到高变化，产生停止信号
    TM1637_delay();
}

/**
 * @brief       向TM1637写入一个字节
 * @param       data: 要写入的数据
 * @retval      无
 */
static void TM1637_write_byte(uint8_t data)
{
    for (uint8_t i = 0; i < 8; i++) {
        TM1637_CLK(0);
        TM1637_delay();
        
        // 发送数据位，LSB先发
        TM1637_DIO((data & 0x01) ? 1 : 0);
        TM1637_delay();
        
        // 产生一个时钟信号
        TM1637_CLK(1);
        TM1637_delay();
        
        data >>= 1;  // 右移1位，准备发送下一位
    }
    
    // 等待ACK（第9个时钟周期）
    TM1637_CLK(0);  // 拉低时钟线
    TM1637_delay();
    TM1637_DIO(1);  // 释放DIO线
    TM1637_delay();
    TM1637_CLK(1);  // 拉高时钟线
    TM1637_delay();
    
    // 实际应用中可以读取ACK
    uint8_t ack = TM1637_DIO_READ;  // 读取ACK位（0 = ACK）
    
    TM1637_CLK(0);  // 拉低时钟线，结束ACK周期
    TM1637_delay();
}

/**
 * @brief       读取TM1637的ACK信号
 * @param       无
 * @retval      ACK信号（0表示有效ACK）
 */
static uint8_t TM1637_read_ack(void)
{
    uint8_t ack;
    
    TM1637_CLK(0);
    TM1637_delay();
    TM1637_DIO(1);  // 释放DIO线
    TM1637_delay();
    TM1637_CLK(1);
    TM1637_delay();
    
    ack = TM1637_DIO_READ;  // 读取ACK位（0 = ACK）
    
    TM1637_CLK(0);
    TM1637_delay();
    
    return ack;
}

/**
 * @brief       初始化TM1637和GPIO引脚
 * @param       无
 * @retval      无
 */
void TM1637_init(void)
{
    // 配置CLK引脚为输出
    gpio_config_t config1 = {
        .pin_bit_mask = (1ULL << TM1637_CLK_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&config1));
    
    // 配置DIO引脚为开漏输出（需要上拉电阻）
    gpio_config_t config2 = {
        .pin_bit_mask = (1ULL << TM1637_DIO_PIN),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&config2));
    
    // 设置初始引脚状态
    TM1637_DIO(1);
    TM1637_CLK(1);
    
    // 配置显示为最大亮度
    TM1637_cfg_display(TM1637_BRIGHT8);
    
    // 清除显示
    TM1637_clear();
}

/**
 * @brief       配置TM1637显示亮度
 * @param       param: TM1637_BRIGHT_OFF (关闭显示)
 *                     TM1637_BRIGHT1-8 (亮度级别1-8，打开显示)
 * @retval      无
 */
void TM1637_cfg_display(uint8_t param)
{
    TM1637_start();
    TM1637_write_byte(param);
    TM1637_stop();
}

/**
 * @brief       清除显示（所有段都设为0）
 * @param       无
 * @retval      无
 */
void TM1637_clear(void)
{
    // 固定地址模式写入数据
    TM1637_start();
    TM1637_write_byte(TM1637_CMD_SET_DATA);  // 数据命令: 自动地址，写数据
    TM1637_stop();
    
    // 设置地址，从0开始写入4个数码管
    TM1637_start();
    TM1637_write_byte(TM1637_CMD_SET_ADDR);  // 地址命令: 从00H开始
    
    // 写入4个0（清空所有数码管）
    for (uint8_t i = 0; i < 4; i++) {
        TM1637_write_byte(0x00);
    }
    
    TM1637_stop();
}

/**
 * @brief       在指定位置显示自定义段码
 * @param       dig: 数码管位置 (0-3)
 * @param       seg_data: 段码数据
 * @retval      无
 */
void TM1637_print(uint8_t dig, uint8_t seg_data)
{
    // 固定地址模式写入数据
    TM1637_start();
    TM1637_write_byte(TM1637_CMD_SET_DATA);  // 数据命令: 固定地址，写数据
    TM1637_stop();
    
    // 设置地址并写入数据
    TM1637_start();
    TM1637_write_byte(TM1637_CMD_SET_ADDR | dig);  // 地址命令: 设置地址
    TM1637_write_byte(seg_data);               // 写入段码数据
    TM1637_stop();
}

/**
 * @brief       在4位数码管上显示数字
 * @param       number: 要显示的数字 (0-9999)
 * @param       leading_zeros: 是否显示前导零
 * @retval      无
 */
void TM1637_print_number(uint16_t number, bool leading_zeros)
{
    uint8_t digit_values[4] = {0};
    uint8_t leading = leading_zeros ? 0 : 1;
    
    // 提取各个位的数字
    digit_values[0] = number / 1000;          // 千位
    digit_values[1] = (number % 1000) / 100;  // 百位
    digit_values[2] = (number % 100) / 10;    // 十位
    digit_values[3] = number % 10;            // 个位
    
    // 自动地址增加模式
    TM1637_start();
    TM1637_write_byte(TM1637_CMD_SET_DATA);  // 数据命令: 自动地址增加
    TM1637_stop();
    
    // 从地址0开始写入数据
    TM1637_start();
    TM1637_write_byte(TM1637_CMD_SET_ADDR);  // 地址命令: 从00H开始
    
    // 依次写入各个位的数据
    for (uint8_t i = 0; i < 4; i++) {
        // 检查是否为前导0
        if (i < 3 && digit_values[i] == 0 && leading) {
            TM1637_write_byte(0x00);  // 不显示前导0
        } else {
            TM1637_write_byte(TM1637_DIGIT_TABLE[digit_values[i]]);
            leading = 0;  // 后续不再考虑前导0
        }
    }
    
    TM1637_stop();
}

/**
 * @brief       显示时间，格式为H.MM（小时带小数点，分钟）
 * @param       hours: 要显示的小时 (0-23)
 * @param       minutes: 要显示的分钟 (0-59)
 * @retval      无
 * @note        如果小时<10，十位将为空
 */
void TM1637_display_time(uint8_t hours, uint8_t minutes)
{
    uint8_t hour_tens, hour_units, min_tens, min_units;
    uint8_t digit_values[4] = {0};
    
    // 提取各个位的数字
    hour_tens = hours / 10;
    hour_units = hours % 10;
    min_tens = minutes / 10;
    min_units = minutes % 10;
    
    // 准备各个位的显示数据
    digit_values[0] = (hour_tens > 0) ? TM1637_DIGIT_TABLE[hour_tens] : 0x00;
    digit_values[1] = TM1637_DIGIT_DP_TABLE[hour_units];  // 带小数点
    digit_values[2] = TM1637_DIGIT_TABLE[min_tens];
    digit_values[3] = TM1637_DIGIT_TABLE[min_units];
    
    // 自动地址增加模式
    TM1637_start();
    TM1637_write_byte(TM1637_CMD_SET_DATA);  // 数据命令: 自动地址增加
    TM1637_stop();
    
    // 从地址0开始写入数据
    TM1637_start();
    TM1637_write_byte(TM1637_CMD_SET_ADDR);  // 地址命令: 从00H开始
    
    // 依次写入各个位的数据
    for (uint8_t i = 0; i < 4; i++) {
        TM1637_write_byte(digit_values[i]);
    }
    
    TM1637_stop();
}

/**
 * @brief       循环显示数字表中的所有数字
 * @param       无
 * @retval      无
 */
void TM1637_print_cycle(void)
{
    uint8_t data_length = sizeof(TM1637_DIGIT_TABLE) / sizeof(TM1637_DIGIT_TABLE[0]);
    
    for (uint8_t i = 0; i < data_length; i++) {
        // 自动地址增加模式
        TM1637_start();
        TM1637_write_byte(TM1637_CMD_SET_DATA);  // 数据命令: 自动地址增加
        TM1637_stop();
        
        // 从地址0开始写入数据
        TM1637_start();
        TM1637_write_byte(TM1637_CMD_SET_ADDR);  // 地址命令: 从00H开始
        
        // 依次写入4个位置的相同数字
        for (uint8_t j = 0; j < 4; j++) {
            TM1637_write_byte(TM1637_DIGIT_TABLE[i]);
        }
        
        TM1637_stop();
        
        // 延时500ms后显示下一个数字
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
} 