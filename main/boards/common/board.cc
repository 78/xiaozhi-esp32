#include "board.h"
#include "system_info.h"
#include "settings.h"
#include "display/display.h"
#include "display/oled_display.h"
#include "assets/lang_config.h"
#include "boards/common/esp32_music.h"

#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_chip_info.h>
#include <esp_random.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>

#define TAG "Board"

Board::Board()
{
    Settings settings("board", true);
    uuid_ = settings.GetString("uuid");
    if (uuid_.empty())
    {
        uuid_ = GenerateUuid();
        settings.SetString("uuid", uuid_);
    }
    ESP_LOGI(TAG, "UUID=%s SKU=%s", uuid_.c_str(), BOARD_NAME);
    InitializeMusic();
}

void Board::InitializeMusic()
{
    ESP_LOGI(TAG, "Initialize Music");
    music_ = new Esp32Music();
}

std::string Board::GenerateUuid()
{
    // UUID v4 需要 16 字节的随机数据
    uint8_t uuid[16];

    // 使用 ESP32 的硬件随机数生成器
    esp_fill_random(uuid, sizeof(uuid));

    // 设置版本 (版本 4) 和变体位
    uuid[6] = (uuid[6] & 0x0F) | 0x40; // 版本 4
    uuid[8] = (uuid[8] & 0x3F) | 0x80; // 变体 1

    // 将字节转换为标准的 UUID 字符串格式
    char uuid_str[37];
    snprintf(uuid_str, sizeof(uuid_str),
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             uuid[0], uuid[1], uuid[2], uuid[3],
             uuid[4], uuid[5], uuid[6], uuid[7],
             uuid[8], uuid[9], uuid[10], uuid[11],
             uuid[12], uuid[13], uuid[14], uuid[15]);

    return std::string(uuid_str);
}

bool Board::GetBatteryLevel(int &level, bool &charging, bool &discharging)
{
#ifdef CONFIG_BATTERY_ADC_GPIO
    static adc_oneshot_unit_handle_t adc_handle = nullptr;
    static adc_cali_handle_t adc_cali_handle = nullptr;
    static bool initialized = false;

    if (!initialized)
    {
        adc_oneshot_unit_init_cfg_t init_config = {
            .unit_id = ADC_UNIT_1,
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

        adc_oneshot_chan_cfg_t config = {
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };

        adc_channel_t channel;
        adc_unit_t unit_id = ADC_UNIT_1;
        if (adc_oneshot_io_to_channel(CONFIG_BATTERY_ADC_GPIO, &unit_id, &channel) == ESP_OK)
        {
            ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, channel, &config));

#if CONFIG_IDF_TARGET_ESP32
            adc_cali_line_fitting_config_t cali_config = {
                .unit_id = unit_id,
                .atten = ADC_ATTEN_DB_12,
                .bitwidth = ADC_BITWIDTH_DEFAULT,
            };
            adc_cali_create_scheme_line_fitting(&cali_config, &adc_cali_handle);
#else
            adc_cali_curve_fitting_config_t cali_config = {
                .unit_id = unit_id,
                .atten = ADC_ATTEN_DB_12,
                .bitwidth = ADC_BITWIDTH_DEFAULT,
            };
            adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle);
#endif
            initialized = true;
        }
        else
        {
            ESP_LOGE(TAG, "ADC channel not found for GPIO %d", CONFIG_BATTERY_ADC_GPIO);
            return false;
        }
    }

    if (!initialized)
        return false;

    int raw = 0;
    adc_channel_t channel;
    adc_unit_t unit_id = ADC_UNIT_1;
    adc_oneshot_io_to_channel(CONFIG_BATTERY_ADC_GPIO, &unit_id, &channel);

    if (adc_oneshot_read(adc_handle, channel, &raw) == ESP_OK)
    {
        int voltage = 0;
        if (adc_cali_handle)
        {
            adc_cali_raw_to_voltage(adc_cali_handle, raw, &voltage);
        }
        else
        {
            voltage = raw * 2500 / 4095;
        }

        float divider_factor = atof(CONFIG_BATTERY_VOLTAGE_DIVIDER_FACTOR);
        int battery_voltage = voltage * divider_factor;

        int max_v = CONFIG_BATTERY_MAX_VOLTAGE;
        int min_v = CONFIG_BATTERY_MIN_VOLTAGE;

        level = (battery_voltage - min_v) * 100 / (max_v - min_v);
        if (level > 100)
            level = 100;
        if (level < 0)
            level = 0;

        charging = false;
        discharging = true;
        return true;
    }
#endif
    return false;
}

bool Board::GetTemperature(float &esp32temp)
{
    return false;
}

Display *Board::GetDisplay()
{
    static NoDisplay display;
    return &display;
}

Camera *Board::GetCamera()
{
    return nullptr;
}

Music *Board::GetMusic()
{
    return music_;
}

Led *Board::GetLed()
{
    static NoLed led;
    return &led;
}

std::string Board::GetSystemInfoJson()
{
    /*
        {
            "version": 2,
            "flash_size": 4194304,
            "psram_size": 0,
            "minimum_free_heap_size": 123456,
            "mac_address": "00:00:00:00:00:00",
            "uuid": "00000000-0000-0000-0000-000000000000",
            "chip_model_name": "esp32s3",
            "chip_info": {
                "model": 1,
                "cores": 2,
                "revision": 0,
                "features": 0
            },
            "application": {
                "name": "my-app",
                "version": "1.0.0",
                "compile_time": "2021-01-01T00:00:00Z"
                "idf_version": "4.2-dev"
                "elf_sha256": ""
            },
            "partition_table": [
                "app": {
                    "label": "app",
                    "type": 1,
                    "subtype": 2,
                    "address": 0x10000,
                    "size": 0x100000
                }
            ],
            "ota": {
                "label": "ota_0"
            },
            "board": {
                ...
            }
        }
    */
    std::string json = R"({"version":2,"language":")" + std::string(Lang::CODE) + R"(",)";
    json += R"("flash_size":)" + std::to_string(SystemInfo::GetFlashSize()) + R"(,)";
    json += R"("minimum_free_heap_size":")" + std::to_string(SystemInfo::GetMinimumFreeHeapSize()) + R"(",)";
    json += R"("mac_address":")" + SystemInfo::GetMacAddress() + R"(",)";
    json += R"("uuid":")" + uuid_ + R"(",)";
    json += R"("chip_model_name":")" + SystemInfo::GetChipModelName() + R"(",)";

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    json += R"("chip_info":{)";
    json += R"("model":)" + std::to_string(chip_info.model) + R"(,)";
    json += R"("cores":)" + std::to_string(chip_info.cores) + R"(,)";
    json += R"("revision":)" + std::to_string(chip_info.revision) + R"(,)";
    json += R"("features":)" + std::to_string(chip_info.features) + R"(},)";

    auto app_desc = esp_app_get_description();
    json += R"("application":{)";
    json += R"("name":")" + std::string(app_desc->project_name) + R"(",)";
    json += R"("version":")" + std::string(app_desc->version) + R"(",)";
    json += R"("compile_time":")" + std::string(app_desc->date) + R"(T)" + std::string(app_desc->time) + R"(Z",)";
    json += R"("idf_version":")" + std::string(app_desc->idf_ver) + R"(",)";
    char sha256_str[65];
    for (int i = 0; i < 32; i++)
    {
        snprintf(sha256_str + i * 2, sizeof(sha256_str) - i * 2, "%02x", app_desc->app_elf_sha256[i]);
    }
    json += R"("elf_sha256":")" + std::string(sha256_str) + R"(")";
    json += R"(},)";

    json += R"("partition_table": [)";
    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
    while (it)
    {
        const esp_partition_t *partition = esp_partition_get(it);
        json += R"({)";
        json += R"("label":")" + std::string(partition->label) + R"(",)";
        json += R"("type":)" + std::to_string(partition->type) + R"(,)";
        json += R"("subtype":)" + std::to_string(partition->subtype) + R"(,)";
        json += R"("address":)" + std::to_string(partition->address) + R"(,)";
        json += R"("size":)" + std::to_string(partition->size) + R"(},)";
        ;
        it = esp_partition_next(it);
    }
    json.pop_back(); // Remove the last comma
    json += R"(],)";

    json += R"("ota":{)";
    auto ota_partition = esp_ota_get_running_partition();
    json += R"("label":")" + std::string(ota_partition->label) + R"(")";
    json += R"(},)";

    // Append display info
    auto display = GetDisplay();
    if (display)
    {
        json += R"("display":{)";
        if (dynamic_cast<OledDisplay *>(display))
        {
            json += R"("monochrome":)" + std::string("true") + R"(,)";
        }
        else
        {
            json += R"("monochrome":)" + std::string("false") + R"(,)";
        }
        json += R"("width":)" + std::to_string(display->width()) + R"(,)";
        json += R"("height":)" + std::to_string(display->height()) + R"(,)";
        json.pop_back(); // Remove the last comma
    }
    json += R"(},)";

    json += R"("board":)" + GetBoardJson();

    // Close the JSON object
    json += R"(})";
    return json;
}
