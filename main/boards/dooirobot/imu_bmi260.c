#include "imu_bmi260.h"
#include "imu_bmi260_config.h"

#include <math.h>
#include <string.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char* TAG = "BMI260_DRV";

static const float s_accel_scale[BMI260_ACCEL_RANGE_MAX] = {
    1.0f / 16384.0f,
    1.0f / 8192.0f,
    1.0f / 4096.0f,
    1.0f / 2048.0f,
};

static const float s_gyro_scale[BMI260_GYRO_RANGE_MAX] = {
    1.0f / 16.4f, 1.0f / 32.8f, 1.0f / 65.6f, 1.0f / 131.2f, 1.0f / 262.4f,
};

typedef struct {
    i2c_master_dev_handle_t i2c_dev;
    int int_pin;
    bmi260_accel_range_t accel_range;
    bmi260_gyro_range_t gyro_range;
    SemaphoreHandle_t data_ready_sem;
    TaskHandle_t task_handle;
    bmi260_data_cb_t callback;
    void* callback_arg;
    bool initialized;
} bmi260_ctx_t;

static bmi260_ctx_t s_ctx = {0};
extern i2c_master_bus_handle_t i2c_bus_;

static esp_err_t reg_read(uint8_t reg, uint8_t* buf, size_t len) {
    return i2c_master_transmit_receive(s_ctx.i2c_dev, &reg, 1, buf, len, 1000);
}

static esp_err_t reg_write(uint8_t reg, const uint8_t* data, size_t len) {
    uint8_t tx[len + 1];
    tx[0] = reg;
    memcpy(&tx[1], data, len);
    return i2c_master_transmit(s_ctx.i2c_dev, tx, len + 1, 1000);
}

static esp_err_t reg_write_byte(uint8_t reg, uint8_t val) { return reg_write(reg, &val, 1); }

static esp_err_t reg_read_byte(uint8_t reg, uint8_t* val) { return reg_read(reg, val, 1); }

static void IRAM_ATTR isr_handler(void* arg) {
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(s_ctx.data_ready_sem, &woken);
    if (woken)
        portYIELD_FROM_ISR();
}

static esp_err_t read_raw(bmi260_raw_data_t* out) {
    uint8_t buf[12];
    esp_err_t ret = reg_read(BMI260_REG_DATA_ACCEL_XOUT_L, buf, 12);
    if (ret != ESP_OK)
        return ret;

    out->accel_x = (int16_t)((buf[1] << 8) | buf[0]);
    out->accel_y = (int16_t)((buf[3] << 8) | buf[2]);
    out->accel_z = (int16_t)((buf[5] << 8) | buf[4]);
    out->gyro_x = (int16_t)((buf[7] << 8) | buf[6]);
    out->gyro_y = (int16_t)((buf[9] << 8) | buf[8]);
    out->gyro_z = (int16_t)((buf[11] << 8) | buf[10]);
    out->timestamp_us = esp_timer_get_time();

    uint8_t dummy;
    reg_read_byte(BMI260_REG_INT_STATUS_1, &dummy);

    return ESP_OK;
}

static esp_err_t chip_init(void) {
    esp_err_t ret;
    uint8_t val;

    if (bmi260_config_file_len == 0)
        return ESP_ERR_INVALID_SIZE;

    ret = reg_write_byte(BMI260_REG_CMD, BMI260_CMD_SOFTRESET);
    if (ret != ESP_OK)
        return ret;
    vTaskDelay(pdMS_TO_TICKS(12));

    ret = reg_read_byte(BMI260_REG_CHIP_ID, &val);
    if (ret != ESP_OK)
        return ret;
    if (val != BMI260_CHIP_ID_VAL)
        return ESP_ERR_NOT_FOUND;

    ret = reg_write_byte(BMI260_REG_PWR_CONF, 0x00);
    if (ret != ESP_OK)
        return ret;
    vTaskDelay(pdMS_TO_TICKS(10));

    ret = reg_write_byte(BMI260_REG_INIT_CTRL, 0x00);
    if (ret != ESP_OK)
        return ret;
    esp_rom_delay_us(1000);

    reg_write_byte(BMI260_REG_INIT_ADDR_0, 0x00);
    reg_write_byte(BMI260_REG_INIT_ADDR_1, 0x00);
    esp_rom_delay_us(50);

    uint8_t* tx_buf = malloc(bmi260_config_file_len + 1);
    if (!tx_buf)
        return ESP_ERR_NO_MEM;
    tx_buf[0] = BMI260_REG_INIT_DATA;
    memcpy(&tx_buf[1], bmi260_config_file, bmi260_config_file_len);

    ret = i2c_master_transmit(s_ctx.i2c_dev, tx_buf, bmi260_config_file_len + 1, 500);
    free(tx_buf);
    if (ret != ESP_OK)
        return ret;

    ret = reg_write_byte(BMI260_REG_INIT_CTRL, 0x01);
    if (ret != ESP_OK)
        return ret;

    vTaskDelay(pdMS_TO_TICKS(200));

    ret = reg_read_byte(BMI260_REG_INTERNAL_STATUS, &val);
    if (ret != ESP_OK)
        return ret;
    if ((val & 0x0F) != 0x01)
        return ESP_FAIL;

    ret = reg_write_byte(BMI260_REG_PWR_CTRL,
                         BMI260_PWR_CTRL_ACC_EN | BMI260_PWR_CTRL_GYR_EN | BMI260_PWR_CTRL_TEMP_EN);
    if (ret != ESP_OK)
        return ret;
    vTaskDelay(pdMS_TO_TICKS(50));

    return ESP_OK;
}

static esp_err_t configure_sensor(const bmi260_config_t* cfg) {
    esp_err_t ret;
    uint8_t acc_conf = (uint8_t)cfg->accel_odr | (0x02 << 4) | (1 << 7);
    ret = reg_write_byte(BMI260_REG_ACCEL_CONFIG, acc_conf);
    if (ret != ESP_OK)
        return ret;
    ret = reg_write_byte(BMI260_REG_ACCEL_RANGE, (uint8_t)cfg->accel_range);
    if (ret != ESP_OK)
        return ret;

    uint8_t gyr_conf = (uint8_t)cfg->gyro_odr | (0x02 << 4) | (1 << 6) | (1 << 7);
    ret = reg_write_byte(BMI260_REG_GYRO_CONFIG, gyr_conf);
    if (ret != ESP_OK)
        return ret;
    ret = reg_write_byte(BMI260_REG_GYRO_RANGE, (uint8_t)cfg->gyro_range);
    if (ret != ESP_OK)
        return ret;

    s_ctx.accel_range = cfg->accel_range;
    s_ctx.gyro_range = cfg->gyro_range;
    return ESP_OK;
}

static esp_err_t configure_interrupt(int gpio_pin) {
    if (gpio_pin < 0)
        return ESP_OK;

    uint8_t int1_io = BMI260_INT_EDGE_TRIGGERED | BMI260_INT_ACTIVE_HIGH | BMI260_INT_OUTPUT_EN;
    esp_err_t ret = reg_write_byte(BMI260_REG_INT1_IO_CTRL, int1_io);
    if (ret != ESP_OK)
        return ret;

    ret = reg_write_byte(BMI260_REG_INT_LATCH, 0x00);
    if (ret != ESP_OK)
        return ret;

    ret = reg_write_byte(BMI260_REG_INT_MAP_DATA, BMI260_INT1_MAP_DRDY);
    if (ret != ESP_OK)
        return ret;

    if (!s_ctx.data_ready_sem) {
        s_ctx.data_ready_sem = xSemaphoreCreateBinary();
        if (!s_ctx.data_ready_sem)
            return ESP_ERR_NO_MEM;
    }

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_POSEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << gpio_pin),
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    ret = gpio_config(&io_conf);
    if (ret != ESP_OK)
        return ret;

    ret = gpio_isr_handler_add(gpio_pin, isr_handler, NULL);
    if (ret != ESP_OK)
        return ret;

    s_ctx.int_pin = gpio_pin;
    return ESP_OK;
}

static void data_task(void* arg) {
    bmi260_raw_data_t raw;
    bmi260_phys_data_t phys;

    while (1) {
        if (xSemaphoreTake(s_ctx.data_ready_sem, pdMS_TO_TICKS(500)) == pdTRUE) {
            if (read_raw(&raw) == ESP_OK) {
                bmi260_convert(&raw, &phys);
                if (s_ctx.callback) {
                    s_ctx.callback(&raw, &phys, s_ctx.callback_arg);
                }
            }
        }
    }
}

esp_err_t bmi260_init(const bmi260_config_t* config) {
    if (s_ctx.initialized)
        return ESP_OK;
    if (!config)
        return ESP_ERR_INVALID_ARG;

    gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.int_pin = -1;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = config->i2c_addr,
        .scl_speed_hz = 400000,
    };
    esp_err_t ret = i2c_master_bus_add_device(config->i2c_bus, &dev_cfg, &s_ctx.i2c_dev);
    if (ret != ESP_OK)
        return ret;

    ret = chip_init();
    if (ret != ESP_OK)
        goto err_remove_dev;

    ret = configure_sensor(config);
    if (ret != ESP_OK)
        goto err_remove_dev;

    ret = configure_interrupt(config->int_pin);
    if (ret != ESP_OK)
        goto err_remove_dev;

    s_ctx.initialized = true;
    return ESP_OK;

err_remove_dev:
    i2c_master_bus_rm_device(s_ctx.i2c_dev);
    if (s_ctx.data_ready_sem) {
        vSemaphoreDelete(s_ctx.data_ready_sem);
        s_ctx.data_ready_sem = NULL;
    }
    return ret;
}

esp_err_t bmi260_register_data_callback(bmi260_data_cb_t cb, void* arg) {
    s_ctx.callback = cb;
    s_ctx.callback_arg = arg;
    return ESP_OK;
}

esp_err_t bmi260_start_task(uint8_t task_priority, uint32_t stack_size) {
    if (!s_ctx.initialized)
        return ESP_ERR_INVALID_STATE;
    if (s_ctx.task_handle)
        return ESP_OK;

    BaseType_t ok =    xTaskCreatePinnedToCore(data_task, "bmi260", stack_size / sizeof(StackType_t), NULL, task_priority, &s_ctx.task_handle, 1);
    return (ok == pdPASS) ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t bmi260_read_sync(bmi260_raw_data_t* raw, bmi260_phys_data_t* phys, uint32_t timeout_ms) {
    if (!s_ctx.initialized)
        return ESP_ERR_INVALID_STATE;
    TickType_t ticks = (timeout_ms == UINT32_MAX) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    if (s_ctx.data_ready_sem) {
        if (xSemaphoreTake(s_ctx.data_ready_sem, ticks) != pdTRUE)
            return ESP_ERR_TIMEOUT;
    }
    bmi260_raw_data_t local_raw;
    esp_err_t ret = read_raw(&local_raw);
    if (ret != ESP_OK)
        return ret;
    if (raw)
        *raw = local_raw;
    if (phys)
        bmi260_convert(&local_raw, phys);
    return ESP_OK;
}

void bmi260_convert(const bmi260_raw_data_t* raw, bmi260_phys_data_t* phys) {
    if (!raw || !phys)
        return;
    float as = s_accel_scale[s_ctx.accel_range];
    float gs = s_gyro_scale[s_ctx.gyro_range];
    phys->ax = raw->accel_x * as;
    phys->ay = raw->accel_y * as;
    phys->az = raw->accel_z * as;
    phys->gx = raw->gyro_x * gs;
    phys->gy = raw->gyro_y * gs;
    phys->gz = raw->gyro_z * gs;
    phys->timestamp_us = raw->timestamp_us;
}

esp_err_t bmi260_set_accel_range(bmi260_accel_range_t range) {
    if (!s_ctx.initialized)
        return ESP_ERR_INVALID_STATE;
    if (range >= BMI260_ACCEL_RANGE_MAX)
        return ESP_ERR_INVALID_ARG;
    esp_err_t ret = reg_write_byte(BMI260_REG_ACCEL_RANGE, (uint8_t)range);
    if (ret == ESP_OK)
        s_ctx.accel_range = range;
    return ret;
}

esp_err_t bmi260_set_gyro_range(bmi260_gyro_range_t range) {
    if (!s_ctx.initialized)
        return ESP_ERR_INVALID_STATE;
    if (range >= BMI260_GYRO_RANGE_MAX)
        return ESP_ERR_INVALID_ARG;
    esp_err_t ret = reg_write_byte(BMI260_REG_GYRO_RANGE, (uint8_t)range);
    if (ret == ESP_OK)
        s_ctx.gyro_range = range;
    return ret;
}

bmi260_accel_range_t bmi260_get_accel_range(void) { return s_ctx.accel_range; }
bmi260_gyro_range_t bmi260_get_gyro_range(void) { return s_ctx.gyro_range; }
bool bmi260_is_initialized(void) { return s_ctx.initialized; }

esp_err_t bmi260_set_power(bool enable) {
    if (!s_ctx.initialized)
        return ESP_ERR_INVALID_STATE;
    uint8_t val =
        enable ? (BMI260_PWR_CTRL_ACC_EN | BMI260_PWR_CTRL_GYR_EN | BMI260_PWR_CTRL_TEMP_EN) : 0x00;
    esp_err_t ret = reg_write_byte(BMI260_REG_PWR_CTRL, val);
    if (enable)
        vTaskDelay(pdMS_TO_TICKS(50));  // 等待唤醒稳定
    return ret;
}