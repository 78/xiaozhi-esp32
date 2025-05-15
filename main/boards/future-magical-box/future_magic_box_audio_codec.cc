#include <esp_log.h>
#include "future_magic_box_audio_codec.h"

static const char *TAG = "MagicBoxAudioCodec";

MagicBoxAudioCodec::MagicBoxAudioCodec(
    void *i2c_master_handle, esp_io_expander_handle_t io_expander, int input_sample_rate, int output_sample_rate,
    gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din,
    int pa_pin, uint8_t es8311_addr, uint8_t es7210_addr, bool input_reference)
    : BoxAudioCodec(
          i2c_master_handle, input_sample_rate, output_sample_rate,
          mclk, bclk, ws, dout, din,
          // 动态决定传递给基类的PA引脚
          IsExpansionGPIO(pa_pin) ? GPIO_NUM_NC : static_cast<gpio_num_t>(pa_pin),
          es8311_addr, es7210_addr, input_reference)
{
    // 判断并存储扩展GPIO
    if ((IsExpansionGPIO(pa_pin)) && (io_expander))
    {
        is_exp_gpio_ = true;
        io_expander_ = io_expander;
        pa_pin_ = pa_pin - EXP_GPIO_START_NUM;
        esp_io_expander_set_dir(io_expander_, pa_pin_, IO_EXPANDER_OUTPUT);
        ESP_LOGI(TAG, "PA using expansion GPIO: %d", pa_pin_);
    }
    else
    {
        is_exp_gpio_ = false;
        ESP_LOGI(TAG, "PA using native GPIO: %d", pa_pin);
    }
}

MagicBoxAudioCodec::~MagicBoxAudioCodec() = default; // 使用编译器生成的默认析构函数

void MagicBoxAudioCodec::EnableOutput(bool enable)
{
    if (enable == output_enabled_)
    {
        return;
    }

    if ((io_expander_) && (is_exp_gpio_))
    {
        // 扩展GPIO操作
        esp_io_expander_set_level(io_expander_, pa_pin_, enable ? 1 : 0);
    }

    BoxAudioCodec::EnableOutput(enable);
}

bool MagicBoxAudioCodec::IsExpansionGPIO(int pin)
{
    return (pin >= EXP_GPIO_START_NUM); // 扩展GPIO编号范围
}
