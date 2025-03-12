#include "iot/thing.h"
#include "board.h"
#include "settings.h"

#include <esp_log.h>

#define TAG "Environment"

namespace iot {

// 这里仅定义 Environment 的属性和方法，不包含具体的实现
class Environment : public Thing {
 private:
    float temperature_ = 0;
    float temperature_diff_ = -0.0;
    float humidity_ = 0;
    float humidity_diff_ = 0.0;
    float light_ = 0;
    float light_diff_ = 0;

    void InitializeDiff() {
        Settings settings("environment", true);
        temperature_diff_ = settings.GetInt("temp_diff", 0) / 10;
        humidity_diff_ = settings.GetInt("humi_diff", 0) / 10;

        ESP_LOGI(TAG, "Get stored temperature_diff is %.1f", temperature_diff_);
        ESP_LOGI(TAG, "Get stored humidity_diff is %.1f", humidity_diff_);
    }

 public:
    Environment() : Thing("Environment", "当前环境信息") {
        // 获取diff初始值
        InitializeDiff();

        // 定义设备的属性
        properties_.AddNumberProperty("temperature", "当前环境温度", [this]() -> float {
            auto& board = Board::GetInstance();
            if (board.GetTemperature(&temperature_)) {
                ESP_LOGI(TAG, "Original Temperature value is %.1f, "
                                "diff is %.1f, return is %.1f",
                                temperature_, temperature_diff_,
                                temperature_ + temperature_diff_);
                return temperature_ + temperature_diff_;
            }
            return 0;
        });

        properties_.AddNumberProperty("humidity", "当前环境湿度", [this]() -> float {
            auto& board = Board::GetInstance();
            if (board.GetHumidity(&humidity_)) {
                if (humidity_ + humidity_diff_ >= 0) {
                    ESP_LOGI(TAG, "Original Humidity value is %.1f, "
                        "diff is %.1f, return is %.1f",
                        humidity_, humidity_diff_,
                        humidity_ + humidity_diff_);
                    return humidity_ + humidity_diff_;
                }
            }
            return 0;
        });

        properties_.AddNumberProperty("temperature_diff", "温度偏差", [this]() -> float {
            return temperature_diff_;
        });

        properties_.AddNumberProperty("humidity_diff", "湿度偏差", [this]() -> float {
            return humidity_diff_;
        });

        properties_.AddNumberProperty("light", "当前环境光照强度", [this]() -> float {
            auto& board = Board::GetInstance();
            if (board.GetLight(&light_)) {
                return light_;
            }
            return 0;
        });

        // 定义设备可以被远程执行的指令
        methods_.AddMethod("SetTemperatureDiff", "设置温度偏差", ParameterList({
            Parameter("temperature_diff", "-50到50之间的整数或者带有1位小数的数", kValueTypeNumber, true)
        }), [this](const ParameterList& parameters) {
            float tmp = parameters["temperature_diff"].number();
            if (tmp >= -50 && tmp <= 50) {
                temperature_diff_ = parameters["temperature_diff"].number();

                Settings settings("environment", true);
                settings.SetInt("temp_diff", static_cast<int>(temperature_diff_*10));
                ESP_LOGI(TAG, "Set Temperature diff to %.1f°C", temperature_diff_);
            } else {
                ESP_LOGE(TAG, "Temperature diff value %.1f°C is invalid", temperature_diff_);
            }
        });

        methods_.AddMethod("SetHumidityDiff", "设置湿度偏差", ParameterList({
            Parameter("humidity_diff", "-50到50之间的整数或者带有1位小数的数", kValueTypeNumber, true)
        }), [this](const ParameterList& parameters) {
            float tmp = parameters["humidity_diff"].number();
            if (tmp >= -50 && tmp <= 50) {
                humidity_diff_ = parameters["humidity_diff"].number();

                Settings settings("environment", true);
                settings.SetInt("humi_diff", static_cast<int>(humidity_diff_*10));
                ESP_LOGI(TAG, "Set Humidity diff to %.1f%%", humidity_diff_);
            } else {
                ESP_LOGE(TAG, "Humidity_diff value %.1f%% is invalid", humidity_diff_);
            }
        });
    }
};

}  // namespace iot

DECLARE_THING(Environment);
