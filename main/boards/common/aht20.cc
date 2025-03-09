#include "aht20.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "AHT20"

AHT20::AHT20(i2c_master_bus_handle_t i2c_bus, uint8_t addr)
    : I2cDevice(i2c_bus, addr), _temperature(0.0f), _humidity(0.0f), _initialized(false) {}

bool AHT20::begin() {
    if (_initialized) return true;

    reset(); // 软复位确保设备状态

    // 检查校准状态
    if (!_check_calibration()) {
        ESP_LOGE(TAG, "Calibration failed");
        return false;
    }

    _initialized = true;
    return true;
}

void AHT20::reset() {
    _send_command(CMD_SOFT_RESET);
    vTaskDelay(pdMS_TO_TICKS(CMD_SOFT_RESET_TIME));
    _initialized = false;
}

// 私有方法实现
bool AHT20::_start_measurement(bool crc_en) {
    uint8_t buffer[7] = {0};
    status_reg_t status;

    if (!_is_device_ready()) return false;
    vTaskDelay(pdMS_TO_TICKS(CMD_INIT_TIME));

    // 发送测量命令（0xAC, 0x33, 0x00）
    _send_command(CMD_MEASUREMENT, CMD_MEASUREMENT_PARAMS_1ST, CMD_MEASUREMENT_PARAMS_2ND);
    const int MAX_RETRY = 5;
    int retry_count = 0;

    // 检查返回数据状态
    while(true) {
        vTaskDelay(pdMS_TO_TICKS(CMD_MEASUREMENT_TIME));

        // 读取数据（6字节数据 + 可选1字节CRC）
        ReadValues(buffer, crc_en ? CMD_MEASUREMENT_DATA_CRC_LEN : CMD_MEASUREMENT_DATA_LEN);

        // 检查状态寄存器
        status.raw = buffer[0];
        if (status.busy) {
            retry_count++;
            if (retry_count >= MAX_RETRY) {
                ESP_LOGE(TAG, "Device busy during measurement");
                return false;
            }
            continue;
        }
        break;
    }

    if (crc_en && !_check_crc(buffer[6], buffer, 6)) {
        ESP_LOGE(TAG, "CRC check failed");
        return false;
    }

    // 解析湿度（20bit）
    uint32_t raw_hum = ((uint32_t)buffer[1] << 12) |
                      ((uint32_t)buffer[2] << 4) |
                      (buffer[3] >> 4);
    _humidity = (raw_hum * 100.0f) / 0x100000;

    // 解析温度（20bit）
    uint32_t raw_temp = ((uint32_t)(buffer[3] & 0x0F) << 16) |
                       ((uint32_t)buffer[4] << 8) |
                       buffer[5];
    _temperature = (raw_temp * 200.0f) / 0x100000 - 50.0f;

    return true;
}

bool AHT20::_check_crc(uint8_t crc, uint8_t *data, size_t len) {
    uint8_t poly = 0x31;
    uint8_t crc_result = 0xFF;

    for (size_t i = 0; i < len; ++i) {
        crc_result ^= data[i];
        for (uint8_t bit = 0; bit < 8; ++bit) {
            if (crc_result & 0x80) {
                crc_result = (crc_result << 1) ^ poly;
            } else {
                crc_result <<= 1;
            }
        }
    }
    return crc_result == crc;
}

bool AHT20::_is_device_ready() {
    status_reg_t status;
    status.raw = _read_status();
    if((status.raw & 0x18) != 0x18) {

    }
    return !status.busy;
}

bool AHT20::_check_calibration() {
    status_reg_t status;
    status.raw = _read_status();
    if (status.cal_en) return true;

    // 发送初始化命令（0xBE, 0x08, 0x00）
    _send_command(CMD_INIT, CMD_INIT_PARAMS_1ST, CMD_INIT_PARAMS_2ND);
    vTaskDelay(pdMS_TO_TICKS(CMD_INIT_TIME));

    status.raw = _read_status();
    return status.cal_en;
}

uint8_t AHT20::_read_status() {
    status_reg_t status;
    status.raw = ReadReg(CMD_STATUS);
    return status.raw;
}

void AHT20::_send_command(uint8_t cmd) {
    WriteValue(cmd);
}

void AHT20::_send_command(uint8_t cmd, uint8_t arg1, uint8_t arg2) {
    uint8_t data[3] = {cmd, arg1, arg2};
    WriteValues(data, 3);
}

bool AHT20::get_measurements(float &temperature, float &humidity, bool crc_en) {
    if (!_initialized && !begin()) {
        ESP_LOGE(TAG, "Device not initialized");
        return false;
    }

    if (!_start_measurement(crc_en)) {
        ESP_LOGE(TAG, "Measurement failed");
        return false;
    }

    // 数据范围校验
    if (_temperature < -40.0f || _temperature > 85.0f ||
        _humidity < 0.0f || _humidity > 100.0f) {
        ESP_LOGW(TAG, "Invalid data: %.1fC, %.1f%%", _temperature, _humidity);
        return false;
    }

    temperature = _temperature;
    humidity = _humidity;
    return true;
}