/*
 * ir.h
 * 
 * 防跌落(IR)传感器驱动
 */

#ifndef _IR_H_
#define _IR_H_

#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

// ================= 用户配置区域 =================

// 4个防跌落传感器引脚 (输入)
#define IR_GPIO_SENSOR_1    GPIO_NUM_2
#define IR_GPIO_SENSOR_2    GPIO_NUM_3
#define IR_GPIO_SENSOR_3    GPIO_NUM_4
#define IR_GPIO_SENSOR_4    GPIO_NUM_5

// IR 发射管使能引脚 (输出)
#define IR_GPIO_ENABLE      6

// 中断触发类型
// GPIO_INTR_NEGEDGE: 下降沿触发 (例如：平时高电平，遇到悬崖变低电平)
// GPIO_INTR_POSEDGE: 上升沿触发
// GPIO_INTR_LOW_LEVEL: 低电平触发
#define IR_INTR_TYPE        GPIO_INTR_NEGEDGE

// 传感器输入上下拉配置
#define IR_PULL_UP_EN       1  // 1: 开启内部上拉
#define IR_PULL_DOWN_EN     0  // 0: 关闭内部下拉

// ==============================================

// 传感器ID枚举
typedef enum {
    IR_SENSOR_1 = 0,
    IR_SENSOR_2 = 1,
    IR_SENSOR_3 = 2,
    IR_SENSOR_4 = 3,
    IR_SENSOR_NUM = 4
} ir_sensor_id_t;

typedef void (*ir_stop_cb_t)(ir_sensor_id_t sensor_id);

/**
 * @brief 初始化 IR 模块和 GPIO 中断
 * @param stop_cb 发生中断时要调用的函数指针 (必须放在 IRAM 中)
 */
esp_err_t ir_init(ir_stop_cb_t stop_cb);

/**
 * @brief 开启 IR 发射 (使能引脚置 1)
 */
void ir_enable(void);

/**
 * @brief 关闭 IR 发射 (使能引脚置 0)
 */
void ir_disable(void);

void ir_mute(void);
void ir_unmute(void);

void ir_trigger_count_get(ir_sensor_id_t sensor_id, uint8_t* count);

#ifdef __cplusplus
}
#endif

#endif // _IR_H_