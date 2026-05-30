/*
 * ir.c
 * 
 * 防跌落(IR)传感器驱动实现
 */

#include "ir.h"
#include "esp_attr.h"  // for IRAM_ATTR
#include "esp_log.h"

static const char *TAG = "IR_MOD";

// 本地保存的回调函数指针
static ir_stop_cb_t s_stop_func = NULL;
static bool s_ir_is_mute = false;
static uint8_t s_ir_trigger_count[IR_SENSOR_NUM] = {0};

/**
 * @brief 中断服务程序
 * @note IRAM_ATTR 强制代码驻留内存，防止 Flash 读取延迟，确保秒停
 */
static void IRAM_ATTR ir_sensor_isr_handler(void* arg)
{
    ir_sensor_id_t sensor_id = (ir_sensor_id_t)(uintptr_t)arg;

    s_ir_trigger_count[sensor_id]++;

    // 只要有任意一个触发，直接调用停止函数
    if (s_stop_func) {
        s_stop_func(sensor_id);
    }
}

esp_err_t ir_init(ir_stop_cb_t stop_cb)
{
    esp_err_t ret;

    // 保存回调函数
    s_stop_func = stop_cb;

    // 1. 配置 IR 使能引脚 (Output)
    gpio_config_t io_conf_out = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << IR_GPIO_ENABLE),
        .pull_down_en = 0,
        .pull_up_en = 0
    };
    ret = gpio_config(&io_conf_out);
    if (ret != ESP_OK) return ret;

    // 默认关闭 IR
    ir_disable();

    // 2. 配置 4 个传感器引脚 (Input)
    uint64_t sensor_mask = (1ULL << IR_GPIO_SENSOR_1) |
                           (1ULL << IR_GPIO_SENSOR_2) |
                           (1ULL << IR_GPIO_SENSOR_3) |
                           (1ULL << IR_GPIO_SENSOR_4);

    gpio_config_t io_conf_in = {
        .intr_type = IR_INTR_TYPE,    // 使用宏定义的触发类型
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = sensor_mask,
        .pull_up_en = IR_PULL_UP_EN,  // 使用宏定义的上拉配置
        .pull_down_en = IR_PULL_DOWN_EN
    };
    
    ret = gpio_config(&io_conf_in);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Sensor GPIO config failed");
        return ret;
    }

    // 3. 安装 ISR 服务
    // ESP_INTR_FLAG_IRAM: 允许 ISR 在 Cache 禁用时运行 (关键安全特性)
    ret = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Install ISR service failed");
        return ret;
    }

    // 4. 为这4个引脚注册同一个处理函数
    gpio_isr_handler_add(IR_GPIO_SENSOR_1, ir_sensor_isr_handler, 
                         (void*)(uintptr_t)IR_SENSOR_1);  // 传递ID
    gpio_isr_handler_add(IR_GPIO_SENSOR_2, ir_sensor_isr_handler, 
                         (void*)(uintptr_t)IR_SENSOR_2);
    gpio_isr_handler_add(IR_GPIO_SENSOR_3, ir_sensor_isr_handler, 
                         (void*)(uintptr_t)IR_SENSOR_3);
    gpio_isr_handler_add(IR_GPIO_SENSOR_4, ir_sensor_isr_handler, 
                         (void*)(uintptr_t)IR_SENSOR_4);

    ESP_LOGI(TAG, "IR Anti-drop initialized");
    return ESP_OK;
}

void ir_enable(void)
{
    gpio_set_level(IR_GPIO_ENABLE, 1);
}

void ir_disable(void)
{
    gpio_set_level(IR_GPIO_ENABLE, 0);
}

void ir_mute(void)
{
    if(s_ir_is_mute)
    {
        return;
    }
    s_ir_is_mute = true;
    gpio_isr_handler_remove(IR_GPIO_SENSOR_1);
    gpio_isr_handler_remove(IR_GPIO_SENSOR_2);
    gpio_isr_handler_remove(IR_GPIO_SENSOR_3);
    gpio_isr_handler_remove(IR_GPIO_SENSOR_4);
}

void ir_unmute(void)
{
    if(!s_ir_is_mute)
    {
        return;
    }
    s_ir_is_mute = false;
    gpio_isr_handler_add(IR_GPIO_SENSOR_1, ir_sensor_isr_handler,
                         (void*)(uintptr_t)IR_SENSOR_1);
    gpio_isr_handler_add(IR_GPIO_SENSOR_2, ir_sensor_isr_handler,
                         (void*)(uintptr_t)IR_SENSOR_2);
    gpio_isr_handler_add(IR_GPIO_SENSOR_3, ir_sensor_isr_handler,
                         (void*)(uintptr_t)IR_SENSOR_3);
    gpio_isr_handler_add(IR_GPIO_SENSOR_4, ir_sensor_isr_handler,
                         (void*)(uintptr_t)IR_SENSOR_4);
}

void ir_trigger_count_get(ir_sensor_id_t sensor_id, uint8_t* count)
{
    if (sensor_id >= IR_SENSOR_NUM) {
        ESP_LOGE(TAG, "Invalid sensor ID");
        return;
    }
    *count = s_ir_trigger_count[sensor_id];
}
