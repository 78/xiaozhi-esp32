
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include "encoder.h"

#include <esp_log.h>

static const char *TAG = "Encoder";

Encoder::Encoder(gpio_num_t gpio_pcnt1, gpio_num_t gpio_pcnt2, int _low_limit, int _high_limit) : gpio_pcnt1_(gpio_pcnt1), gpio_pcnt2_(gpio_pcnt2)
{
    if (gpio_pcnt1 == -1 || gpio_pcnt2 == -1)
    {
        return;
    }
    ESP_LOGI(TAG, "install pcnt unit");
    pcnt_unit_config_t unit_config = {
        .low_limit = _low_limit,
        .high_limit = _high_limit,
    };
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &pcnt_unit_));

    ESP_LOGI(TAG, "set glitch filter");
    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 1000,
    };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit_, &filter_config));

    ESP_LOGI(TAG, "install pcnt channels");
    pcnt_chan_config_t chan_a_config = {
        .edge_gpio_num = gpio_pcnt1,
        .level_gpio_num = gpio_pcnt2,
    };
    pcnt_channel_handle_t pcnt_chan_a = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit_, &chan_a_config, &pcnt_chan_a));
    pcnt_chan_config_t chan_b_config = {
        .edge_gpio_num = gpio_pcnt2,
        .level_gpio_num = gpio_pcnt1,
    };
    pcnt_channel_handle_t pcnt_chan_b = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit_, &chan_b_config, &pcnt_chan_b));

    ESP_LOGI(TAG, "set edge and level actions for pcnt channels");
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

    ESP_LOGI(TAG, "add watch points and register callbacks");
    ESP_ERROR_CHECK(pcnt_unit_add_watch_point(pcnt_unit_, _low_limit));
    ESP_ERROR_CHECK(pcnt_unit_add_watch_point(pcnt_unit_, _high_limit));
}

Encoder::~Encoder()
{
    if (pcnt_unit_ != NULL)
    {
        ESP_ERROR_CHECK(pcnt_del_unit(pcnt_unit_));
        pcnt_unit_ = NULL;
    }
}

void Encoder::OnPcntReach(std::function<void(int)> callback)
{
    if (pcnt_unit_ == nullptr)
    {
        return;
    }

    on_pcnt_reach_ = callback;
    pcnt_event_callbacks_t cbs = {
        .on_reach = [](pcnt_unit_handle_t unit, const pcnt_watch_event_data_t *edata, void *user_ctx) -> bool
        {
            Encoder *encoder = static_cast<Encoder *>(user_ctx);
            if (encoder->on_pcnt_reach_)
            {
                encoder->on_pcnt_reach_(edata->watch_point_value);
            }
            return true;
        }};
    ESP_ERROR_CHECK(pcnt_unit_register_event_callbacks(pcnt_unit_, &cbs, this));

    ESP_LOGI(TAG, "enable pcnt unit");
    ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit_));
    ESP_LOGI(TAG, "clear pcnt unit");
    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit_));
    ESP_LOGI(TAG, "start pcnt unit");
    ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit_));
    // ESP_LOGI(TAG, "set pcnt unit: %d", 50);
    // ESP_ERROR_CHECK(pcnt_unit_set_count(pcnt_unit_, 50));

    xTaskCreate(
        [](void *arg)
        {
            Encoder *encoder = static_cast<Encoder *>(arg);
            static int lastvalue = 0;
            int pulse_count = 0;
            while(encoder->pcnt_unit_ != NULL)
            {
                ESP_ERROR_CHECK(pcnt_unit_get_count(encoder->pcnt_unit_, &pulse_count));
                if(lastvalue!=pulse_count)
                {   
                    lastvalue=pulse_count;
                    // ESP_LOGI(TAG, "Pulse count: %d", pulse_count);
                    if (encoder->on_pcnt_reach_)
                    {
                        encoder->on_pcnt_reach_(pulse_count);
                    }
                }
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        vTaskDelete(NULL); }, "encoder", 4096, this, 3, nullptr);
}
