#ifndef _TM1637_H_
#define _TM1637_H_

#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// 命令字节
#define TM1637_CMD_SET_DATA        0x40 // 设置数据命令
#define TM1637_CMD_SET_ADDR        0xC0 // 设置地址命令
#define TM1637_CMD_SET_DISPLAY     0x80 // 设置显示控制命令

// 显示亮度参数
#define TM1637_BRIGHT_OFF      0x80    // 关闭显示
#define TM1637_BRIGHT1         0x88    // 亮度级别1（最低）
#define TM1637_BRIGHT2         0x89    // 亮度级别2
#define TM1637_BRIGHT3         0x8A    // 亮度级别3
#define TM1637_BRIGHT4         0x8B    // 亮度级别4
#define TM1637_BRIGHT5         0x8C    // 亮度级别5
#define TM1637_BRIGHT6         0x8D    // 亮度级别6
#define TM1637_BRIGHT7         0x8E    // 亮度级别7
#define TM1637_BRIGHT8         0x8F    // 亮度级别8（最高）

// 数码管位置
#define TM1637_DIG1            0
#define TM1637_DIG2            1
#define TM1637_DIG3            2
#define TM1637_DIG4            3

#ifdef __cplusplus
extern "C" {
#endif

// 函数原型
void TM1637_init(void);
void TM1637_cfg_display(uint8_t param);
void TM1637_clear(void);
void TM1637_print(uint8_t dig, uint8_t seg_data);
void TM1637_print_number(uint16_t number, bool leading_zeros);
void TM1637_display_time(uint8_t hours, uint8_t minutes);
void TM1637_print_cycle(void);

#ifdef __cplusplus
}
#endif

#endif // _TM1637_H_ 