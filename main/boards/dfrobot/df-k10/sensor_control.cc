#include "sensor_control.h"
#include "mcp_server.h"

#include <cJSON.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string>

#define TAG "SensorControl"

namespace {

std::string ToJsonString(cJSON* json) {
    char* str = cJSON_PrintUnformatted(json);
    std::string result(str);
    cJSON_free(str);
    cJSON_Delete(json);
    return result;
}

std::string JsonError(const std::string& message) {
    cJSON* json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "error", message.c_str());
    return ToJsonString(json);
}

i2c_master_dev_handle_t aht20_dev = nullptr;
i2c_master_dev_handle_t ltr303_dev = nullptr;
i2c_master_dev_handle_t sc7a20_dev = nullptr;

esp_err_t I2cWrite(i2c_master_dev_handle_t dev, const uint8_t* data, size_t len) {
    return i2c_master_transmit(dev, data, len, 100);
}

esp_err_t I2cReadReg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t* buf, size_t len) {
    esp_err_t ret = i2c_master_transmit(dev, &reg, 1, 50);
    if (ret != ESP_OK) return ret;
    return i2c_master_receive(dev, buf, len, 100);
}

// AHT20 — blocks ~85ms while the sensor takes a measurement.
bool ReadTemperature(float& temperature, float& humidity) {
    if (!aht20_dev) return false;
    uint8_t cmd[] = {0xAC, 0x33, 0x00};
    I2cWrite(aht20_dev, cmd, sizeof(cmd));
    vTaskDelay(pdMS_TO_TICKS(85));
    uint8_t buf[6] = {};
    if (i2c_master_receive(aht20_dev, buf, sizeof(buf), 200) != ESP_OK) return false;
    uint32_t raw_humi = ((uint32_t)(buf[1]) << 12) | ((uint32_t)(buf[2]) << 4) | (buf[3] >> 4);
    uint32_t raw_temp = ((uint32_t)(buf[3] & 0x0F) << 16) | ((uint32_t)(buf[4]) << 8) | buf[5];
    humidity = raw_humi * 100.0f / (1 << 20);
    temperature = raw_temp * 200.0f / (1 << 20) - 50.0f;
    return true;
}

// LTR303-ALS — fast, no blocking delay.
bool ReadLight(float& lux) {
    if (!ltr303_dev) return false;
    uint8_t ch1[2] = {}, ch0[2] = {};
    I2cReadReg(ltr303_dev, 0x88, ch1, 2);
    I2cReadReg(ltr303_dev, 0x8A, ch0, 2);
    uint16_t c0 = ch0[0] | (ch0[1] << 8);
    uint16_t c1 = ch1[0] | (ch1[1] << 8);
    float ratio = c0 ? (float)c1 / c0 : 0;
    if (ratio < 0.45f) lux = 1.7860f * c0 + 1.1090f * c1;
    else if (ratio < 0.64f) lux = 4.2280f * c0 - 1.9300f * c1;
    else if (ratio < 0.85f) lux = 0.5926f * c0 + 0.1185f * c1;
    else lux = 0;
    return true;
}

// SC7A20H — fast, no blocking delay.
bool ReadAccel(float& x, float& y, float& z) {
    if (!sc7a20_dev) return false;
    uint8_t buf[6] = {};
    if (I2cReadReg(sc7a20_dev, 0x28 | 0x80, buf, sizeof(buf)) != ESP_OK) return false;
    x = ((int16_t)((buf[1] << 8) | buf[0]) >> 4) / 1000.0f;
    y = ((int16_t)((buf[3] << 8) | buf[2]) >> 4) / 1000.0f;
    z = ((int16_t)((buf[5] << 8) | buf[4]) >> 4) / 1000.0f;
    return true;
}

}  // namespace

void InitializeSensorTool(i2c_master_bus_handle_t i2c_bus) {
    i2c_device_config_t cfg = {};
    cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    cfg.scl_speed_hz = 100000;

    cfg.device_address = 0x38;  // AHT20
    i2c_master_bus_add_device(i2c_bus, &cfg, &aht20_dev);

    cfg.device_address = 0x29;  // LTR303-ALS
    i2c_master_bus_add_device(i2c_bus, &cfg, &ltr303_dev);
    if (ltr303_dev) {
        uint8_t init[] = {0x80, 0x01};  // ALS_CONTROL: activate
        I2cWrite(ltr303_dev, init, sizeof(init));
    }

    cfg.device_address = 0x19;  // SC7A20H
    i2c_master_bus_add_device(i2c_bus, &cfg, &sc7a20_dev);
    if (sc7a20_dev) {
        uint8_t init[] = {0x20, 0x57};  // CTRL_REG1: enable all axes, 100 Hz
        I2cWrite(sc7a20_dev, init, sizeof(init));
    }

    auto& mcp = McpServer::GetInstance();
    mcp.AddTool("self.sense",
        "Read the device's onboard environmental sensors.\n"
        "Args:\n"
        "  `sensor`: \"temperature\", \"light\", or \"accel\" — omit to read all.\n"
        "Return:\n"
        "  A JSON object with the requested reading(s), or an error if a sensor read fails.",
        PropertyList({
            Property("sensor", kPropertyTypeString, std::string(""))
        }),
        [](const PropertyList& props) -> ReturnValue {
            std::string sensor = props["sensor"].value<std::string>();

            if (sensor == "temperature" || sensor == "temp") {
                float t = 0, h = 0;
                if (!ReadTemperature(t, h)) return JsonError("Temperature sensor read failed");
                cJSON* json = cJSON_CreateObject();
                cJSON_AddNumberToObject(json, "temperature", t);
                cJSON_AddNumberToObject(json, "humidity", h);
                return ToJsonString(json);
            }

            if (sensor == "light" || sensor == "lux") {
                float lux = 0;
                if (!ReadLight(lux)) return JsonError("Light sensor read failed");
                cJSON* json = cJSON_CreateObject();
                cJSON_AddNumberToObject(json, "lux", lux);
                return ToJsonString(json);
            }

            if (sensor == "accel" || sensor == "accelerometer") {
                float x = 0, y = 0, z = 0;
                if (!ReadAccel(x, y, z)) return JsonError("Accelerometer read failed");
                cJSON* json = cJSON_CreateObject();
                cJSON_AddNumberToObject(json, "accel_x", x);
                cJSON_AddNumberToObject(json, "accel_y", y);
                cJSON_AddNumberToObject(json, "accel_z", z);
                return ToJsonString(json);
            }

            if (!sensor.empty()) {
                return JsonError("Unknown sensor: " + sensor + " — use temperature, light, or accel");
            }

            // No sensor specified: read all, and include whichever succeed.
            float t = 0, h = 0, lux = 0, x = 0, y = 0, z = 0;
            bool temp_ok = ReadTemperature(t, h);
            bool light_ok = ReadLight(lux);
            bool accel_ok = ReadAccel(x, y, z);
            if (!temp_ok && !light_ok && !accel_ok) {
                return JsonError("All sensor reads failed");
            }

            cJSON* json = cJSON_CreateObject();
            if (temp_ok) {
                cJSON_AddNumberToObject(json, "temperature", t);
                cJSON_AddNumberToObject(json, "humidity", h);
            }
            if (light_ok) cJSON_AddNumberToObject(json, "lux", lux);
            if (accel_ok) {
                cJSON_AddNumberToObject(json, "accel_x", x);
                cJSON_AddNumberToObject(json, "accel_y", y);
                cJSON_AddNumberToObject(json, "accel_z", z);
            }
            return ToJsonString(json);
        });

    ESP_LOGI(TAG, "self.sense registered");
}
